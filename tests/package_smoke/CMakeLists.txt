cmake_minimum_required(VERSION 3.23)
project(ripstop-package-smoke LANGUAGES CXX)

find_package(RipStopCodec 1.1 CONFIG REQUIRED)

add_executable(ripstop-package-smoke main.cpp)
target_link_libraries(ripstop-package-smoke PRIVATE RipStopCodec::ripstop-codec)
