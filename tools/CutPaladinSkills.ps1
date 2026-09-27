# Oracool asset pipeline: cuts ui\paladin_skill_icons.png out of the delivered contact sheet.
#
# All the pixel work lives in tools\CutLabelledIconSheet.ps1, which every sheet in this format
# shares; what is specific to the Paladin lives here, in the layout table below.
#
# Strip order IS PaladinSkill's enum order - GetPaladinSkillIconIndex is the identity, so the sheet
# and the enum cannot drift.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutPaladinSkills.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"

# Grid cell -> the GAME SKILL that takes that cell's drawing. $null means "on the sheet, not in the
# game". Row-major, 4 columns x 2 rows, exactly as delivered.
#
# Cells 5 and 6 do not match their printed labels, deliberately (user request, 2026-08-15): the sheet
# labels cell 5 "Smite" and cell 6 "Shield Bash", but cell 5's drawing - a shield driven into a
# recoiling figure - is the one that reads as a bash, while cell 6 is a shield with an impact burst
# beside it. So Shield Bash takes cell 5's art and cell 6 is the one held back. Smite itself was to
# be left out either way ("ignore this skill for now. Don't add it.").
$layout = @(
    "Charge", "Zeal", "Hammer of Faith", "Blessed Shield",
    "Fist of the Heavens", "Shield Bash", $null, "Blessed Hammer"
)

& "$PSScriptRoot\CutLabelledIconSheet.ps1" `
    -Source "..\Resources\02. Oracooll Assets\skill-glyphs\paladin-skills\Paladin Skills.png" `
    -Layout $layout `
    -Columns 4 -Rows 2 `
    -OutName "paladin_skill_icons.png" `
    -VaultDir "..\Resources\02. Oracooll Assets\skill-glyphs\paladin-skills"
