cmake_minimum_required(VERSION 3.5)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif(POLICY CMP0091)

set(QTADS_VERSION_MAJOR 4)
set(QTADS_VERSION_MINOR 3)
set(QTADS_VERSION_PATCH 0)
set(QTADS_VERSION
    "${QTADS_VERSION_MAJOR}.${QTADS_VERSION_MINOR}.${QTADS_VERSION_PATCH}")

project(
  QtADS
  LANGUAGES CXX
  VERSION ${QTADS_VERSION})

option(BUILD_STATIC "Build the static library" OFF)
option(BUILD_EXAMPLES "Build the examples" OFF)

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  set(ads_PlatformDir "x86")
else()
  set(ads_PlatformDir "x64")
endif()

add_subdirectory(src)

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
  add_subdirectory(demo)
endif()
