cmake_minimum_required(VERSION 3.28)
project(ai_passport_simulator LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PASSPORT_WITH_LVGL "Build real LVGL/SDL rendering (Stage 2)" OFF)
include(cmake/FetchDeps.cmake)
enable_testing()
add_library(geometry STATIC shared/screen_rounding.c)
target_include_directories(geometry PUBLIC shared)
add_library(caption STATIC shared/caption_buffers.cc)
target_include_directories(caption PUBLIC shared)
add_library(layout STATIC shared/layout.cc)
target_include_directories(layout PUBLIC shared)
target_link_libraries(layout PUBLIC geometry)
add_library(sim_core STATIC
  src/virtual_clock.cc
  src/button_input.cc
  src/settings_store.cc
  src/scenario_runner.cc)
target_include_directories(sim_core PUBLIC include shared)
target_link_libraries(sim_core PUBLIC caption nlohmann_json::nlohmann_json)
add_subdirectory(app)
add_subdirectory(tests/unit)

add_test(NAME replay_ptt_log
  COMMAND ${CMAKE_COMMAND} -DREPLAY_BIN=$<TARGET_FILE:passport-replay>
    -DBIN_DIR=${CMAKE_BINARY_DIR} -DSRC_DIR=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/replay_check.cmake)
add_test(NAME shell_smoke
  COMMAND $<TARGET_FILE:passport-simulator>
    --scenario tests/scenarios/menu-open-close.json
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(shell_smoke PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy")
