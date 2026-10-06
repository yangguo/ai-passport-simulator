add_executable(passport-replay replay.cc)
target_link_libraries(passport-replay PRIVATE sim_core)
add_executable(passport-simulator main.cc)
target_link_libraries(passport-simulator PRIVATE sim_core layout SDL2::SDL2 SDL2::SDL2main)
