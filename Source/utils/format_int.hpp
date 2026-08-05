#pragma once

#include <cstdint>
#include <string>

namespace devilution {

/**
 * @brief Formats integer with thousands separator.
 */
std::string FormatInteger(int n);

/**
 * @brief Formats a 64-bit unsigned integer with thousands separator.
 * @details Oracool: needed for the extended level-99 experience values, which exceed INT_MAX.
 */
std::string FormatInteger(uint64_t n);

/**
 * @brief Oracool: disambiguates existing uint32_t callers (e.g. TotalPlayerGold()) - without this,
 * a uint32_t argument converts equally well to both the int and uint64_t overloads above.
 */
inline std::string FormatInteger(uint32_t n)
{
	return FormatInteger(static_cast<uint64_t>(n));
}

} // namespace devilution
