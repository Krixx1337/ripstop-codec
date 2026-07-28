cmake_minimum_required(VERSION 3.23)
project(ripstop-subdirectory-smoke LANGUAGES C CXX)

include(CTest)
add_subdirectory(../.. ripstop-codec)

if(TARGET ripstop-codec-tests)
  message(FATAL_ERROR "RipStop tests must remain disabled when consumed as a subdirectory")
endif()

add_executable(ripstop-subdirectory-smoke ../package_smoke/main.cpp)
target_link_libraries(ripstop-subdirectory-smoke PRIVATE RipStopCodec::ripstop-codec)
