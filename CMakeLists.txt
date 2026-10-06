cmake_minimum_required(VERSION 3.28)
project(ai_passport_simulator LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/FetchDeps.cmake)
include(cmake/FetchStb.cmake)
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
target_link_libraries(sim_core PUBLIC caption nlohmann_json::nlohmann_json transport audio_mock)
add_library(transport STATIC src/transport_mock.cc)
target_include_directories(transport PUBLIC include)
add_library(audio_mock STATIC src/audio_pipeline_mock.cc)
target_include_directories(audio_mock PUBLIC include)
add_library(lvglui STATIC src/lvgl_shell.cc src/screenshot_png.cc)
target_include_directories(lvglui PUBLIC include)
target_link_libraries(lvglui PUBLIC layout caption lvgl stb)
add_subdirectory(app)
add_subdirectory(tools)
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

# Golden screenshot regression: replay a fixture, render the checkpoint, diff.
function(add_golden_test name fixture golden at_ms alert)
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND} -DREPLAY_BIN=$<TARGET_FILE:passport-replay>
      -DCOMPARE_BIN=$<TARGET_FILE:compare-png>
      -DBIN_DIR=${CMAKE_BINARY_DIR} -DSRC_DIR=${CMAKE_SOURCE_DIR}
      -DFIXTURE=${fixture} -DGOLDEN=${golden} -DAT_MS=${at_ms}
      -DALERT=${alert}
      -P ${CMAKE_SOURCE_DIR}/tests/golden_check.cmake)
endfunction()
add_golden_test(golden_idle ptt-normal idle 3800 "")
add_golden_test(golden_listening ptt-normal listening 450 "")
add_golden_test(golden_thinking ptt-normal thinking 700 "")
add_golden_test(golden_speaking_long long-mixed-text-paging speaking-long -1 "")
add_golden_test(golden_menu menu-open-close menu -1 "")
add_golden_test(golden_low_battery missing-battery-low-alert low-battery -1 "LOW BATTERY")
