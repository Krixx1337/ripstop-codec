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
  VERSION 1.0.1
  DESCRIPTION "Tear-resistant asset protection for modern games and apps"
  LANGUAGES C CXX
)

include(GNUInstallDirs)
include(CTest)

set(RIPSTOP_USER_CONFIG_HEADER "" CACHE STRING "Optional project-specific RipStop config header")
set(RIPSTOP_USER_CONFIG_INCLUDE_DIR "" CACHE PATH "Optional include directory for the project-specific RipStop config header during build-tree usage")

if(BUILD_TESTING AND NOT RIPSTOP_USER_CONFIG_HEADER)
  set(RIPSTOP_USER_CONFIG_HEADER "RipStop_TestConfig.h")
  set(RIPSTOP_USER_CONFIG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif()

if(RIPSTOP_USER_CONFIG_HEADER)
  if(RIPSTOP_USER_CONFIG_HEADER MATCHES "^[<\"].*[>\"]$")
    set(RIPSTOP_USER_CONFIG_HEADER_LITERAL "${RIPSTOP_USER_CONFIG_HEADER}")
  else()
    set(RIPSTOP_USER_CONFIG_HEADER_LITERAL "\"${RIPSTOP_USER_CONFIG_HEADER}\"")
  endif()
endif()

add_library(ripstop-codec STATIC)
add_library(RipStopCodec::ripstop-codec ALIAS ripstop-codec)

target_sources(
  ripstop-codec
  PRIVATE
    src/detail/memory_hygiene.cpp
    src/RipStop.cpp
    third_party/miniz/miniz.c
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
      include/ripstop/Codec.h
      include/ripstop/config_bridge.h
      include/ripstop/Error.h
      include/ripstop/Format.h
      include/ripstop/MemStream.h
      include/ripstop/detail/constexpr_obfuscation.h
      include/ripstop/detail/memory_hygiene.h
      include/ripstop/detail/polymorphic_error.h
)

target_include_directories(
  ripstop-codec
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(RIPSTOP_USER_CONFIG_INCLUDE_DIR)
  target_include_directories(
    ripstop-codec
    PUBLIC
      $<BUILD_INTERFACE:${RIPSTOP_USER_CONFIG_INCLUDE_DIR}>
  )
endif()

target_compile_features(ripstop-codec PUBLIC cxx_std_20)

if(RIPSTOP_USER_CONFIG_HEADER)
  target_compile_definitions(
    ripstop-codec
    PUBLIC
      RIPSTOP_USER_CONFIG_HEADER=${RIPSTOP_USER_CONFIG_HEADER_LITERAL}
  )
endif()

target_compile_definitions(
  ripstop-codec
  PRIVATE
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1
    HOSTILE_CORE_EXPORT=
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
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  )

  add_test(NAME ripstop-codec-tests COMMAND ripstop-codec-tests)
endif()
