Diablo Orcl V1 - Oracool Edition v{{VERSION}}
Windows x64, Release build
========================================

GAME DATA YOU MUST SUPPLY

This package contains no original game data and cannot: those archives are
Blizzard's, and they are not ours to hand out. You supply them from copies of
Diablo and Hellfire that you own - the GOG installers, the original discs, or
an existing installation.

Copy them into this folder, next to DiabloOrcl.exe.


  REQUIRED

    diabdat.mpq       Diablo's game data. Without it the game cannot start and
                      will ask you to insert the CD.

  REQUIRED FOR THE FULL GAME - all four, or none of them

    hellfire.mpq      Hellfire's data.
    hfmonk.mpq        The Monk's sprites and sound.
    hfmusic.mpq       Hellfire's music.
    hfvoice.mpq       Hellfire's speech.

    Oracool Edition is built on top of Hellfire, not just Diablo. Its area
    ladder runs to 24 areas and includes the Nest and the Crypt, which are
    Hellfire's dungeons, and the Monk is one of its six classes. Without these
    you get a shorter game and five classes instead of six.

    *** These four go together. If hellfire.mpq is present and any of the
    other three is missing, the game shows "Some Hellfire MPQs are missing"
    and exits. It is all four or none. ***

  OPTIONAL

    hfbard.mpq        Forces the Bard on. Not needed - the Bard is offered by
    hfbarb.mpq        default, and the Barbarian can be enabled in the options.
                      Both classes borrow the Rogue's and the Warrior's
                      artwork, so these archives only ever added voices.

    hfmonk.mpq alone  If you own Hellfire but would rather play Diablo's
    (no hellfire.mpq) content, supplying only the monk archive gets you the
                      Monk class without turning on Hellfire's quests, levels,
                      monsters and item tables. Oracool searches this one
                      archive whether or not it is a Hellfire game, precisely
                      so that this works.


HOW TO RUN

  1. Copy the archives above into this folder.
  2. Run DiabloOrcl.exe.

Saves and settings go to %APPDATA%\diasurgical\devilution\ - not to this
folder - so you can replace this build in place without losing a character.

  *** SAVES: this is a development build and save compatibility is NOT being
  maintained between versions. A character from an earlier release may fail to
  load or may lose items. Do not get attached to a hero you care about. ***


WHAT IS IN HERE

  DiabloOrcl.exe   The game.
  oracool.mpq      Oracool Edition's own art and data. Searched before every
                   other archive, so it overrides the original game's assets
                   without diabdat.mpq or the Hellfire archives ever being
                   modified.
  assets\          Fonts and interface art (the loose form of devilutionx.mpq).
                   REQUIRED - the game will not start without this folder.
  *.dll            SDL2 and the compression/format libraries the game links.

All of it is either this fork's own work or open-source dependencies. Nothing
here is redistributed commercial content.


BUILT FROM

  Repository : github.com/Oracooll/Diablo-Orcl
  Branch     : oracool-v1-main
  Version    : {{VERSION}}

Based on DevilutionX, which is itself a reimplementation of the Diablo engine.
