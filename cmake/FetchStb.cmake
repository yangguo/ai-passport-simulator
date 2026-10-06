# Fetches stb single-header image libs pinned to a commit (no CMakeLists upstream).
set(STB_COMMIT 2c980bb59875b0d32144a71867fbdebb2f77cd20)
set(STB_DIR ${CMAKE_BINARY_DIR}/_stb)
file(MAKE_DIRECTORY ${STB_DIR})
foreach(header stb_image_write.h stb_image.h)
  if(NOT EXISTS ${STB_DIR}/${header})
    file(DOWNLOAD
      https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}/${header}
      ${STB_DIR}/${header} STATUS dl_status)
    list(GET dl_status 0 dl_code)
    if(NOT dl_code EQUAL 0)
      message(FATAL_ERROR "failed to download ${header}: ${dl_status}")
    endif()
  endif()
endforeach()
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${STB_DIR})
