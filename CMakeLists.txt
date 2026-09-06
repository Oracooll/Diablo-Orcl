cmake_minimum_required(VERSION 3.15)

foreach(_policy CMP0111 CMP0126 CMP0135)
  if(POLICY ${_policy})
    cmake_policy(SET ${_policy} NEW)
    set(CMAKE_POLICY_DEFAULT_${_policy} NEW)
  endif()
endforeach()

# Projects added via `add_subdirectory` or `FetchContent` may have a lower
# `cmake_minimum_required` than we set here. Set policies that we require
# to their new value so that they still apply.
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
  message("-- Detected a source distribution with the required FetchContent dependencies and devilutionx.mpq included")
  set(SRC_DIST ON)
  add_subdirectory(dist)
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(WARNING [[In-source build detected, please eg. create a new directory and use `cmake ..`]])
endif()

include(CMakeDependentOption)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/finders")
include(functions/genex)

# Options required by `VcPkgManifestFeatures`, which must be included before the `project` call.
option(USE_SDL1 "Use SDL1.2 instead of SDL2" OFF)
option(NONET "Disable network support" OFF)
cmake_dependent_option(PACKET_ENCRYPTION "Encrypt network packets" ON "NOT NONET" OFF)
# The gettext[tools] package takes a very long time to install
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake$")
  option(USE_GETTEXT_FROM_VCPKG "Add vcpkg dependency for gettext[tools] for compiling translations" OFF)
endif()
option(BUILD_TESTING "Build tests." ON)

# These must be included after the options above but before the `project` call.
include(VcPkgManifestFeatures)

# Set up the `project` before the rest of the options so that:
#
# 1. Properties such as `TARGET_SUPPORTS_SHARED_LIBS` are defined.
# 2. Toolchain file is evaluated, required for `Platforms.cmake`,
#    which can override the options.
if(NOT VERSION_NUM)
  file(STRINGS "VERSION" VERSION_STR)
  if(NOT "${VERSION_STR}" STREQUAL "")
    string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)(.*)?" VERSION_PREFIX ${VERSION_STR})
    set(VERSION_NUM ${CMAKE_MATCH_1})
  endif()
endif()

# Oracool Edition release version, shown on the main menu. This is
# independent from VERSION above, which is the DevilutionX engine base and
# also drives the multiplayer version-compatibility check.
if(NOT ORACOOL_VERSION_STR)
  file(STRINGS "ORACOOL_VERSION" ORACOOL_VERSION_STR)
endif()
# Oracool: `file(STRINGS ...)` above only runs when CMake actually configures,
# not on a plain `cmake --build` - and editing this plain data file was never
# declared as a reason to reconfigure, so bumping ORACOOL_VERSION alone was
# silently having zero effect on the built binary (it kept whatever version
# string was baked in from the project's last real configure, however old).
# This makes CMake notice the file changed and reconfigure automatically.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ORACOOL_VERSION")

# Oracool: the version split into numeric components, for the multiplayer wire identity
# (external audit, 2026-08-17). The wire used to advertise the DevilutionX base version, which
# stopped being honest the moment PlayerNetPack grew past vanilla's shape - a stock 1.5.5 client
# could version-match this fork and then corrupt on the first player-info exchange. See
# InitGameInfo (multi.cpp) and the match check in DiabloUI/multi/selgame.cpp.
string(REPLACE "." ";" ORACOOL_VERSION_PARTS "${ORACOOL_VERSION_STR}")
list(GET ORACOOL_VERSION_PARTS 0 ORACOOL_VERSION_MAJOR)
list(GET ORACOOL_VERSION_PARTS 1 ORACOOL_VERSION_MINOR)
list(GET ORACOOL_VERSION_PARTS 2 ORACOOL_VERSION_PATCH)
# Oracool (2026-09-07): the patch is written with three digits ("008"), and a C++ literal with a
# leading zero is OCTAL - 008 does not compile (multi.cpp, selgame.cpp). Strip the zeros before
# the numbers reach the compiler; 007 happened to be a valid octal 7, which is why it never showed.
foreach(part MAJOR MINOR PATCH)
  string(REGEX REPLACE "^0+([0-9])" "\\1" ORACOOL_VERSION_${part} "${ORACOOL_VERSION_${part}}")
endforeach()

if(NOT VERSION_SUFFIX)
  # For versions with a suffix, e.g. "1.5.0-dev", include
  # the build type and the git hash.
  set(VERSION_SUFFIX "-$<CONFIG>")
  if(VERSION_PREFIX MATCHES "-")
    if(NOT GIT_COMMIT_HASH)
      include(functions/git)
      get_git_commit_hash(GIT_COMMIT_HASH)
    endif()
    if(GIT_COMMIT_HASH)
      set(VERSION_SUFFIX "${VERSION_SUFFIX}-${GIT_COMMIT_HASH}")
    endif()
  else()
    # For versions without a suffix, e.g. "1.5.0" rather than "1.5.0-dev",
    # only include the build type if it is a debug build.
    set(VERSION_SUFFIX "$<$<CONFIG:Debug>:$<CONFIG>>")
  endif()
endif()

project(DevilutionX
  VERSION ${VERSION_NUM}
  LANGUAGES C CXX)
set(PROJECT_VERSION_WITH_SUFFIX "${VERSION_PREFIX}${VERSION_SUFFIX}")

# Platform definitions can override options and we want `cmake_dependent_option` to see the effects.
# Note that a few options are still defined before this because they're needed by `VcPkgManifestFeatures.cmake`.
include(Platforms)

# This built-in CMake module adds a BUILD_TESTING option (ON by default).
# Must be included in the top-level `CMakeLists.txt` after calling `project`.
# Because we must include `VcPkgManifestFeatures` before the `project` call,
# we add a BUILD_TESTING option ourselves above as well.
include(CTest)

# Debugging / profiling options
DEBUG_OPTION(ASAN "Enable address sanitizer")
DEBUG_OPTION(UBSAN "Enable undefined behaviour sanitizer")
option(TSAN "Enable thread sanitizer (not compatible with ASAN=ON)" OFF)
DEBUG_OPTION(DEBUG "Enable debug mode in engine")
option(GPERF "Build with GPerfTools profiler" OFF)
cmake_dependent_option(GPERF_HEAP_FIRST_GAME_ITERATION "Save heap profile of the first game iteration" OFF "GPERF" OFF)
option(ENABLE_CODECOVERAGE "Instrument code for code coverage (only enabled with BUILD_TESTING)" OFF)

# Packaging options
RELEASE_OPTION(CPACK "Configure CPack")
option(MACOSX_STANDALONE_APP_BUNDLE "Generate a portable app bundle to use on other devices (requires sudo)" OFF)

# Network options
cmake_dependent_option(DISABLE_TCP "Disable TCP multiplayer option" OFF "NOT NONET" ON)
# Oracool: DEFAULTS TO ON, unlike upstream (user, 2026-08-29: "disable zero tier, we are single
# player only").
#
# ZeroTier is the internet-multiplayer TRANSPORT, not the multiplayer screens - those stay exactly
# as they are, per the standing rule not to delete them. What goes is a network stack this fork
# cannot reach: V1 never enters a multiplayer game, and oracool::IsMultiplayerReachable() is a
# compile-time statement of that.
#
# It is not free to keep. 3rdParty/libzt builds zt.lib (127 MB), zto_pic.lib (93 MB) and zt_pic.lib
# (25 MB) into every build tree, plus its own source under _deps - and the build trees sit inside
# OneDrive, so all of it is sync traffic for a mode the game will not enter.
#
# The default is changed here rather than pinned in each build cache so a fresh configure inherits
# it. Five upstream platform files already force it ON for the same reason (n3ds, switch, gkd350h,
# emscripten, macos_tiger), so this is a supported configuration rather than a new one, and the code
# it removes is behind two #ifndef guards plus one connection-select entry.
cmake_dependent_option(DISABLE_ZERO_TIER "Disable ZeroTier multiplayer option" ON "NOT NONET" ON)

# Graphics options
if(NOT USE_SDL1)
  set(DEVILUTIONX_DISPLAY_TEXTURE_FORMAT "SDL_PIXELFORMAT_RGB888" CACHE STRING "Texture format for DevilutionX textures when using the GPU renderer")
  mark_as_advanced(DEVILUTIONX_DISPLAY_TEXTURE_FORMAT)
endif()

# Sound options
option(NOSOUND "Disable sound support" OFF)
option(DEVILUTIONX_RESAMPLER_SPEEX "Build with Speex resampler" ON)
cmake_dependent_option(DEVILUTIONX_RESAMPLER_SDL "Build with SDL resampler" ON "NOT USE_SDL1" OFF)
if(DEVILUTIONX_RESAMPLER_SPEEX)
  list(APPEND _resamplers Speex)
endif()
if(DEVILUTIONX_RESAMPLER_SDL)
  list(APPEND _resamplers SDL)
endif()
list(GET _resamplers 0 _default_resampler)
set(DEVILUTIONX_DEFAULT_RESAMPLER ${_default_resampler} CACHE STRING "Default resampler")
set_property(CACHE DEVILUTIONX_DEFAULT_RESAMPLER PROPERTY STRINGS ${_resamplers})

# Optimization / link options
option(DISABLE_LTO "Disable link-time optimization (by default enabled in release mode)" OFF)
option(PIE "Generate position-independent code" OFF)
cmake_dependent_option(DEVILUTIONX_DISABLE_RTTI "Disable RTTI" ON "NONET" OFF)
cmake_dependent_option(DEVILUTIONX_DISABLE_EXCEPTIONS "Disable exceptions" ON "NONET" OFF)
RELEASE_OPTION(DEVILUTIONX_STATIC_CXX_STDLIB "Link C++ standard library statically (if available)")
option(DEVILUTIONX_PROFILE_GENERATE "Build a binary that generates the profile for PGO" OFF)
option(DEVILUTIONX_PROFILE_USE "Build with PGO using the given profile file" OFF)
set(DEVILUTIONX_PROFILE_DIR "" CACHE STRING "Directory where the profile is stored")

include(MoldLinker)

# Memory / performance trade-off options
option(UNPACKED_MPQS "Expect MPQs to be unpacked and the data converted with devilutionx-mpq-tools" OFF)
option(UNPACKED_SAVES "Uses unpacked save files instead of MPQ .sv/.hsv files" OFF)
option(DISABLE_STREAMING_MUSIC "Disable streaming music (to work around broken platform implementations)" OFF)
mark_as_advanced(DISABLE_STREAMING_MUSIC)
option(DISABLE_STREAMING_SOUNDS "Disable streaming sounds (to work around broken platform implementations)" OFF)
mark_as_advanced(DISABLE_STREAMING_SOUNDS)
set(STREAM_ALL_AUDIO_MIN_FILE_SIZE "" CACHE STRING "If set, stream all the audio files larger than this size")
mark_as_advanced(STREAM_ALL_AUDIO_MIN_FILE_SIZE)
option(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT "Whether to use a lookup table for transparency blending with black. This improves performance of blending transparent black overlays, such as quest dialog background, at the cost of 128 KiB of RAM." ON)
mark_as_advanced(DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT)

# Additional features
option(DISABLE_DEMOMODE "Disable demo mode support" OFF)
option(DISCORD_INTEGRATION "Build with Discord SDK for rich presence support" OFF)

# If both UNPACKED_MPQS and UNPACKED_SAVES are enabled, we completely remove MPQ support.
if(UNPACKED_MPQS AND UNPACKED_SAVES)
  set(SUPPORTS_MPQ OFF)
else()
  set(SUPPORTS_MPQ ON)
endif()

# By default, devilutionx.mpq is built only if smpq is installed and MPQ support is enabled.
if(SUPPORTS_MPQ)
  if(NOT DEFINED BUILD_ASSETS_MPQ AND NOT SRC_DIST)
    find_program(SMPQ smpq)
  elseif(BUILD_ASSETS_MPQ)
    find_program(SMPQ smpq REQUIRED)
  endif()
  if(SMPQ)
    set(_has_smpq ON)
  else()
    set(_has_smpq OFF)
  endif()
  option(BUILD_ASSETS_MPQ "If true, assets are packaged into devilutionx.mpq." ${_has_smpq})
else()
  set(BUILD_ASSETS_MPQS OFF)
endif()

# === Option overrides ===
# TSAN is not compatible with ASAN.
if(TSAN)
  set(ASAN OFF)
endif()

# Oracool: SMALLER DEBUG SYMBOLS (user, 2026-08-29: "can you reduce the size of Debug folder? It is
# becoming too big to constantly sync over onedrive").
#
# The build tree lives inside OneDrive, so every rebuild is an upload. A full Debug link wrote a
# 989 MB DiabloOrcl.pdb and a 732 MB libdevilutionx_so.pdb, plus 1.3 GB across the 35 test binaries -
# 3.4 GB of symbols regenerated and re-synced on every build, which was two thirds of the whole tree.
#
# /DEBUG:FASTLINK makes the linker PDB REFERENCE the debug info already sitting in the .obj files
# instead of copying it in. The trade is real and worth stating: the resulting PDB is only usable
# while those .obj files are still present, so it cannot be shipped or archived on its own. That is
# exactly the case here - these symbols are for debugging this tree in place, and the objects never
# leave it. A Release build is untouched; it barely has symbols to shrink (175 MB against 3,433).
#
# Opt out with -DDEVILUTIONX_FULL_DEBUG_PDB=ON if a PDB ever has to travel.
option(DEVILUTIONX_FULL_DEBUG_PDB "Write standalone (large) Debug PDBs instead of /DEBUG:FASTLINK" OFF)
if(MSVC AND NOT DEVILUTIONX_FULL_DEBUG_PDB)
  add_link_options("$<$<CONFIG:Debug>:/DEBUG:FASTLINK>")
endif()

if(MSVC AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT DISABLE_LTO)
  # Work around MSVC + CMake bug when LTO is enabled.
  # See https://github.com/diasurgical/devilutionX/issues/3778
  # and https://gitlab.kitware.com/cmake/cmake/-/issues/23035
  set(BUILD_TESTING OFF)
endif()

# Note: `CMAKE_CROSSCOMPILING` is only available after the `project` call.
if(CMAKE_CROSSCOMPILING)
  set(BUILD_TESTING OFF)
endif()

if(DISABLE_DEMOMODE)
  # Testing requires demomode.
  set(BUILD_TESTING OFF)
endif()

if(BUILD_TESTING)
  # When tests are enabled, we build a shared devilutionx_so library, which needs to be PIC to link.
  set(PIE ON)
endif()

# Recalculate the dependent options that are defined before `include(Platforms)`:
if(NONET)
  # PACKET_ENCRYPTION is defined before `Platforms.cmake` is included.
  # This means that if a `Platforms.cmake` sets NONET to OFF, PACKET_ENCRYPTION will not automatically
  # reflect that.
  set(PACKET_ENCRYPTION OFF)
endif()
# === End of option overrides ===

if(PIE)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if(DEVILUTIONX_DISABLE_RTTI)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  elseif(MSVC)
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif()
endif()

if(DEVILUTIONX_DISABLE_EXCEPTIONS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
  elseif(MSVC)
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  endif()
endif()

# Remove unused symbols in non-debug mode.
# This is useful even with LTO (-84 KiB with MinSizeRel).
#
# PS4 toolchain crashes in `create-fself` when linking with these flags, so we exclude it:
# https://github.com/PacBrew/ps4-openorbis/issues/8
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT PS4)
  add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-ffunction-sections;-fdata-sections>")
  if(APPLE)
    add_link_options("$<$<NOT:$<CONFIG:Debug>>:LINKER:-dead_strip>")
  else()
    add_link_options("$<$<NOT:$<CONFIG:Debug>>:LINKER:--gc-sections,--as-needed>")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # -fipa-pta and -fdevirtualize-at-ltrans improve performance.
  add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-fipa-pta;-fdevirtualize-at-ltrans>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options("$<$<BOOL:${DEVILUTIONX_PROFILE_GENERATE}>:-fprofile-generate>")
  add_link_options("$<$<BOOL:${DEVILUTIONX_PROFILE_GENERATE}>:-fprofile-generate>")
  add_compile_options("$<$<BOOL:${DEVILUTIONX_PROFILE_USE}>:-fprofile-use>")
  add_link_options("$<$<BOOL:${DEVILUTIONX_PROFILE_USE}>:-fprofile-use>")
  add_compile_options("$<$<BOOL:${DEVILUTIONX_PROFILE_DIR}>:-fprofile-dir=${DEVILUTIONX_PROFILE_DIR};-fprofile-prefix-path=${CMAKE_CURRENT_BINARY_DIR}>")
  add_link_options("$<$<BOOL:${DEVILUTIONX_PROFILE_DIR}>:-fprofile-dir=${DEVILUTIONX_PROFILE_DIR};-fprofile-prefix-path=${CMAKE_CURRENT_BINARY_DIR}>")
endif()

# Not a genexp because CMake doesn't support it
# https://gitlab.kitware.com/cmake/cmake/-/issues/20546
if(NOT DISABLE_LTO)
  # LTO if supported:
  include(CheckIPOSupported)
  check_ipo_supported(RESULT is_ipo_supported OUTPUT lto_error)
  if(is_ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  endif()
endif()

if(GPERF)
  set(DEVILUTIONX_STATIC_CXX_STDLIB OFF)

  if(GPERF_HEAP_FIRST_GAME_ITERATION)
    set(GPERF_HEAP_MAIN ON)
  endif()

  # Compile with information about file and line numbers for everything
  # even in non-Debug build types.
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-g2>")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Use the more size-efficient `-gmlt` option on clang.
    add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-gmlt>")
  endif()
endif()

# Despite setting C++ standard to 20, features from this version are not being used.
# The oldest compiler used is GCC 6.5 - and that defines our C++ feature set (meaning most of C++17).
# It's present only to take advantage of fmt::format build time errors.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clang-tidy
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Dependencies must be included after Platforms.
include(Dependencies)

add_subdirectory(Source)

set(BIN_TARGET devilutionx)
if(NINTENDO_3DS)
  set(BIN_TARGET ${BIN_TARGET}.elf)
endif()

if(ANDROID)
  add_library(${BIN_TARGET} SHARED Source/main.cpp)
elseif(UWP_LIB)
  add_custom_target(${BIN_TARGET})  # we only need libdevilutionx
else()
  add_executable(${BIN_TARGET}
    WIN32
    MACOSX_BUNDLE
    Source/main.cpp
    Packaging/windows/devilutionx.exe.manifest
    Packaging/windows/devilutionx.rc
    Packaging/apple/LaunchScreen.storyboard)

  # The authoritative Windows version stamp, generated from ORACOOL_VERSION. See the template's own
  # header for why packaging needs it and why --version could not serve.
  if(WIN32)
    configure_file(
      "${PROJECT_SOURCE_DIR}/Packaging/windows/oracool_version.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/oracool_version.rc"
      @ONLY)
    target_sources(${BIN_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/oracool_version.rc")
  endif()

    if(CMAKE_STRIP AND NOT DEVILUTIONX_DISABLE_STRIP)
      add_custom_command(
        TARGET ${BIN_TARGET} POST_BUILD
        DEPENDS ${BIN_TARGET}
        COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
                $<$<CONFIG:MinSizeRel>:${CMAKE_STRIP}>
        ARGS $<TARGET_FILE:${BIN_TARGET}>)
    endif()

    if(WIN32)
      set_target_properties(${BIN_TARGET} PROPERTIES OUTPUT_NAME "DiabloOrcl")
    endif()
endif()

if(NOT UWP_LIB)
  target_link_libraries(${BIN_TARGET} PRIVATE libdevilutionx)

# Oracool: packs Packaging/resources/oracool_assets/ into oracool.mpq, Oracool Edition's own asset
# archive. Uses the engine's own MpqWriter (save files are MPQs, so the writer already exists)
# rather than the external smpq that devilutionx.mpq needs and which is not installed here.
add_executable(oracool_mpq_pack tools/oracool_mpq_pack.cpp)
target_link_libraries(oracool_mpq_pack PRIVATE libdevilutionx)

# Oracool: the original-art exporter (tools/oracool_art_export.cpp) - runs from the build directory,
# where the archives are, and writes PNGs through the engine's own loaders. Not part of ALL.
add_executable(oracool_art_export EXCLUDE_FROM_ALL tools/oracool_art_export.cpp)
target_link_libraries(oracool_art_export PRIVATE libdevilutionx)

# ORACOOL ASSETS ARE PART OF THE BUILD GRAPH (external audit of v1.9.92, finding 3).
#
# They were not, and the consequence was exactly what you would expect: `cmake --build` rebuilt the
# C++ cleanly while oracool.mpq stayed whatever a prior manual command had produced. The audit caught
# a Debug tree whose executable said v1.9.92 and whose archive predated one of its own source assets
# by nearly an hour - so "tested on v1.9.92 Debug" was not true of the art. This project has already
# shipped two asset units that never reached the game for the same reason (v1.8.38, v1.8.40).
#
# The dependency runs libdevilutionx -> oracool_mpq_pack -> oracool.mpq -> the game binary. No cycle:
# the packer and the game are both consumers of the library, and only the game waits on the archive.
file(GLOB_RECURSE ORACOOL_ASSET_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/resources/oracool_assets/*")
set(ORACOOL_MPQ "${CMAKE_CURRENT_BINARY_DIR}/oracool.mpq")
# The response file exists because the asset count passed 400 and the accumulated command line hit
# Windows' ~8191-character limit, which cmd.exe reports as "The input line is too long" with no cause.
set(ORACOOL_MPQ_LIST "${CMAKE_CURRENT_BINARY_DIR}/oracool_mpq_files.txt")
set(_oracool_rel_list "")
foreach(_asset ${ORACOOL_ASSET_FILES})
  file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/resources/oracool_assets" "${_asset}")
  string(APPEND _oracool_rel_list "${_rel}\n")
endforeach()
file(GENERATE OUTPUT "${ORACOOL_MPQ_LIST}" CONTENT "${_oracool_rel_list}")
add_custom_command(
  COMMENT "Packing oracool.mpq"
  OUTPUT "${ORACOOL_MPQ}"
  COMMAND oracool_mpq_pack
    "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/resources/oracool_assets"
    "${ORACOOL_MPQ}"
    "@${ORACOOL_MPQ_LIST}"
  DEPENDS oracool_mpq_pack ${ORACOOL_ASSET_FILES} "${ORACOOL_MPQ_LIST}"
  VERBATIM)
add_custom_target(oracool_mpq DEPENDS "${ORACOOL_MPQ}")
add_dependencies(${BIN_TARGET} oracool_mpq)

# Oracool: exports a class's player sprites out of the archives as PNG sheets. Also EXCLUDE_FROM_ALL -
# it is an inspection tool, not part of the game. See the file header for how to run it.
add_executable(oracool_sprite_export EXCLUDE_FROM_ALL tools/oracool_sprite_export.cpp)
target_link_libraries(oracool_sprite_export PRIVATE libdevilutionx)
endif()

if(GPERF)
  target_link_libraries(${BIN_TARGET} PUBLIC ${GPERFTOOLS_LIBRARIES})
endif()

# Must be included after `BIN_TARGET` and `libdevilutionx` are defined.
include(Assets)

# DEVILUTIONX.MPQ FROM THE MANIFEST, not from whatever is lying in the build tree (external audit of
# v1.9.92, finding 6).
#
# tools\build_devilutionx_mpq.cmd walked `build\...\assets` recursively. That directory is only ever
# ADDED to - CMake copies each current manifest entry into it and never removes one that has left the
# manifest - so an asset deleted from `devilutionx_assets` stayed on disk and was packed into the
# next archive. A clean tree and an incremental tree could produce different archives from the same
# commit, and the same removal I made earlier today (70 stray files) is exactly the shape that
# resurrects. The audit measured it as latent rather than live: 188 expected, 188 present, no
# difference - clean today, not hermetic.
#
# `devilutionx_assets` is the authority, so the archive is built from that list directly. Skipped
# when BUILD_ASSETS_MPQ is on, which means smpq exists and upstream's own rule already owns this
# file - two producers for one output is worse than either.
if(NOT BUILD_ASSETS_MPQ AND NOT UWP_LIB AND NOT EMSCRIPTEN)
  set(ORACOOL_DVLX_MPQ "${CMAKE_CURRENT_BINARY_DIR}/devilutionx.mpq")
  set(ORACOOL_DVLX_LIST "${CMAKE_CURRENT_BINARY_DIR}/devilutionx_mpq_files.txt")
  set(_dvlx_rel_list "")
  set(_dvlx_inputs "")
  foreach(_asset ${devilutionx_assets})
    string(APPEND _dvlx_rel_list "${_asset}\n")
    list(APPEND _dvlx_inputs "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}/${_asset}")
  endforeach()
  file(GENERATE OUTPUT "${ORACOOL_DVLX_LIST}" CONTENT "${_dvlx_rel_list}")
  add_custom_command(
    COMMENT "Packing devilutionx.mpq"
    OUTPUT "${ORACOOL_DVLX_MPQ}"
    COMMAND oracool_mpq_pack
      "${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}"
      "${ORACOOL_DVLX_MPQ}"
      "@${ORACOOL_DVLX_LIST}"
    DEPENDS oracool_mpq_pack ${_dvlx_inputs} "${ORACOOL_DVLX_LIST}"
    VERBATIM)
  add_custom_target(oracool_devilutionx_mpq DEPENDS "${ORACOOL_DVLX_MPQ}")
  add_dependencies(${BIN_TARGET} oracool_devilutionx_mpq)
endif()

if(EMSCRIPTEN)
  target_link_options(${BIN_TARGET} PRIVATE --preload-file assets)
endif()

if(NOT USE_SDL1 AND NOT UWP_LIB)
  target_link_libraries(${BIN_TARGET} PUBLIC ${SDL2_MAIN})
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

include(functions/set_relative_file_macro)
set_relative_file_macro(${BIN_TARGET})

if(APPLE)
  set(MACOSX_BUNDLE_GUI_IDENTIFIER com.diasurgical.devilutionx)
  set(MACOSX_BUNDLE_COPYRIGHT Unlicense)
  set(MACOSX_BUNDLE_BUNDLE_NAME devilutionx)
  set(MACOSX_BUNDLE_DISPLAY_NAME DevilutionX)
  set(MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")
  if(IOS)
    set(MACOSX_BUNDLE_REQUIRED_PLATFORM IPhoneOS)
    set_target_properties(${BIN_TARGET} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "9.0")
  elseif(DARWIN_MAJOR_VERSION GREATER_EQUAL 16)
    set(MACOSX_BUNDLE_REQUIRED_PLATFORM Carbon)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12.0")
  endif()
  if(DARWIN_MAJOR_VERSION VERSION_LESS 9)
    # Finder on OSX Tiger can only handle icns files with up to 128x128 icons.
    set(_icon_file AppIcon_128)
  else()
    set(_icon_file AppIcon)
  endif()
  target_sources(${BIN_TARGET} PRIVATE "Packaging/apple/${_icon_file}.icns")
  set_source_files_properties("./Packaging/apple/${_icon_file}.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(${BIN_TARGET} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_icon_file}.icns")
  set_target_properties(${BIN_TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/apple/Info.plist")

  install (TARGETS ${BIN_TARGET} DESTINATION ./)

  if(MACOSX_STANDALONE_APP_BUNDLE)
      install(CODE "
        include(BundleUtilities)
        fixup_bundle(${CMAKE_BINARY_DIR}/${MACOSX_BUNDLE_BUNDLE_NAME}.app \"\" \"\")
        "
        COMPONENT Runtime)
  endif()

  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  if(COREFOUNDATION_LIBRARY)
    target_link_libraries(libdevilutionx PUBLIC "${COREFOUNDATION_LIBRARY}")
    target_compile_definitions(libdevilutionx PRIVATE USE_COREFOUNDATION)
  endif()

  set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")
  set(CPACK On)
endif()

if(NINTENDO_SWITCH)
  nx_generate_nacp (${BIN_TARGET}.nacp
    NAME    "DevilutionX"
    AUTHOR  "Devilution Team"
    VERSION "${PROJECT_VERSION}"
  )

  nx_create_nro(${BIN_TARGET}
    NACP  ${BIN_TARGET}.nacp
    ICON  "${PROJECT_SOURCE_DIR}/Packaging/switch/icon.jpg"
    ROMFS ${DEVILUTIONX_ASSETS_OUTPUT_DIRECTORY}
  )
endif()

if(VITA)
  set(VITA_APP_NAME "devilutionX")
  set(VITA_TITLEID  "DVLX00001")
  set(VITA_VERSION  "01.00")
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")
  vita_create_self(devilutionx.self devilutionx UNSAFE)
  if(BUILD_ASSETS_MPQ OR SRC_DIST)
    vita_create_vpk(devilutionx.vpk ${VITA_TITLEID} devilutionx.self
      VERSION ${VITA_VERSION}
      NAME ${VITA_APP_NAME}
      FILE Packaging/vita/sce_sys sce_sys
      FILE ${DEVILUTIONX_MPQ} devilutionx.mpq
    )
  else()
    vita_create_vpk(devilutionx.vpk ${VITA_TITLEID} devilutionx.self
      VERSION ${VITA_VERSION}
      NAME ${VITA_APP_NAME}
      FILE Packaging/vita/sce_sys sce_sys
      FILE Packaging/resources/assets assets
      ${VITA_TRANSLATIONS_LIST}
    )
  endif()

endif()

if(PS4)
  add_custom_command(
    TARGET devilutionx_mpq POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DEVILUTIONX_MPQ} "${PROJECT_SOURCE_DIR}/Packaging/ps4/")
  add_self(${BIN_TARGET})
  add_pkg(${BIN_TARGET} "${PROJECT_SOURCE_DIR}/Packaging/ps4"
    "DVLX00001" "DevilutionX" "${PROJECT_VERSION}")
endif()

if(NINTENDO_3DS)
  set(APP_TITLE       "DevilutionX")
  set(APP_DESCRIPTION "DevilutionX port for 3DS")
  set(APP_AUTHOR      "Diasurgical Team")
  set(APP_ICON        "${PROJECT_SOURCE_DIR}/Packaging/ctr/icon.png")
  set(APP_BANNER      "${PROJECT_SOURCE_DIR}/Packaging/ctr/banner.png")
  set(APP_AUDIO       "${CMAKE_BINARY_DIR}/banner_audio.wav")
  set(APP_RSF         "${PROJECT_SOURCE_DIR}/Packaging/ctr/template.rsf")
  set(APP_VERSION ${PROJECT_VERSION})

  find_program(FFMPEG ffmpeg)
  if(FFMPEG)
    add_custom_command(OUTPUT ${APP_AUDIO}
      COMMAND ${FFMPEG} -y -ss 3.3 -t 3 -i "${PROJECT_SOURCE_DIR}/Packaging/resources/shareware-startup.wav" -af "afade=t=in:st=0:d=0.1,afade=t=out:st=2.9:d=0.1" ${APP_AUDIO}
      DEPENDS ${PROJECT_SOURCE_DIR}/Packaging/resources/shareware-startup.wav
      VERBATIM)
  else()
    add_custom_command(OUTPUT ${APP_AUDIO}
      COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Packaging/ctr/audio_silent.wav ${APP_AUDIO}
      DEPENDS ${PROJECT_SOURCE_DIR}/Packaging/ctr/audio_silent.wav
      VERBATIM)
  endif()

  include(Tools3DS)
  add_3dsx_target(${BIN_TARGET})
  add_cia_target(${BIN_TARGET} ${APP_RSF} ${APP_BANNER} ${APP_AUDIO})
endif()

if(UWP_LIB)
  target_compile_definitions(libdevilutionx PRIVATE __UWP__=1)
endif()

if(NXDK)
  target_link_libraries(${BIN_TARGET} PRIVATE "${NXDK_DIR}/lib/libnxdk_automount_d.lib")
  target_link_options(${BIN_TARGET} PRIVATE "-include:_automount_d_drive")

  set(_nxdk_pkg_dir "${CMAKE_BINARY_DIR}/pkg")
  set(_xbe_path "${_nxdk_pkg_dir}/default.xbe")
  add_custom_command(
    OUTPUT "${_xbe_path}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_nxdk_pkg_dir}"
    COMMAND "${NXDK_DIR}/tools/cxbe/cxbe" "-OUT:${_xbe_path}" -TITLE:DevilutionX "-Logo:${PROJECT_SOURCE_DIR}/Packaging/xbox_nxdk/xbe_logo.pgm" "${CMAKE_BINARY_DIR}/${BIN_TARGET}.exe"
    DEPENDS "${BIN_TARGET}"
  )
  add_custom_target(nxdk_xbe DEPENDS "${_xbe_path}")
endif()

if(CPACK AND (APPLE OR BUILD_ASSETS_MPQ OR SRC_DIST))
  if(WIN32)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      set(SDL2_WIN32_DLLS_DIR "${CMAKE_BINARY_DIR}")
    else()
      set(SDL2_WIN32_DLLS_DIR "${SDL2_EXEC_PREFIX}")
    endif()
    set(SDL2_WIN32_LICENSES_DIR "${PROJECT_SOURCE_DIR}/Packaging/resources")

    file(GLOB SDL2_WIN32_ALL_DLLS
      LIST_DIRECTORIES false
      "${SDL2_WIN32_DLLS_DIR}/*.dll")
    file(GLOB SDL2_WIN32_ALL_LICENSES
      LIST_DIRECTORIES false
      "${SDL2_WIN32_LICENSES_DIR}/LICENSE*.txt"
      "${SDL2_WIN32_LICENSES_DIR}/README*.txt")

    set(CPACK_PACKAGE_FILE_NAME "devilutionx")
    set(CPACK_PACKAGE_NAME ${project_name})
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_STRIP_FILES TRUE)
    install(TARGETS ${BIN_TARGET} DESTINATION .)
    # Oracool: RELEASE_README.txt, not the upstream README.txt. The generic one describes
    # DevilutionX and names only diabdat.mpq; ours names all seven archives, says which four go
    # together, and warns that supplying hellfire.mpq without hfmonk/hfmusic/hfvoice makes the game
    # exit on start. A package that shipped the other one walked players into that (external audit,
    # 2026-08-25).
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/windows/RELEASE_README.txt"
      DESTINATION "."
      RENAME "README.txt"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/_ProjectLibrary/Documentation/CHANGELOG.md"
      DESTINATION "."
      RENAME "CHANGELOG.txt"
    )
    install(FILES "${DEVILUTIONX_MPQ}"
      DESTINATION "."
    )
    # Oracool: the fork's own asset archive, without which most of the game's UI and content is
    # missing.
    #
    # This was OPTIONAL, and that was the bug. The 2026-08-17 audit found that a package could omit
    # it entirely; the mitigation was to install it "if present", which means a clean CI or
    # release build - exactly the one nobody watches - produces a silently gutted game. The
    # 2026-08-25 audit raised it again for that reason, and it was raised a third time by
    # experience: the v1.9.31 release zip was assembled by hand, and oracool.mpq had to be copied
    # out of the Debug directory because no packaging step knew about it.
    #
    # Required now, with a message that says what to do. It fires at install/package time only, so
    # an ordinary build is unaffected - the archive is still produced by tools/build_oracool_mpq.cmd
    # rather than by the build, which is a separate thing worth fixing and not this.
    #
    # FORWARD slashes in the message, and not as a style choice. The path crosses two levels of
    # CMake string escaping on its way into cmake_install.cmake, and a backslash does not survive
    # it: `tools\\\\build...` here reaches the generated script as `\b` and aborts the install with
    # "Invalid character escape". Verified by running both branches of this guard in a throwaway
    # project, because it can only fail at package time - which is the one time nobody wants a
    # surprise.
    install(CODE "
      if(NOT EXISTS \"${CMAKE_BINARY_DIR}/oracool.mpq\")
        message(FATAL_ERROR
          \"oracool.mpq is missing from the build directory, so this package would ship without \"
          \"Oracool Edition's own art and data - most of the UI and content. Build it first:\\n\"
          \"    tools/build_oracool_mpq.cmd\\n\"
          \"(the packer is EXCLUDE_FROM_ALL; build the oracool_mpq_pack target once if it is absent)\")
      endif()
    ")
    install(FILES "${CMAKE_BINARY_DIR}/oracool.mpq"
      DESTINATION "."
    )

    foreach(_SDL2_WIN32_DLL_PATH ${SDL2_WIN32_ALL_DLLS})
      install(FILES "${_SDL2_WIN32_DLL_PATH}"
        DESTINATION "."
      )
    endforeach()

    foreach(_SDL2_WIN32_LICENSE_PATH ${SDL2_WIN32_ALL_LICENSES})
      install(FILES "${_SDL2_WIN32_LICENSE_PATH}"
        DESTINATION "LICENSE"
      )
    endforeach()

    if(DISCORD_SHARED_LIB)
      install(FILES "${DISCORD_SHARED_LIB}"
        DESTINATION "."
      )
    endif()

  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(TOLOWER ${PROJECT_NAME} project_name)
    set(CPACK_PACKAGE_NAME ${project_name})
    set(CPACK_GENERATOR "7Z")
    # Common *nix files
    set(CPACK_STRIP_FILES TRUE)
    install(TARGETS ${BIN_TARGET} DESTINATION bin)
    set(desktop_file "${PROJECT_SOURCE_DIR}/Packaging/nix/${project_name}.desktop")
    set(desktop_file_hellfire "${PROJECT_SOURCE_DIR}/Packaging/nix/${project_name}-hellfire.desktop")

    find_program(DFI desktop-file-install)
    if(DFI)
      execute_process(COMMAND ${DFI} --dir=${CMAKE_BINARY_DIR} ${desktop_file})
      set(desktop_file "${CMAKE_BINARY_DIR}/${project_name}.desktop")
      execute_process(COMMAND ${DFI} --dir=${CMAKE_BINARY_DIR} ${desktop_file_hellfire})
      set(desktop_file_hellfire "${CMAKE_BINARY_DIR}/${project_name}-hellfire.desktop")
    endif()

    install(FILES "${desktop_file}"
      DESTINATION "share/applications"
    )
    install(FILES "${desktop_file_hellfire}"
      DESTINATION "share/applications"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/nix/README.txt"
      DESTINATION "share/diasurgical/${project_name}"
    )
    install(FILES "${DEVILUTIONX_MPQ}"
      DESTINATION "share/diasurgical/${project_name}"
    )
    # Oracool: same reasoning as the Windows block, and now the same RULE. This said "same
    # reasoning" while doing the opposite - it kept the OPTIONAL that was removed there, so a Linux
    # package would still silently ship without the fork's art and data (audit, 2026-08-26).
    #
    # "When built" is exactly the phrasing that hides the bug: the archive is produced by a script
    # nobody runs on a clean machine, so "when built" means "usually not".
    install(CODE "
      if(NOT EXISTS \"${CMAKE_BINARY_DIR}/oracool.mpq\")
        message(FATAL_ERROR
          \"oracool.mpq is missing from the build directory, so this package would ship without \"
          \"Oracool Edition's own art and data - most of the UI and content. Build it first:\\n\"
          \"    tools/build_oracool_mpq.cmd\\n\"
          \"(the packer is EXCLUDE_FROM_ALL; build the oracool_mpq_pack target once if it is absent)\")
      endif()
    ")
    install(FILES "${CMAKE_BINARY_DIR}/oracool.mpq"
      DESTINATION "share/diasurgical/${project_name}"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/resources/icon_flat.png"
      DESTINATION "share/icons/hicolor/512x512/apps"
      RENAME "${project_name}.png"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/resources/hellfire.png"
      DESTINATION "share/icons/hicolor/512x512/apps"
      RENAME "${project_name}-hellfire.png"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Packaging/nix/devilutionx.metainfo.xml"
      DESTINATION "share/metainfo"
      RENAME "${project_name}.metainfo.xml"
    )
    if(DISCORD_SHARED_LIB)
      install(FILES "${DISCORD_SHARED_LIB}" DESTINATION "lib")
    endif()

    # -G DEB
    set(CPACK_PACKAGE_CONTACT "anders@jenbo.dk")
    if(USE_SDL1)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl1.2debian")
    else()
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-image-2.0-0")
    endif()
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # -G RPM
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

    find_program(RPMBUILD rpmbuild)
    if(RPMBUILD)
      list(APPEND CPACK_GENERATOR "RPM")
    endif()
    find_program(DPKG dpkg)
    if(DPKG)
      list(APPEND CPACK_GENERATOR "DEB")
    endif()

  elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "devilutionx")
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK "ON")
    set(CPACK_STRIP_FILES TRUE)
    set(CPACK_GENERATOR "DragNDrop")
  endif()

  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  include(CPack)
endif()
