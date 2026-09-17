#include "oracool/sprite_mix.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "engine/assets.hpp"
#include "engine/load_cl2.hpp"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "itemdat.h"
#include "items.h"
#include "oracool/hero_look.h"
#include "oracool/sprite_colours.h"
#include "playerdat.hpp"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/surface_to_clx.hpp"

namespace devilution::oracool {

namespace {

constexpr int Facings = 8;
constexpr int16_t Clear = -1;    // no pixel
constexpr int16_t ShadowIndex = 0; // the foot shadow: a real pixel, but never part of a piece
constexpr size_t HeavyArmour = 2;  // ArmourChar index

/** One frame in palette-index space, Clear where the sprite drew nothing. */
using Frame = std::vector<int16_t>;
using Mask = std::vector<uint8_t>;

struct Geometry {
	int width = 0;
	int height = 0;
	int frames = 0;
	[[nodiscard]] size_t pixels() const { return static_cast<size_t>(width) * static_cast<size_t>(height); }
	bool operator==(const Geometry &o) const { return width == o.width && height == o.height && frames == o.frames; }
};

/** A loaded sheet plus the two scratch surfaces its frames are rendered through. */
class Source {
public:
	Source() = default;
	Source(OwnedClxSpriteSheet &&sheet, std::string name)
	    : sheet_(std::move(sheet))
	    , name_(std::move(name))
	{
		const ClxSpriteSheet view { *sheet_ };
		if (view.numLists() != Facings)
			return;
		const ClxSpriteList first = view[0];
		geometry_.frames = static_cast<int>(first.numSprites());
		for (size_t dir = 0; dir < Facings; dir++) {
			const ClxSpriteList list = view[dir];
			if (static_cast<int>(list.numSprites()) != geometry_.frames) {
				geometry_ = {};
				return;
			}
			for (const ClxSprite sprite : list) {
				geometry_.width = std::max(geometry_.width, static_cast<int>(sprite.width()));
				geometry_.height = std::max(geometry_.height, static_cast<int>(sprite.height()));
			}
		}
	}

	[[nodiscard]] bool ok() const { return static_cast<bool>(sheet_) && geometry_.frames > 0 && geometry_.width > 0; }
	[[nodiscard]] const Geometry &geometry() const { return geometry_; }
	[[nodiscard]] const std::string &name() const { return name_; }

	/** @brief Frame @p index of facing @p dir, in index space. Two passes: a sprite pixel may BE index 0. */
	void Render(int dir, int index, OwnedSurface &colour, OwnedSurface &mask, Frame &out) const
	{
		// Into the CANVAS's cell, not this sheet's own: sprites stand on the bottom edge, so a sheet cut
		// shorter (the heavy staff's attack is 96 tall beside the sword's 128) lines up by its feet.
		static const std::array<uint8_t, 256> Opaque = [] {
			std::array<uint8_t, 256> table;
			table.fill(1);
			return table;
		}();
		const int w = colour.w();
		const int h = colour.h();
		for (int y = 0; y < h; y++) {
			std::memset(&colour[Point { 0, y }], 0, static_cast<size_t>(w));
			std::memset(&mask[Point { 0, y }], 0, static_cast<size_t>(w));
		}
		const ClxSprite sprite = ClxSpriteSheet { *sheet_ }[static_cast<size_t>(dir)][static_cast<size_t>(index)];
		ClxDraw(colour, { 0, h - 1 }, sprite);
		ClxDrawTRN(mask, { 0, h - 1 }, sprite, Opaque.data());
		out.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
		for (int y = 0; y < h; y++) {
			const uint8_t *colourRow = &colour[Point { 0, y }];
			const uint8_t *maskRow = &mask[Point { 0, y }];
			for (int x = 0; x < w; x++)
				out[static_cast<size_t>(y * w + x)] = maskRow[x] != 0 ? static_cast<int16_t>(colourRow[x]) : Clear;
		}
	}

private:
	OptionalOwnedClxSpriteSheet sheet_;
	std::string name_;
	Geometry geometry_;
};

using SourcePtr = std::shared_ptr<const Source>;
/** @brief Hands over the sheet of (armour tier, weapon class) for the animation in hand, or null. */
using SourceLoader = std::function<SourcePtr(size_t, PlayerWeaponGraphic)>;

/**
 * The voters of the sword family. The attack has fewer twins - the axe swings on its own frame count, the heavy
 * mace on another - so the mace WITH a shield votes too: its mace and its shield are each alone in the vote and
 * lose it.
 */
constexpr std::array<PlayerWeaponGraphic, 5> SwordVoters = { PlayerWeaponGraphic::Sword, PlayerWeaponGraphic::Mace,
	PlayerWeaponGraphic::Axe, PlayerWeaponGraphic::Staff, PlayerWeaponGraphic::MaceShield };

SourcePtr LoadSource(const char *classPath, char classChar, size_t armour, PlayerWeaponGraphic weapon, const char *szCel, uint16_t frameWidth)
{
	const char prefix[3] = { classChar, ArmourChar[armour], WepChar[static_cast<size_t>(weapon)] };
	const std::string path = fmt::format(R"(plrgfx\{0}\{1}\{1}{2})", classPath, string_view(prefix, 3), szCel);
	if (!FindAsset((path + DEVILUTIONX_CL2_EXT).c_str()).ok())
		return nullptr;
	return std::make_shared<const Source>(LoadCl2Sheet(path.c_str(), frameWidth), path);
}

// ---------------------------------------------------------------------------------------------------
// Masks
// ---------------------------------------------------------------------------------------------------

/** @brief The rows and columns @p mask has anything in. A shield or a blade is a twentieth of its frame. */
struct Box {
	int x0, y0, x1, y1; // inclusive; x1 < x0 when empty
	[[nodiscard]] bool empty() const { return x1 < x0; }
};

Box BoundsOf(const Mask &mask, int w, int h)
{
	Box box { w, h, -1, -1 };
	for (int y = 0; y < h; y++) {
		const uint8_t *row = &mask[static_cast<size_t>(y * w)];
		for (int x = 0; x < w; x++) {
			if (row[x] == 0)
				continue;
			box.x0 = std::min(box.x0, x);
			box.x1 = std::max(box.x1, x);
			box.y0 = std::min(box.y0, y);
			box.y1 = y;
		}
	}
	return box;
}

Mask Dilate(const Mask &mask, int w, int h, int radius)
{
	Mask current = mask;
	Box box = BoundsOf(mask, w, h);
	if (box.empty())
		return current;
	for (int step = 0; step < radius; step++) {
		Mask next = current;
		box = { std::max(0, box.x0 - 1), std::max(0, box.y0 - 1), std::min(w - 1, box.x1 + 1), std::min(h - 1, box.y1 + 1) };
		for (int y = box.y0; y <= box.y1; y++) {
			for (int x = box.x0; x <= box.x1; x++) {
				if (current[static_cast<size_t>(y * w + x)] != 0)
					continue;
				for (int dy = -1; dy <= 1 && next[static_cast<size_t>(y * w + x)] == 0; dy++) {
					for (int dx = -1; dx <= 1; dx++) {
						const int nx = x + dx;
						const int ny = y + dy;
						if (nx >= 0 && ny >= 0 && nx < w && ny < h && current[static_cast<size_t>(ny * w + nx)] != 0) {
							next[static_cast<size_t>(y * w + x)] = 1;
							break;
						}
					}
				}
			}
		}
		current = std::move(next);
	}
	return current;
}

Mask Erode(const Mask &mask, int w, int h)
{
	Mask out(mask.size(), 0);
	const Box box = BoundsOf(mask, w, h);
	if (box.empty())
		return out;
	for (int y = std::max(1, box.y0); y <= std::min(h - 2, box.y1); y++) {
		for (int x = std::max(1, box.x0); x <= std::min(w - 2, box.x1); x++) {
			bool all = true;
			for (int dy = -1; dy <= 1 && all; dy++)
				for (int dx = -1; dx <= 1; dx++)
					if (mask[static_cast<size_t>((y + dy) * w + x + dx)] == 0) {
						all = false;
						break;
					}
			out[static_cast<size_t>(y * w + x)] = all ? 1 : 0;
		}
	}
	return out;
}

/** @brief 8-connected components, each a list of pixel offsets. */
std::vector<std::vector<int>> Components(const Mask &mask, int w, int h)
{
	std::vector<std::vector<int>> out;
	std::vector<uint8_t> seen(mask.size(), 0);
	std::vector<int> stack;
	for (int start = 0; start < static_cast<int>(mask.size()); start++) {
		if (mask[static_cast<size_t>(start)] == 0 || seen[static_cast<size_t>(start)] != 0)
			continue;
		std::vector<int> points;
		stack.assign(1, start);
		seen[static_cast<size_t>(start)] = 1;
		while (!stack.empty()) {
			const int p = stack.back();
			stack.pop_back();
			points.push_back(p);
			const int x = p % w;
			const int y = p / w;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					const int nx = x + dx;
					const int ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= w || ny >= h)
						continue;
					const int q = ny * w + nx;
					if (mask[static_cast<size_t>(q)] != 0 && seen[static_cast<size_t>(q)] == 0) {
						seen[static_cast<size_t>(q)] = 1;
						stack.push_back(q);
					}
				}
			}
		}
		out.push_back(std::move(points));
	}
	return out;
}

int Count(const Mask &mask)
{
	return static_cast<int>(std::count_if(mask.begin(), mask.end(), [](uint8_t v) { return v != 0; }));
}

// ---------------------------------------------------------------------------------------------------
// The pieces
// ---------------------------------------------------------------------------------------------------

/** @brief Where @p with shows a solid pixel that @p without does not: the piece the two sheets differ by. */
Mask SolidDifference(const Frame &without, const Frame &with)
{
	Mask out(with.size(), 0);
	for (size_t i = 0; i < with.size(); i++)
		out[i] = (with[i] != Clear && with[i] != ShadowIndex && with[i] != without[i]) ? 1 : 0;
	return out;
}

/**
 * @brief The body's own shield. Found on an ERODED difference: the sword differs by a pixel here and there
 * between the two sheets, that speckle runs the length of the blade, and a two-pixel blade does not
 * survive an erosion where a shield does.
 */
Mask OwnShieldMask(const Frame &without, const Frame &with, int w, int h)
{
	const Mask solid = SolidDifference(without, with);
	std::vector<std::vector<int>> cores = Components(Erode(solid, w, h), w, h);
	if (cores.empty())
		return Mask(solid.size(), 0);
	const auto largest = std::max_element(cores.begin(), cores.end(), [](const auto &a, const auto &b) { return a.size() < b.size(); });
	Mask core(solid.size(), 0);
	for (const int p : *largest)
		core[static_cast<size_t>(p)] = 1;
	const Mask grown = Dilate(core, w, h, 3);
	Mask out(solid.size(), 0);
	for (size_t i = 0; i < out.size(); i++)
		out[i] = solid[i] != 0 && grown[i] != 0 ? 1 : 0;
	return out;
}

/**
 * @brief The shield to bring in, from the other tier's pair of sheets, found BY the shield it replaces.
 *
 * The other tier's difference also holds its sword (held at another angle, so it differs) and that sword
 * reaches the shield through the armoured arm - so it is severed by DISTANCE first and only then asked
 * about connectivity. The shield's cast shadow comes with it.
 */
Mask ForeignShieldMask(const Frame &without, const Frame &with, const Mask &ownShield, int w, int h)
{
	const Mask solid = SolidDifference(without, with);
	const Mask zone = Dilate(ownShield, w, h, 9);
	const Mask reach = Dilate(zone, w, h, 4);
	Mask cut(solid.size(), 0);
	for (size_t i = 0; i < cut.size(); i++)
		cut[i] = solid[i] != 0 && reach[i] != 0 ? 1 : 0;
	Mask out(solid.size(), 0);
	for (const std::vector<int> &component : Components(cut, w, h)) {
		if (component.size() < 8)
			continue;
		const auto inZone = std::count_if(component.begin(), component.end(), [&](int p) { return zone[static_cast<size_t>(p)] != 0; });
		if (static_cast<size_t>(inZone) * 10 < component.size() * 6)
			continue;
		for (const int p : component)
			out[static_cast<size_t>(p)] = 1;
	}
	const Mask beside = Dilate(out, w, h, 2);
	for (size_t i = 0; i < out.size(); i++) {
		if (with[i] == ShadowIndex && with[i] != without[i] && beside[i] != 0 && zone[i] != 0)
			out[i] = 1;
	}
	return out;
}

/** @brief What at least two of @p voters agree on, else Clear: the body with nothing in its hands. */
void Vote(const std::vector<Frame> &voters, Frame &out)
{
	const size_t pixels = voters[0].size();
	out.assign(pixels, Clear);
	for (size_t i = 0; i < pixels; i++) {
		for (size_t a = 0; a + 1 < voters.size(); a++) {
			int agree = 1;
			for (size_t b = a + 1; b < voters.size(); b++)
				if (voters[b][i] == voters[a][i])
					agree++;
			if (agree >= 2) {
				out[i] = voters[a][i];
				break;
			}
		}
	}
}

/** @brief What the sword sheet shows that the empty-handed body does not - sword, scabbard, stray dither. */
Mask SwordDifference(const Frame &sword, const Frame &body)
{
	Mask out(sword.size(), 0);
	for (size_t i = 0; i < sword.size(); i++)
		out[i] = (sword[i] != Clear && sword[i] != ShadowIndex && sword[i] != body[i]) ? 1 : 0;
	return out;
}

/**
 * @brief The blade out of a sword difference: the component that reaches furthest OUTSIDE the body. A
 * scabbard lies across the back and a blade sticks out of the silhouette, which is the one thing that
 * tells them apart when the scabbard is the larger of the two.
 */
Mask BladeMask(const Mask &difference, const Frame &body, int w, int h)
{
	Mask out(difference.size(), 0);
	const std::vector<std::vector<int>> components = Components(difference, w, h);
	const std::vector<int> *best = nullptr;
	int bestOutside = 0;
	for (const std::vector<int> &component : components) {
		if (component.size() < 10)
			continue;
		const int outside = static_cast<int>(std::count_if(component.begin(), component.end(), [&](int p) { return body[static_cast<size_t>(p)] == Clear; }));
		if (best == nullptr || outside > bestOutside) {
			best = &component;
			bestOutside = outside;
		}
	}
	if (best == nullptr)
		return out;
	for (const int p : *best)
		out[static_cast<size_t>(p)] = 1;
	// The hilt and guard are sometimes a separate few pixels beside it.
	const Mask beside = Dilate(out, w, h, 2);
	for (const std::vector<int> &component : components) {
		if (&component == best || component.size() < 3)
			continue;
		if (std::any_of(component.begin(), component.end(), [&](int p) { return beside[static_cast<size_t>(p)] != 0; }))
			for (const int p : component)
				out[static_cast<size_t>(p)] = 1;
	}
	return out;
}

/** @brief Fills a hole the vote left inside the body from what surrounds it. One pass; an edge stays open. */
void FillSmallHoles(Frame &canvas, const Mask &where, int w, int h)
{
	const Frame before = canvas;
	for (int y = 1; y < h - 1; y++) {
		for (int x = 1; x < w - 1; x++) {
			const size_t i = static_cast<size_t>(y * w + x);
			if (where[i] == 0 || before[i] != Clear)
				continue;
			std::array<int16_t, 8> around {};
			int n = 0;
			for (int dy = -1; dy <= 1; dy++)
				for (int dx = -1; dx <= 1; dx++) {
					if (dx == 0 && dy == 0)
						continue;
					const int16_t v = before[static_cast<size_t>((y + dy) * w + x + dx)];
					if (v != Clear && v != ShadowIndex)
						around[static_cast<size_t>(n++)] = v;
				}
			if (n < 5)
				continue;
			int16_t best = around[0];
			int bestCount = 0;
			for (int a = 0; a < n; a++) {
				const int c = static_cast<int>(std::count(around.begin(), around.begin() + n, around[static_cast<size_t>(a)]));
				if (c > bestCount) {
					bestCount = c;
					best = around[static_cast<size_t>(a)];
				}
			}
			canvas[i] = best;
		}
	}
}

/**
 * @brief Whether two sheets are THE SAME BODY RENDER - the premise of everything here. Measured on the first and
 * the middle frame of every facing: of the pixels both draw, how many are identical. Twins score 65-78%; two
 * different poses score 30-50% (the spell casts with and without a shield are such a pair, and subtracting them
 * brought the whole heavy body across - seen in the first contact sheet, 2026-09-17).
 */
bool AreTwins(const Source &a, const Source &b, OwnedSurface &colour, OwnedSurface &mask)
{
	constexpr int TwinPercent = 58;
	// The answer is a fact about two files in the archive and never changes, so it is worked out once a session
	// (and the finished sheets are cached on disk, so in practice once ever).
	static std::mutex memoMutex;
	static std::unordered_map<std::string, bool> memo;
	const std::string memoKey = a.name() + "|" + b.name();
	{
		const std::lock_guard<std::mutex> lock(memoMutex);
		if (const auto it = memo.find(memoKey); it != memo.end())
			return it->second;
	}
	Frame fa, fb;
	int64_t same = 0;
	int64_t both = 0;
	const int frames = a.geometry().frames;
	for (int dir = 0; dir < Facings; dir++) {
		for (const int index : { 0, frames / 2 }) {
			a.Render(dir, index, colour, mask, fa);
			b.Render(dir, index, colour, mask, fb);
			for (size_t i = 0; i < fa.size(); i++) {
				if (fa[i] == Clear || fb[i] == Clear || fa[i] == ShadowIndex || fb[i] == ShadowIndex)
					continue;
				both++;
				if (fa[i] == fb[i])
					same++;
			}
		}
	}
	const bool twins = both > 0 && same * 100 >= both * TwinPercent;
	const std::lock_guard<std::mutex> lock(memoMutex);
	memo[memoKey] = twins;
	return twins;
}

bool HasShield(PlayerWeaponGraphic weapon)
{
	return IsAnyOf(weapon, PlayerWeaponGraphic::SwordShield, PlayerWeaponGraphic::MaceShield, PlayerWeaponGraphic::UnarmedShield);
}

PlayerWeaponGraphic WithoutShield(PlayerWeaponGraphic weapon)
{
	switch (weapon) {
	case PlayerWeaponGraphic::SwordShield: return PlayerWeaponGraphic::Sword;
	case PlayerWeaponGraphic::MaceShield: return PlayerWeaponGraphic::Mace;
	case PlayerWeaponGraphic::UnarmedShield: return PlayerWeaponGraphic::Unarmed;
	default: return weapon;
	}
}

item_cursor_graphic BaseCursor(const Item &item)
{
	return AllItemsList[static_cast<size_t>(item.IDidx)].iCurs;
}

} // namespace

GearLook GearLookFor(const Player &player)
{
	GearLook look;
	for (const inv_body_loc hand : { INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT }) {
		const Item &item = player.InvBody[hand];
		if (item.isEmpty() || !item._iStatFlag)
			continue;
		const item_cursor_graphic cursor = BaseCursor(item);
		if (item._itype == ItemType::Shield && IsAnyOf(cursor, ICURS_KITE_SHIELD, ICURS_TOWER_SHIELD, ICURS_GOTHIC_SHIELD))
			look.shield = LookTier::Heavy;
		if (item._itype == ItemType::Sword
		    && IsAnyOf(cursor, ICURS_LONG_SWORD, ICURS_BROAD_SWORD, ICURS_BASTARD_SWORD, ICURS_TWO_HANDED_SWORD, ICURS_GREAT_SWORD))
			look.sword = LookTier::Heavy;
	}
	return look;
}

uint8_t GearLookCode(const Player &player)
{
	const GearLook look = GearLookFor(player);
	return static_cast<uint8_t>((look.shield == LookTier::Heavy ? 2 : 0) | (look.sword == LookTier::Heavy ? 1 : 0));
}

namespace {

std::optional<ColouredSpriteSheet> ComposeMixedSheet(const PlayerSheetRequest &request, const SourceLoader &load)
{
	if (!WantsMixedSheet(request))
		return std::nullopt;
	const size_t armour = request.armour;
	const PlayerWeaponGraphic weapon = request.weapon;
	const std::shared_ptr<const SpriteColours> &dye = request.dye;
	const PlayerWeaponGraphic bare = WithoutShield(weapon);
	const bool swapShield = request.look.shield == LookTier::Heavy && HasShield(weapon);
	const bool swapSword = request.look.sword == LookTier::Heavy && bare == PlayerWeaponGraphic::Sword;

	// The canvas is this tier's sheet WITHOUT a shield: a whole body, so taking the shield away leaves no
	// hole to patch. The block animation has no such twin - nothing to subtract from - and is left alone.
	const SourcePtr ownBare = load(armour, bare);
	if (ownBare == nullptr || !ownBare->ok())
		return std::nullopt;
	const Geometry geometry = ownBare->geometry();
	// Same frames, same width, and no TALLER than the canvas - a shorter sheet is rendered onto its floor.
	const auto fits = [&](const SourcePtr &source) {
		return source != nullptr && source->ok() && source->geometry().frames == geometry.frames
		    && source->geometry().width == geometry.width && source->geometry().height <= geometry.height;
	};

	OwnedSurface colour(geometry.width, geometry.height);
	OwnedSurface scratchMask(geometry.width, geometry.height);
	const auto twin = [&](const SourcePtr &source) { return fits(source) && AreTwins(*ownBare, *source, colour, scratchMask); };

	const bool shielded = HasShield(weapon);
	SourcePtr ownShielded;
	SourcePtr shieldBare;
	SourcePtr shieldShielded;
	if (shielded) {
		ownShielded = load(armour, weapon);
		if (!twin(ownShielded))
			return std::nullopt; // not the same render with a shield added: nothing to subtract
		if (swapShield) {
			shieldBare = load(HeavyArmour, bare);
			shieldShielded = load(HeavyArmour, weapon);
			if (!fits(shieldBare) || !fits(shieldShielded) || !AreTwins(*shieldBare, *shieldShielded, colour, scratchMask))
				return std::nullopt;
		}
	}

	std::vector<SourcePtr> ownVoters;
	std::vector<SourcePtr> heavyVoters;
	SourcePtr heavySword;
	bool doSword = swapSword;
	if (doSword) {
		heavySword = load(HeavyArmour, PlayerWeaponGraphic::Sword);
		for (const PlayerWeaponGraphic voter : SwordVoters) {
			SourcePtr own = load(armour, voter);
			SourcePtr heavy = load(HeavyArmour, voter);
			// A voter in another pose (the staff attack) agrees with nobody and only adds noise.
			if (twin(own))
				ownVoters.push_back(std::move(own));
			if (fits(heavy) && fits(heavySword) && AreTwins(*heavySword, *heavy, colour, scratchMask))
				heavyVoters.push_back(std::move(heavy));
		}
		// Two voters cannot outvote each other: where they differ there is no majority and no body.
		doSword = ownVoters.size() >= 3 && heavyVoters.size() >= 3 && fits(heavySword);
	}
	if (!doSword && !swapShield)
		return std::nullopt;

	const int w = geometry.width;
	const int h = geometry.height;

	std::vector<Frame> composed(static_cast<size_t>(Facings * geometry.frames));
	std::vector<Mask> foreign(composed.size()); // pixels brought in from the other tier
	std::vector<Frame> voterFrames;
	Frame bareFrame, shieldedFrame, otherBare, otherShielded, body, heavyBody, heavySwordFrame;

	for (int dir = 0; dir < Facings; dir++) {
		for (int index = 0; index < geometry.frames; index++) {
			const size_t slot = static_cast<size_t>(dir * geometry.frames + index);
			Frame &canvas = composed[slot];
			Mask &brought = foreign[slot];
			ownBare->Render(dir, index, colour, scratchMask, bareFrame);
			canvas = bareFrame;
			brought.assign(canvas.size(), 0);
			Mask ownSwordGone(canvas.size(), 0);

			Mask blade;
			if (doSword) {
				voterFrames.resize(ownVoters.size());
				for (size_t v = 0; v < ownVoters.size(); v++)
					ownVoters[v]->Render(dir, index, colour, scratchMask, voterFrames[v]);
				Vote(voterFrames, body);
				voterFrames.resize(heavyVoters.size());
				for (size_t v = 0; v < heavyVoters.size(); v++)
					heavyVoters[v]->Render(dir, index, colour, scratchMask, voterFrames[v]);
				Vote(voterFrames, heavyBody);
				heavySword->Render(dir, index, colour, scratchMask, heavySwordFrame);

				ownSwordGone = SwordDifference(bareFrame, body);
				for (size_t i = 0; i < canvas.size(); i++)
					if (ownSwordGone[i] != 0)
						canvas[i] = body[i];
				FillSmallHoles(canvas, ownSwordGone, w, h);
				blade = BladeMask(SwordDifference(heavySwordFrame, heavyBody), heavyBody, w, h);
			}

			Mask shieldPlaced(canvas.size(), 0);
			if (shielded) {
				ownShielded->Render(dir, index, colour, scratchMask, shieldedFrame);
				const Mask ownShield = OwnShieldMask(bareFrame, shieldedFrame, w, h);
				Mask shieldMask = ownShield;
				const Frame *shieldPixels = &shieldedFrame;
				if (swapShield) {
					shieldBare->Render(dir, index, colour, scratchMask, otherBare);
					shieldShielded->Render(dir, index, colour, scratchMask, otherShielded);
					shieldMask = ForeignShieldMask(otherBare, otherShielded, ownShield, w, h);
					shieldPixels = &otherShielded;
				}

				// What stood IN FRONT of the old shield stays in front of the new one: inside the old
				// shield's outline, a pixel its presence did not change was never behind it.
				Mask hull = Dilate(ownShield, w, h, 3);
				for (int k = 0; k < 3; k++)
					hull = Erode(hull, w, h);

				// Seen from behind, the heavy shield is a black back inside a white rim, and the heavy
				// sword crossing it came along in the difference. That back is wiped clean first.
				Mask inner;
				bool fromBehind = false;
				if (swapShield) {
					int blue = 0;
					int black = 0;
					const int total = Count(shieldMask);
					for (size_t i = 0; i < shieldMask.size(); i++) {
						if (shieldMask[i] == 0)
							continue;
						const int16_t v = (*shieldPixels)[i];
						if (v == ShadowIndex)
							black++;
						else if (v >= 184 && v <= 191)
							blue++;
					}
					fromBehind = total > 0 && blue * 20 < total && black * 100 > total * 35;
					if (fromBehind)
						inner = Erode(Erode(shieldMask, w, h), w, h);
				}

				for (size_t i = 0; i < canvas.size(); i++) {
					if (shieldMask[i] == 0)
						continue;
					const bool wasInFront = hull[i] != 0 && ownShield[i] == 0 && bareFrame[i] != Clear && bareFrame[i] == shieldedFrame[i];
					if (wasInFront && canvas[i] != Clear && ownSwordGone[i] == 0)
						continue;
					canvas[i] = (fromBehind && inner[i] != 0 && (*shieldPixels)[i] != ShadowIndex) ? ShadowIndex : (*shieldPixels)[i];
					shieldPlaced[i] = 1;
					brought[i] = swapShield && canvas[i] != ShadowIndex ? 1 : 0;
				}
			}

			if (doSword) {
				for (size_t i = 0; i < canvas.size(); i++) {
					if (blade[i] == 0)
						continue;
					// Over the shield, the blade shows only where the heavy tier's own shielded sheet shows
					// it too - that sheet is the one that knows which of the two is in front.
					if (shieldPlaced[i] != 0 && swapShield && otherShielded[i] != heavySwordFrame[i])
						continue;
					canvas[i] = heavySwordFrame[i];
					brought[i] = 1;
				}
			}
		}
	}

	// Pieces from the other tier must not take the class's dye: a heater shield's white lion sits on the
	// greys the Barbarian's mail is dyed by. Each such index is moved to one nothing in the sheet uses and
	// given the colour it had - which is why a mixed sheet carries colours of its own.
	std::shared_ptr<const SpriteColours> colours = dye;
	std::array<bool, 256> used {};
	for (const Frame &frame : composed)
		for (const int16_t v : frame)
			if (v != Clear)
				used[static_cast<size_t>(v)] = true;
	if (dye != nullptr) {
		auto own = std::make_shared<SpriteColours>(*dye);
		std::array<int16_t, 256> moved;
		moved.fill(Clear);
		int next = 1;
		for (size_t slot = 0; slot < composed.size(); slot++) {
			Frame &frame = composed[slot];
			for (size_t i = 0; i < frame.size(); i++) {
				if (foreign[slot][i] == 0 || frame[i] == Clear)
					continue;
				const auto original = static_cast<uint8_t>(frame[i]);
				if (!dye->HasOwn(original))
					continue;
				if (moved[original] == Clear) {
					while (next < 256 && (used[static_cast<size_t>(next)] || dye->HasOwn(static_cast<uint8_t>(next))))
						next++;
					if (next >= 256)
						continue; // out of room: this pixel takes the dye, which is a blemish and not a crash
					moved[original] = static_cast<int16_t>(next);
					used[static_cast<size_t>(next)] = true;
					own->Set(static_cast<uint8_t>(next), SharedPaletteRgb(original), original);
				}
				frame[i] = moved[original];
			}
		}
		colours = std::move(own);
	}

	int transparent = -1;
	for (int i = 1; i < 256; i++) {
		if (!used[static_cast<size_t>(i)]) {
			transparent = i;
			break;
		}
	}
	if (transparent < 0)
		return std::nullopt;

	std::vector<OwnedClxSpriteList> lists;
	lists.reserve(Facings);
	for (int dir = 0; dir < Facings; dir++) {
		OwnedSurface column(w, h * geometry.frames);
		for (int index = 0; index < geometry.frames; index++) {
			const Frame &frame = composed[static_cast<size_t>(dir * geometry.frames + index)];
			for (int y = 0; y < h; y++) {
				uint8_t *dst = &column[Point { 0, index * h + y }];
				for (int x = 0; x < w; x++) {
					const int16_t v = frame[static_cast<size_t>(y * w + x)];
					dst[x] = static_cast<uint8_t>(v == Clear ? transparent : v);
				}
			}
		}
		lists.push_back(SurfaceToClx(column, static_cast<unsigned>(geometry.frames), static_cast<uint8_t>(transparent)));
	}
	return ColouredSpriteSheet { CombineSpriteLists(lists), std::move(colours) };
}

} // namespace

// =====================================================================================================
// The request, the cache and the worker (v1.12.024)
//
// User, 2026-09-17: "a noticeable lag when first frame of a mixed sprite needs to occur like when i place a
// large shield or when i initiate an attack." The mix ran on the main thread at the moment an animation was
// first wanted. Now: a finished sheet is REMEMBERED (in memory for the session, on disk for good), it is
// BUILT AHEAD of need on a worker thread while the hero wears the plain sheet, and the archive reads - the
// one part that must stay on the main thread, because nothing says the MPQ reader is safe to share - are
// spread one sheet a tick.
// =====================================================================================================

namespace {

/** Bump when the mixer would produce different pixels, so old files on disk stop being believed. */
constexpr uint32_t CacheVersion = 1;
constexpr uint32_t CacheMagic = 0x584D534F; // "OSMX"

/** A finished sheet as bytes, so any number of heroes can be handed their own copy. */
struct CachedSheet {
	bool nothing = false; // the mixer declined: remember that too, it is the expensive answer to reach
	std::vector<uint8_t> data;
	uint16_t numLists = 0;
	std::shared_ptr<const SpriteColours> colours;
};

struct Job {
	PlayerSheetRequest request;
	std::string key;
	std::vector<std::pair<size_t, PlayerWeaponGraphic>> wanted; // still to read, main thread, one a tick
	std::map<std::pair<size_t, PlayerWeaponGraphic>, SourcePtr> loaded;
};

struct Finished {
	std::string key;
	std::shared_ptr<const CachedSheet> sheet;
};

std::mutex StateMutex; // guards everything below
std::condition_variable WorkerWake;
std::unordered_map<std::string, std::shared_ptr<const CachedSheet>> Memory;
std::deque<std::unique_ptr<Job>> Loading;  // main thread fills `loaded`
std::deque<std::unique_ptr<Job>> Ready;    // worker composes
std::deque<Finished> Done;                 // main thread delivers
std::unordered_map<std::string, bool> InFlight;
std::atomic<bool> WorkerStop { false };

/**
 * The worker, in a holder that JOINS on destruction. A bare static std::thread that is still joinable when the
 * statics are torn down calls std::terminate - which is how v1.12.024 ended every ordinary exit in abort().
 * ShutdownSpriteMixer is still called on the way out; this is what makes forgetting it harmless.
 */
struct WorkerHolder {
	std::thread thread;
	~WorkerHolder();
};
WorkerHolder WorkerThread;

std::string CachePath(const std::string &key)
{
	return paths::PrefPath() + "sprite_cache" + DirectorySeparator + key + ".osm";
}

std::shared_ptr<const CachedSheet> MakeCached(const std::optional<ColouredSpriteSheet> &result)
{
	auto cached = std::make_shared<CachedSheet>();
	if (!result) {
		cached->nothing = true;
		return cached;
	}
	const ClxSpriteSheet view { result->sheet };
	cached->data.assign(view.data(), view.data() + view.dataSize());
	cached->numLists = view.numLists();
	cached->colours = result->colours;
	return cached;
}

ColouredSpriteSheet Instantiate(const CachedSheet &cached)
{
	std::unique_ptr<uint8_t[]> copy { new uint8_t[cached.data.size()] };
	std::memcpy(copy.get(), cached.data.data(), cached.data.size());
	return ColouredSpriteSheet { OwnedClxSpriteSheet { std::move(copy), cached.numLists }, cached.colours };
}

void WriteU32(std::FILE *file, uint32_t value)
{
	const uint8_t bytes[4] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
	std::fwrite(bytes, 1, 4, file);
}

bool ReadU32(std::FILE *file, uint32_t &value)
{
	uint8_t bytes[4];
	if (std::fread(bytes, 1, 4, file) != 4)
		return false;
	value = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) | (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
	return true;
}

/** magic, version, kind (0 nothing / 1 sheet), lists, size, bytes, colour count, {index, rgb, fallback}... */
void SaveToDisk(const std::string &key, const CachedSheet &cached)
{
	RecursivelyCreateDir((paths::PrefPath() + "sprite_cache").c_str());
	std::FILE *file = OpenFile(CachePath(key).c_str(), "wb");
	if (file == nullptr)
		return; // a cache that cannot be written is a slower game, not a broken one
	WriteU32(file, CacheMagic);
	WriteU32(file, CacheVersion);
	WriteU32(file, cached.nothing ? 0 : 1);
	WriteU32(file, cached.numLists);
	WriteU32(file, static_cast<uint32_t>(cached.data.size()));
	if (!cached.data.empty())
		std::fwrite(cached.data.data(), 1, cached.data.size(), file);
	uint32_t own = 0;
	if (cached.colours != nullptr)
		for (int i = 0; i < 256; i++)
			if (cached.colours->HasOwn(static_cast<uint8_t>(i)))
				own++;
	WriteU32(file, cached.colours != nullptr ? own + 1 : 0); // 0 = no colours at all; n+1 = n own colours
	if (cached.colours != nullptr) {
		for (int i = 0; i < 256; i++) {
			const auto index = static_cast<uint8_t>(i);
			if (!cached.colours->HasOwn(index))
				continue;
			WriteU32(file, index);
			WriteU32(file, cached.colours->Own(index));
			WriteU32(file, cached.colours->Fallback(index));
		}
	}
	std::fclose(file);
}

std::shared_ptr<const CachedSheet> LoadFromDisk(const std::string &key)
{
	std::FILE *file = OpenFile(CachePath(key).c_str(), "rb");
	if (file == nullptr)
		return nullptr;
	auto cached = std::make_shared<CachedSheet>();
	uint32_t magic = 0, version = 0, kind = 0, lists = 0, size = 0, colourCount = 0;
	bool ok = ReadU32(file, magic) && ReadU32(file, version) && ReadU32(file, kind) && ReadU32(file, lists) && ReadU32(file, size)
	    && magic == CacheMagic && version == CacheVersion && size <= 64U * 1024U * 1024U && lists <= 64;
	if (ok) {
		cached->nothing = kind == 0;
		cached->numLists = static_cast<uint16_t>(lists);
		cached->data.resize(size);
		ok = size == 0 || std::fread(cached->data.data(), 1, size, file) == size;
	}
	if (ok)
		ok = ReadU32(file, colourCount) && colourCount <= 257;
	if (ok && colourCount > 0) {
		auto colours = std::make_shared<SpriteColours>();
		for (uint32_t i = 0; ok && i + 1 < colourCount; i++) {
			uint32_t index = 0, rgb = 0, fallback = 0;
			ok = ReadU32(file, index) && ReadU32(file, rgb) && ReadU32(file, fallback) && index < 256 && fallback < 256;
			if (ok)
				colours->Set(static_cast<uint8_t>(index), rgb, static_cast<uint8_t>(fallback));
		}
		cached->colours = std::move(colours);
	}
	std::fclose(file);
	if (!ok || (!cached->nothing && (cached->data.empty() || cached->numLists == 0)))
		return nullptr; // truncated or from another version: recompute, and the rewrite replaces it
	return cached;
}

/** Every sheet ComposeMixedSheet might ask for, so they can be read ahead on the main thread. */
std::vector<std::pair<size_t, PlayerWeaponGraphic>> SourcesFor(const PlayerSheetRequest &request)
{
	std::vector<std::pair<size_t, PlayerWeaponGraphic>> out;
	const auto want = [&](size_t tier, PlayerWeaponGraphic weapon) {
		const std::pair<size_t, PlayerWeaponGraphic> id { tier, weapon };
		if (std::find(out.begin(), out.end(), id) == out.end())
			out.push_back(id);
	};
	const PlayerWeaponGraphic bare = WithoutShield(request.weapon);
	want(request.armour, bare);
	if (HasShield(request.weapon)) {
		want(request.armour, request.weapon);
		if (request.look.shield == LookTier::Heavy) {
			want(HeavyArmour, bare);
			want(HeavyArmour, request.weapon);
		}
	}
	if (request.look.sword == LookTier::Heavy && bare == PlayerWeaponGraphic::Sword) {
		for (const PlayerWeaponGraphic voter : SwordVoters) {
			want(request.armour, voter);
			want(HeavyArmour, voter);
		}
	}
	return out;
}

SourcePtr LoadFor(const PlayerSheetRequest &request, size_t tier, PlayerWeaponGraphic weapon)
{
	return LoadSource(PlayersData[static_cast<size_t>(request.spriteClass)].classPath, CharChar[static_cast<size_t>(request.spriteClass)],
	    tier, weapon, request.cel.c_str(), request.frameWidth);
}

/** The whole derived sheet: mixed, then at the class's size. Pure CPU once the sources are in hand. */
std::optional<ColouredSpriteSheet> BuildDerivedSheet(const PlayerSheetRequest &request, const SourceLoader &load)
{
	std::optional<ColouredSpriteSheet> mixed = ComposeMixedSheet(request, load);
	if (!mixed)
		return std::nullopt;
	if (request.scalePercent != 100) {
		if (OptionalOwnedClxSpriteSheet scaled = ScaleSpriteSheet(mixed->sheet, request.scalePercent))
			mixed->sheet = std::move(*scaled);
	}
	return mixed;
}

void WorkerMain()
{
	for (;;) {
		std::unique_ptr<Job> job;
		{
			std::unique_lock<std::mutex> lock(StateMutex);
			WorkerWake.wait(lock, [] { return WorkerStop.load() || !Ready.empty(); });
			if (WorkerStop.load())
				return;
			job = std::move(Ready.front());
			Ready.pop_front();
		}
		const std::optional<ColouredSpriteSheet> result = BuildDerivedSheet(job->request, [&](size_t tier, PlayerWeaponGraphic weapon) -> SourcePtr {
			const auto it = job->loaded.find({ tier, weapon });
			return it != job->loaded.end() ? it->second : nullptr;
		});
		std::shared_ptr<const CachedSheet> cached = MakeCached(result);
		SaveToDisk(job->key, *cached);
		const std::lock_guard<std::mutex> lock(StateMutex);
		Memory[job->key] = cached;
		Done.push_back({ job->key, std::move(cached) });
	}
}

} // namespace

std::string PlayerSheetRequest::Key() const
{
	return fmt::format("{}{}{}-{}-k{}-d{}-s{}", CharChar[static_cast<size_t>(spriteClass)], ArmourChar[armour],
	    WepChar[static_cast<size_t>(weapon)], cel, (look.shield == LookTier::Heavy ? 2 : 0) | (look.sword == LookTier::Heavy ? 1 : 0),
	    dyeId, scalePercent);
}

PlayerSheetRequest MakePlayerSheetRequest(const Player &player, HeroClass spriteClass, PlayerWeaponGraphic weapon, const char *szCel, uint16_t frameWidth)
{
	PlayerSheetRequest request;
	request.spriteClass = spriteClass;
	request.armour = static_cast<uint8_t>(player._pgfxnum >> 4);
	request.weapon = weapon;
	request.look = GearLookFor(player);
	request.cel = szCel;
	request.frameWidth = frameWidth;
	request.scalePercent = SpriteScalePercent(player._pClass);
	request.dye = HeroColours(player);
	request.dyeId = request.dye != nullptr ? 1 : 0;
	return request;
}

bool WantsMixedSheet(const PlayerSheetRequest &request)
{
	if (request.spriteClass != HeroClass::Warrior)
		return false; // the only sheets measured
	if (request.armour >= HeavyArmour)
		return false; // already wearing the heavy tier's pieces
	const bool swapShield = request.look.shield == LookTier::Heavy && HasShield(request.weapon);
	const bool swapSword = request.look.sword == LookTier::Heavy && WithoutShield(request.weapon) == PlayerWeaponGraphic::Sword;
	return swapShield || swapSword;
}

std::optional<ColouredSpriteSheet> MixPlayerSheetNow(const PlayerSheetRequest &request)
{
	return BuildDerivedSheet(request, [&](size_t tier, PlayerWeaponGraphic weapon) { return LoadFor(request, tier, weapon); });
}

CachedSheetState TakeCachedPlayerSheet(const PlayerSheetRequest &request, std::optional<ColouredSpriteSheet> &out)
{
	const std::string key = request.Key();
	std::shared_ptr<const CachedSheet> cached;
	{
		const std::lock_guard<std::mutex> lock(StateMutex);
		if (const auto it = Memory.find(key); it != Memory.end())
			cached = it->second;
	}
	if (cached == nullptr) {
		cached = LoadFromDisk(key);
		if (cached == nullptr)
			return CachedSheetState::Unknown;
		const std::lock_guard<std::mutex> lock(StateMutex);
		Memory[key] = cached;
	}
	if (cached->nothing)
		return CachedSheetState::Nothing;
	out = Instantiate(*cached);
	return CachedSheetState::Ready;
}

void RequestPlayerSheet(const PlayerSheetRequest &request)
{
	auto job = std::make_unique<Job>();
	job->request = request;
	job->key = request.Key();
	job->wanted = SourcesFor(request);
	SharedPaletteRgb(0); // read town's palette HERE, on the main thread; the worker only ever reads the table
	const std::lock_guard<std::mutex> lock(StateMutex);
	if (InFlight[job->key])
		return;
	InFlight[job->key] = true;
	Loading.push_back(std::move(job));
}

void PumpSpriteMixer()
{
	// One archive read a tick. A plain animation change costs the game the same, so this is not felt.
	Job *job = nullptr;
	{
		const std::lock_guard<std::mutex> lock(StateMutex);
		if (!Loading.empty())
			job = Loading.front().get(); // only this thread ever removes from Loading
	}
	if (job == nullptr)
		return;
	if (!job->wanted.empty()) {
		const std::pair<size_t, PlayerWeaponGraphic> id = job->wanted.back();
		job->wanted.pop_back();
		job->loaded[id] = LoadFor(job->request, id.first, id.second);
		if (!job->wanted.empty())
			return;
	}
	const std::lock_guard<std::mutex> lock(StateMutex);
	Ready.push_back(std::move(Loading.front()));
	Loading.pop_front();
	if (!WorkerThread.thread.joinable()) {
		WorkerStop = false;
		WorkerThread.thread = std::thread(WorkerMain);
	}
	WorkerWake.notify_one();
}

std::vector<FinishedPlayerSheet> TakeFinishedPlayerSheets()
{
	std::vector<FinishedPlayerSheet> out;
	const std::lock_guard<std::mutex> lock(StateMutex);
	while (!Done.empty()) {
		Finished finished = std::move(Done.front());
		Done.pop_front();
		InFlight.erase(finished.key);
		FinishedPlayerSheet entry;
		entry.key = std::move(finished.key);
		entry.nothing = finished.sheet->nothing;
		entry.make = [sheet = std::move(finished.sheet)] { return Instantiate(*sheet); };
		out.push_back(std::move(entry));
	}
	return out;
}

void ShutdownSpriteMixer()
{
	{
		const std::lock_guard<std::mutex> lock(StateMutex);
		WorkerStop = true;
		Loading.clear();
		Ready.clear();
	}
	WorkerWake.notify_all();
	// Joined, not detached: the mutex and the queues are statics, and a thread still running while they are torn
	// down is exactly the kind of exit that leaves a windowless process behind. A job in hand finishes first - well
	// under a second - and no new one can start.
	if (WorkerThread.thread.joinable())
		WorkerThread.thread.join();
}

namespace {
WorkerHolder::~WorkerHolder()
{
	ShutdownSpriteMixer();
}
} // namespace

} // namespace devilution::oracool
