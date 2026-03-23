cmake_minimum_required(VERSION 3.23)

# Enable Hot Reload for MSVC compilers if supported.
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(
    CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>"
  )
endif()

project(
  ripstop-codec
  VERSION 1.0.0
  DESCRIPTION "Tear-resistant asset protection for modern games and apps"
  LANGUAGES C CXX
)

include(GNUInstallDirs)
include(CTest)

add_library(ripstop-codec STATIC)
add_library(RipStopCodec::ripstop-codec ALIAS ripstop-codec)

target_sources(
  ripstop-codec
  PRIVATE
    src/RipStop.cpp
    third_party/miniz/miniz.c
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
      include/ripstop/Codec.h
      include/ripstop/Error.h
      include/ripstop/Format.h
      include/ripstop/MemStream.h
)

target_include_directories(
  ripstop-codec
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)

target_compile_features(ripstop-codec PUBLIC cxx_std_20)
target_compile_definitions(
  ripstop-codec
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:_FILE_OFFSET_BITS=64>
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:_LARGEFILE64_SOURCE=1>
)

set_target_properties(
  ripstop-codec
  PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

install(
  TARGETS ripstop-codec
  FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  FILES templates/RipStop_Config.example.h
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ripstop-codec/templates
)

if(BUILD_TESTING)
  add_executable(
    ripstop-codec-tests
    tests/test_ripstop.cpp
  )

  target_link_libraries(ripstop-codec-tests PRIVATE ripstop-codec)

  target_include_directories(
    ripstop-codec-tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz/doctest
  )

  add_test(NAME ripstop-codec-tests COMMAND ripstop-codec-tests)
endif()
