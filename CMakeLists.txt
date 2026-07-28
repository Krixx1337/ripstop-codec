cmake_minimum_required(VERSION 3.23)

if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(
    CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>"
  )
endif()

project(
  ripstop-codec
  VERSION 1.1.0
  DESCRIPTION "Tear-resistant asset protection for modern games and apps"
  LANGUAGES C CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(PROJECT_IS_TOP_LEVEL)
  set(RIPSTOP_BUILD_TESTS_DEFAULT ON)
else()
  set(RIPSTOP_BUILD_TESTS_DEFAULT OFF)
endif()

option(RIPSTOP_BUILD_TESTS "Build RipStop Codec tests" ${RIPSTOP_BUILD_TESTS_DEFAULT})
option(RIPSTOP_BUILD_EXAMPLES "Build RipStop Codec examples" OFF)
option(RIPSTOP_BUILD_FUZZER "Build the Clang libFuzzer decode target" OFF)

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
      include/ripstop/detail/constexpr_obfuscation.h
      include/ripstop/detail/memory_hygiene.h
)

target_include_directories(
  ripstop-codec
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_features(ripstop-codec PUBLIC cxx_std_20)

target_compile_definitions(
  ripstop-codec
  PUBLIC
    RIPSTOP_CODEC_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    RIPSTOP_CODEC_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    RIPSTOP_CODEC_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    RIPSTOP_CODEC_VERSION_STRING="${PROJECT_VERSION}"
)

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
    EXPORT_NAME ripstop-codec
)

install(
  TARGETS ripstop-codec
  EXPORT ripstop-codec-targets
  FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(RIPSTOP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/RipStopCodec")

configure_package_config_file(
  cmake/RipStopCodecConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/RipStopCodecConfig.cmake"
  INSTALL_DESTINATION "${RIPSTOP_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/RipStopCodecConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  EXPORT ripstop-codec-targets
  FILE RipStopCodecTargets.cmake
  NAMESPACE RipStopCodec::
  DESTINATION "${RIPSTOP_INSTALL_CMAKEDIR}"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/RipStopCodecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/RipStopCodecConfigVersion.cmake"
  DESTINATION "${RIPSTOP_INSTALL_CMAKEDIR}"
)

install(
  FILES templates/RipStop_Config.example.h
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ripstop-codec/templates
)

install(
  FILES LICENSE NOTICE README.md INSTALL.md CHANGELOG.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ripstop-codec
)

install(
  FILES docs/SPEC.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ripstop-codec/docs
)

if(RIPSTOP_BUILD_TESTS)
  enable_testing()

  add_executable(ripstop-codec-tests tests/test_ripstop.cpp)
  target_link_libraries(ripstop-codec-tests PRIVATE RipStopCodec::ripstop-codec)
  target_include_directories(ripstop-codec-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
  add_test(NAME ripstop-codec-tests COMMAND ripstop-codec-tests)

  add_executable(ripstop-codec-config-template-smoke tests/config_template_smoke.cpp)
  target_link_libraries(ripstop-codec-config-template-smoke PRIVATE RipStopCodec::ripstop-codec)
  target_include_directories(
    ripstop-codec-config-template-smoke
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/templates
  )
  add_test(NAME ripstop-codec-config-template-smoke COMMAND ripstop-codec-config-template-smoke)
endif()

if(RIPSTOP_BUILD_EXAMPLES)
  add_executable(ripstop-codec-basic-example examples/basic.cpp)
  target_link_libraries(ripstop-codec-basic-example PRIVATE RipStopCodec::ripstop-codec)

  add_executable(ripstop-codec-custom-scrambler-example examples/custom_scrambler.cpp)
  target_link_libraries(ripstop-codec-custom-scrambler-example PRIVATE RipStopCodec::ripstop-codec)
endif()

if(RIPSTOP_BUILD_FUZZER)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "RIPSTOP_BUILD_FUZZER requires Clang with libFuzzer")
  endif()

  target_compile_options(ripstop-codec PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
  add_executable(ripstop-codec-fuzz-decode tests/fuzz_decode.cpp)
  target_link_libraries(ripstop-codec-fuzz-decode PRIVATE RipStopCodec::ripstop-codec)
  target_compile_options(ripstop-codec-fuzz-decode PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(ripstop-codec-fuzz-decode PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
