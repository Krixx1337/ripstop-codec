if(NOT COMMAND CPMAddPackage)
  set(RIPSTOP_CPM_VERSION 0.43.1)
  set(RIPSTOP_CPM_SHA256 1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934)

  if(CPM_SOURCE_CACHE)
    set(_ripstop_cpm_dir "${CPM_SOURCE_CACHE}/cpm")
  elseif(DEFINED ENV{CPM_SOURCE_CACHE} AND NOT "$ENV{CPM_SOURCE_CACHE}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{CPM_SOURCE_CACHE}" _ripstop_cpm_source_cache)
    set(_ripstop_cpm_dir "${_ripstop_cpm_source_cache}/cpm")
  else()
    set(_ripstop_cpm_dir "${CMAKE_BINARY_DIR}/cmake")
  endif()

  get_filename_component(_ripstop_cpm_dir "${_ripstop_cpm_dir}" ABSOLUTE)
  set(_ripstop_cpm_file "${_ripstop_cpm_dir}/CPM_${RIPSTOP_CPM_VERSION}.cmake")
  set(_ripstop_download_cpm YES)

  if(EXISTS "${_ripstop_cpm_file}")
    file(SHA256 "${_ripstop_cpm_file}" _ripstop_cpm_existing_sha256)
    if(_ripstop_cpm_existing_sha256 STREQUAL RIPSTOP_CPM_SHA256)
      set(_ripstop_download_cpm NO)
    endif()
  endif()

  if(_ripstop_download_cpm)
    file(MAKE_DIRECTORY "${_ripstop_cpm_dir}")
    file(
      DOWNLOAD
      "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${RIPSTOP_CPM_VERSION}/CPM.cmake"
      "${_ripstop_cpm_file}"
      EXPECTED_HASH "SHA256=${RIPSTOP_CPM_SHA256}"
      TLS_VERIFY ON
      STATUS _ripstop_cpm_download_status
    )

    list(GET _ripstop_cpm_download_status 0 _ripstop_cpm_download_code)
    list(GET _ripstop_cpm_download_status 1 _ripstop_cpm_download_message)
    if(NOT _ripstop_cpm_download_code EQUAL 0)
      message(FATAL_ERROR "Failed to download CPM.cmake: ${_ripstop_cpm_download_message}")
    endif()
  endif()

  include("${_ripstop_cpm_file}")
endif()

CPMAddPackage(
  NAME ripstop_miniz
  VERSION 3.1.2
  GITHUB_REPOSITORY richgel999/miniz
  GIT_TAG 3.1.2
  DOWNLOAD_ONLY YES
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

set(
  RIPSTOP_MINIZ_SOURCES
  "${ripstop_miniz_SOURCE_DIR}/miniz.c"
  "${ripstop_miniz_SOURCE_DIR}/miniz_tdef.c"
  "${ripstop_miniz_SOURCE_DIR}/miniz_tinfl.c"
)

set(
  RIPSTOP_MINIZ_INCLUDE_DIRS
  "${ripstop_miniz_SOURCE_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/miniz"
)

if(RIPSTOP_BUILD_TESTS)
  CPMAddPackage(
    NAME doctest
    VERSION 2.5.3
    GITHUB_REPOSITORY doctest/doctest
    GIT_TAG v2.5.3
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
      "DOCTEST_WITH_TESTS OFF"
      "DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF"
      "DOCTEST_NO_INSTALL ON"
  )
endif()
