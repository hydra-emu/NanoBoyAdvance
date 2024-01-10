cmake_minimum_required(VERSION 3.11...3.28)
project(NanoBoyAdvance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PLATFORM_QT "Build Qt frontend." ON)
option(PLATFORM_HYDRA "Build Hydra core" OFF)

if (PLATFORM_HYDRA)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  add_subdirectory(src/platform/hydra ${CMAKE_CURRENT_BINARY_DIR}/bin/hydra/)
endif()

add_subdirectory(src/nba)
add_subdirectory(src/platform/core)

if (PLATFORM_QT)
  add_subdirectory(src/platform/qt ${CMAKE_CURRENT_BINARY_DIR}/bin/qt/)
endif()
