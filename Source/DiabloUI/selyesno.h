#pragma once

namespace devilution {

/**
 * @brief The delete-character confirmation.
 *
 * @param heroName the character's name, bare. It was a full "Are you sure ...?" sentence until the
 * user asked for that sentence to go; the name is all this screen prints, because it is the one
 * thing the title and the figure cannot say between them.
 */
bool UiSelHeroYesNoDialog(const char *title, const char *heroName);

} // namespace devilution
