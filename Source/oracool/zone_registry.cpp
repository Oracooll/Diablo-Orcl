#include "oracool/zone_registry.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The world, as it stands. Ranges must be contiguous and ascending - FindZoneForLevel
 * walks them in order. A new zone is a new row past the last; nothing existing moves.
 */
constexpr ZoneDefinition Zones[] = {
	{ 0, 0, DTYPE_TOWN, "Tristram", nullptr },
	{ 1, 4, DTYPE_CATHEDRAL, "Cathedral", nullptr },
	{ 5, 8, DTYPE_CATACOMBS, "Catacombs", nullptr },
	{ 9, 12, DTYPE_CAVES, "Caves", nullptr },
	{ 13, 16, DTYPE_HELL, "Hell", nullptr },
	{ 17, 20, DTYPE_NEST, "Hive", nullptr },
	{ 21, 24, DTYPE_CRYPT, "Crypt", nullptr },
};

} // namespace

const char *GetZonePaletteOverride(int level)
{
	const ZoneDefinition *zone = FindZoneForLevel(level);
	return zone != nullptr ? zone->paletteOverride : nullptr;
}

const ZoneDefinition *FindZoneForLevel(int level)
{
	for (const ZoneDefinition &zone : Zones) {
		if (level >= zone.firstLevel && level <= zone.lastLevel)
			return &zone;
	}
	return nullptr;
}

int GetZoneCount()
{
	return static_cast<int>(sizeof(Zones) / sizeof(Zones[0]));
}

const ZoneDefinition &GetZone(int index)
{
	return Zones[index];
}

} // namespace devilution::oracool
