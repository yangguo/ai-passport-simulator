function(add_passport_test name)
  add_executable(${name} test_main.cc ${ARGN})
  target_link_libraries(${name} PRIVATE doctest::doctest)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_passport_test(test_smoke test_smoke.cc)
add_passport_test(test_geometry test_geometry.cc)
target_link_libraries(test_geometry PRIVATE geometry)
