#pragma once

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "items.h"
#include "player.h"

namespace devilution {

/**
 * @brief A range over non-empty items in a container.
 */
class ItemsContainerRange {
public:
	class Iterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = int;
		using value_type = Item;
		using pointer = value_type *;
		using reference = value_type &;

		Iterator() = default;

		Iterator(Item *items, std::size_t count, std::size_t index)
		    : items_(items)
		    , count_(count)
		    , index_(index)
		{
			advancePastEmpty();
		}

		pointer operator->() const
		{
			return &items_[index_];
		}

		reference operator*() const
		{
			return items_[index_];
		}

		Iterator &operator++()
		{
			++index_;
			advancePastEmpty();
			return *this;
		}

		Iterator operator++(int)
		{
			auto copy = *this;
			++(*this);
			return copy;
		}

		bool operator==(const Iterator &other) const
		{
			return index_ == other.index_;
		}

		bool operator!=(const Iterator &other) const
		{
			return !(*this == other);
		}

		[[nodiscard]] bool atEnd() const
		{
			return index_ == count_;
		}

		[[nodiscard]] std::size_t index() const
		{
			return index_;
		}

	private:
		void advancePastEmpty()
		{
			while (index_ < count_ && items_[index_].isEmpty()) {
				++index_;
			}
		}

		Item *items_ = nullptr;
		std::size_t count_ = 0;
		std::size_t index_ = 0;
	};

	ItemsContainerRange(Item *items, std::size_t count)
	    : items_(items)
	    , count_(count)
	{
	}

	[[nodiscard]] Iterator begin() const
	{
		return Iterator { items_, count_, 0 };
	}

	[[nodiscard]] Iterator end() const
	{
		return Iterator { nullptr, count_, count_ };
	}

private:
	Item *items_;
	std::size_t count_;
};

/**
 * @brief A range over non-empty items in a list of containers.
 */
class ItemsContainerListRange {
public:
	class Iterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = int;
		using value_type = Item;
		using pointer = value_type *;
		using reference = value_type &;

		Iterator() = default;

		explicit Iterator(std::vector<ItemsContainerRange::Iterator> iterators)
		    : iterators_(std::move(iterators))
		{
			advancePastEmpty();
		}

		pointer operator->() const
		{
			return iterators_[current_].operator->();
		}

		reference operator*() const
		{
			return iterators_[current_].operator*();
		}

		Iterator &operator++()
		{
			++iterators_[current_];
			advancePastEmpty();
			return *this;
		}

		Iterator operator++(int)
		{
			auto copy = *this;
			++(*this);
			return copy;
		}

		bool operator==(const Iterator &other) const
		{
			return current_ == other.current_ && iterators_[current_] == other.iterators_[current_];
		}
		bool operator!=(const Iterator &other) const
		{
			return !(*this == other);
		}

		/** @brief Which sub-container (in construction order) the current item belongs to. */
		[[nodiscard]] std::size_t containerIndex() const
		{
			return current_;
		}

		/** @brief The current item's index within its own sub-container (see containerIndex()). */
		[[nodiscard]] std::size_t index() const
		{
			return iterators_[current_].index();
		}

	private:
		void advancePastEmpty()
		{
			while (current_ + 1 < iterators_.size() && iterators_[current_].atEnd()) {
				++current_;
			}
		}

		std::vector<ItemsContainerRange::Iterator> iterators_;
		std::size_t current_ = 0;
	};
};

/**
 * @brief A range over equipped player items.
 */
class EquippedPlayerItemsRange {
public:
	explicit EquippedPlayerItemsRange(Player &player)
	    : player_(&player)
	{
	}

	[[nodiscard]] ItemsContainerRange::Iterator begin() const
	{
		return ItemsContainerRange::Iterator { &player_->InvBody[0], containerSize(), 0 };
	}

	[[nodiscard]] ItemsContainerRange::Iterator end() const
	{
		return ItemsContainerRange::Iterator { nullptr, containerSize(), containerSize() };
	}

private:
	[[nodiscard]] std::size_t containerSize() const
	{
		return sizeof(player_->InvBody) / sizeof(player_->InvBody[0]);
	}

	Player *player_;
};

/**
 * @brief Oracool Tabbed Inventory: shared by InventoryPlayerItemsRange/InventoryAndBeltPlayerItemsRange/
 * PlayerItemsRange below - the flat list of begin (or end) iterators covering the original backpack
 * (InvList) followed by all 9 extra tabs (InvTabList), so every general-purpose "scan the player's
 * inventory" algorithm built on these ranges picks up extra-tab items for free instead of silently
 * skipping them (a real bug found via user report - quest turn-ins, stat-flag refreshes, and several
 * shrine effects all missed items filed into an extra tab before this).
 */
inline std::vector<ItemsContainerRange::Iterator> InventoryContainerBeginIterators(Player &player)
{
	std::vector<ItemsContainerRange::Iterator> iterators;
	iterators.reserve(1 + Player::NumExtraInventoryTabs);
	iterators.push_back(ItemsContainerRange::Iterator { &player.InvList[0], static_cast<std::size_t>(player._pNumInv), 0 });
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		const auto count = static_cast<std::size_t>(player._pNumInvTab[t]);
		iterators.push_back(ItemsContainerRange::Iterator { &player.InvTabList[t][0], count, 0 });
	}
	return iterators;
}

/** @brief End-iterator equivalent of InventoryContainerBeginIterators; see that function. */
inline std::vector<ItemsContainerRange::Iterator> InventoryContainerEndIterators(Player &player)
{
	std::vector<ItemsContainerRange::Iterator> iterators;
	iterators.reserve(1 + Player::NumExtraInventoryTabs);
	const auto invCount = static_cast<std::size_t>(player._pNumInv);
	iterators.push_back(ItemsContainerRange::Iterator { nullptr, invCount, invCount });
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		const auto count = static_cast<std::size_t>(player._pNumInvTab[t]);
		iterators.push_back(ItemsContainerRange::Iterator { nullptr, count, count });
	}
	return iterators;
}

/**
 * @brief A range over non-equipped inventory player items - the original backpack (InvList) plus
 * every Oracool Tabbed Inventory extra tab (InvTabList), flattened into one iteration.
 */
class InventoryPlayerItemsRange {
public:
	explicit InventoryPlayerItemsRange(Player &player)
	    : player_(&player)
	{
	}

	[[nodiscard]] ItemsContainerListRange::Iterator begin() const
	{
		return ItemsContainerListRange::Iterator(InventoryContainerBeginIterators(*player_));
	}

	[[nodiscard]] ItemsContainerListRange::Iterator end() const
	{
		return ItemsContainerListRange::Iterator(InventoryContainerEndIterators(*player_));
	}

private:
	Player *player_;
};

/**
 * @brief A range over belt player items.
 */
class BeltPlayerItemsRange {
public:
	explicit BeltPlayerItemsRange(Player &player)
	    : player_(&player)
	{
	}

	[[nodiscard]] ItemsContainerRange::Iterator begin() const
	{
		return ItemsContainerRange::Iterator { &player_->SpdList[0], containerSize(), 0 };
	}

	[[nodiscard]] ItemsContainerRange::Iterator end() const
	{
		return ItemsContainerRange::Iterator { nullptr, containerSize(), containerSize() };
	}

private:
	[[nodiscard]] std::size_t containerSize() const
	{
		return sizeof(player_->SpdList) / sizeof(player_->SpdList[0]);
	}

	Player *player_;
};

/**
 * @brief A range over non-equipped player items in the following order: Inventory, Belt.
 */
class InventoryAndBeltPlayerItemsRange {
public:
	explicit InventoryAndBeltPlayerItemsRange(Player &player)
	    : player_(&player)
	{
	}

	[[nodiscard]] ItemsContainerListRange::Iterator begin() const
	{
		auto iterators = InventoryContainerBeginIterators(*player_);
		iterators.push_back(BeltPlayerItemsRange(*player_).begin());
		return ItemsContainerListRange::Iterator(std::move(iterators));
	}

	[[nodiscard]] ItemsContainerListRange::Iterator end() const
	{
		auto iterators = InventoryContainerEndIterators(*player_);
		iterators.push_back(BeltPlayerItemsRange(*player_).end());
		return ItemsContainerListRange::Iterator(std::move(iterators));
	}

private:
	Player *player_;
};

/**
 * @brief A range over non-empty player items in the following order: Equipped, Inventory (plus
 * every Oracool Tabbed Inventory extra tab), Belt.
 */
class PlayerItemsRange {
public:
	explicit PlayerItemsRange(Player &player)
	    : player_(&player)
	{
	}

	[[nodiscard]] ItemsContainerListRange::Iterator begin() const
	{
		std::vector<ItemsContainerRange::Iterator> iterators;
		iterators.push_back(EquippedPlayerItemsRange(*player_).begin());
		for (auto &it : InventoryContainerBeginIterators(*player_))
			iterators.push_back(it);
		iterators.push_back(BeltPlayerItemsRange(*player_).begin());
		return ItemsContainerListRange::Iterator(std::move(iterators));
	}

	[[nodiscard]] ItemsContainerListRange::Iterator end() const
	{
		std::vector<ItemsContainerRange::Iterator> iterators;
		iterators.push_back(EquippedPlayerItemsRange(*player_).end());
		for (auto &it : InventoryContainerEndIterators(*player_))
			iterators.push_back(it);
		iterators.push_back(BeltPlayerItemsRange(*player_).end());
		return ItemsContainerListRange::Iterator(std::move(iterators));
	}

private:
	Player *player_;
};

} // namespace devilution
