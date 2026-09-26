include(GoogleTest)

add_library(libdevilutionx_so SHARED)
set_target_properties(libdevilutionx_so PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(libdevilutionx_so PUBLIC libdevilutionx)
target_include_directories(libdevilutionx_so INTERFACE "${PROJECT_SOURCE_DIR}/Source")
set_target_properties(libdevilutionx_so PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(test_main STATIC main.cpp)
target_link_libraries(test_main PUBLIC libdevilutionx_so GTest::gtest GTest::gmock)

set(tests
  animationinfo_test
  appfat_test
  automap_test
  codec_test
  cursor_test
  dead_test
  diablo_test
  drlg_common_test
  drlg_l1_test
  drlg_l2_test
  drlg_l3_test
  drlg_l4_test
  effects_test
  file_util_test
  format_int_test
  inv_test
  items_test
  lighting_test
  loadsave_test
  math_test
  missiles_test
  oracool_audit_test
  oracool_rage_test
  oracool_essence_test
  oracool_minions_test
  oracool_necro_items_test
  oracool_passive_sweep_test
  oracool_skill_rules_test
  oracool_skill_assets_test
  oracool_town_objects_test
  oracool_hero_look_test
  oracool_tooltip_sweep_test
  pack_test
  path_test
  player_test
  quests_test
  random_test
  rectangle_test
  scrollrt_test
  stores_test
  str_cat_test
  timedemo_test
  utf8_test
  writehero_test
)

include(Fixtures.cmake)

foreach(test_target ${tests})
  add_executable(${test_target} "${test_target}.cpp")
  # Oracool: 30 s rather than CMake's 5 s. On 2026-09-19 a full rebuild had twelve binaries time out
  # listing their tests while the linker saturated the CPU, and every one listed in 0.07 s alone.
  gtest_discover_tests(${test_target} DISCOVERY_TIMEOUT 30)
  # Oracool: the whole-binary SHUFFLE lane (external audit, 2026-09-06). gtest_discover_tests runs
  # every case in its own process, which hides state that leaks between cases in one executable -
  # 17 crashes and four wrong answers were hiding that way. This runs each binary whole, in a random
  # order, three times; gtest prints the seed, so a failure is reproducible with
  # --gtest_random_seed. Select it alone with: ctest -L shuffle
  add_test(NAME ${test_target}_shuffled COMMAND ${test_target} --gtest_shuffle --gtest_repeat=3 --gtest_brief=1)
  set_tests_properties(${test_target}_shuffled PROPERTIES LABELS "shuffle" TIMEOUT 600)
  target_link_libraries(${test_target} PRIVATE test_main)
  set_target_properties(${test_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  if(GPERF)
    target_link_libraries(${test_target} PUBLIC ${GPERFTOOLS_LIBRARIES})
  endif()
endforeach()

target_include_directories(writehero_test PRIVATE ../3rdParty/PicoSHA2)
