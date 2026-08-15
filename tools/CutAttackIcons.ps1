# Oracool asset pipeline: cuts ui\attack_icons.png - the two basic-attack icons the HUD's skill wells
# and the Abilities window's Skills sheet draw - out of the delivered contact sheet.
#
# REWRITTEN 2026-08-15 for the second delivery. The first arrived as two separate 1254x1254 files,
# each a framed stone tile on a green screen, and this script keyed and cropped them one at a time.
# The new one is a single labelled grid in the same format as Paladin Skills.png, so the pixel work
# is now tools\CutLabelledIconSheet.ps1's and only the layout lives here - which also means these
# icons and the Paladin skills are cut by identical code and cannot drift apart in weight or scale,
# and they are drawn in the same hand, so the Skills sheet reads as one set top to bottom.
#
# Strip order IS oracool::AttackIcon's enum order (Regular = 0, Fist = 1), which happens to be the
# sheet's own left-to-right order.
#
# ONE size, 38px, for both destinations. A larger 46px variant was cut for the HUD wells at one point
# - their openings are ~49x51, so 38 leaves a visible margin - and then dropped on an explicit call:
# the RMB well alternates between this icon and the engine's readied-spell icon, which is 37x38 and
# cannot be resized, so a bigger attack icon made that slot change size depending on what was in it.
# 38 also matches the aura, Barbarian and Paladin strips, so every row of the Abilities window and
# every icon on the HUD plate is the same size. Consistency beat snugness; if that is ever revisited,
# the well geometry is in oracool/hud_layout.h - and note SkillWellIconSize is asserted against this
# strip's cell size at run time, so a recut at another size fails loudly rather than drawing wrong.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutAttackIcons.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"

# Grid cell -> the game entity that takes that cell's drawing. Row-major, 2 columns x 1 row.
$layout = @("Regular Attack", "Fist Attack")

& "$PSScriptRoot\CutLabelledIconSheet.ps1" `
    -Source "..\Oracool.MPQ\Fist and Regular Attacks.png" `
    -Layout $layout `
    -Columns 2 -Rows 1 `
    -OutName "attack_icons.png" `
    -VaultDir "..\Oracool.MPQ\02-source-art\attack-skills"
