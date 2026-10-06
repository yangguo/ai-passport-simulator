include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.5.3)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0)
FetchContent_Declare(SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.32.10)
FetchContent_Declare(lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG v9.5.0)
# stb has no CMakeLists; headers are fetched via file(DOWNLOAD) in
# cmake/FetchStb.cmake pinned to the same commit.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_DIR ${CMAKE_SOURCE_DIR}/config CACHE PATH "LVGL config dir" FORCE)
FetchContent_MakeAvailable(doctest nlohmann_json SDL2 lvgl)
