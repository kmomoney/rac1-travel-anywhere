# SPDX-License-Identifier: GPL-3.0-or-later
# rac1-travel-anywhere (README.md): the ship's planet select from anywhere in Ratchet & Clank on
# OpenRAC's native port. The top of the repository, from the SDK's template (sdk/template), with the
# tests.
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build && ctest --test-dir build --output-on-failure
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rac1_travel_anywhere -o dist
cmake_minimum_required(VERSION 3.20)
project(rac1_travel_anywhere LANGUAGES C CXX)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  else()
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    # Stop early, saying how to get the SDK, when it has not been fetched.
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
# The SDK provides openrac_add_mod and the OpenRAC::mod target the tests link against.
find_package(OpenRACMod 1.0 REQUIRED)
include(CTest)

# The mod's library and the manifest it is packaged with.
openrac_add_mod(rac1_travel_anywhere MANIFEST openrac-mod.json
                SOURCES src/mod.cpp src/rac1/game.cpp)
# The sources include version.h and rac1/... from src/.
target_include_directories(rac1_travel_anywhere PRIVATE src)
# Warnings on.
if(MSVC)
  target_compile_options(rac1_travel_anywhere PRIVATE /W4)
else()
  target_compile_options(rac1_travel_anywhere PRIVATE -Wall -Wextra)
endif()

# The tests build only when testing is on (CTest's BUILD_TESTING).
if(BUILD_TESTING)
  # The decisions alone (src/rac1/travel.h): plain C, no game, no SDK.
  add_executable(rac1_travel_anywhere_travel_test tests/travel_test.c)
  target_include_directories(rac1_travel_anywhere_travel_test PRIVATE src tests)
  set_target_properties(rac1_travel_anywhere_travel_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
  # Warnings on.
  if(NOT MSVC)
    target_compile_options(rac1_travel_anywhere_travel_test PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME rac1_travel_anywhere_travel COMMAND rac1_travel_anywhere_travel_test)

  # The library as the port loads it, against a host table of the test's own, on both discs' rows.
  add_executable(rac1_travel_anywhere_load_test tests/load_test.c)
  target_include_directories(rac1_travel_anywhere_load_test PRIVATE src tests)
  set_target_properties(rac1_travel_anywhere_load_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
  target_link_libraries(rac1_travel_anywhere_load_test PRIVATE OpenRAC::mod ${CMAKE_DL_LIBS})
  add_dependencies(rac1_travel_anywhere_load_test rac1_travel_anywhere)
  add_test(NAME rac1_travel_anywhere_load COMMAND rac1_travel_anywhere_load_test $<TARGET_FILE:rac1_travel_anywhere>)

  # The manifest against the code: run when Python is found.
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME rac1_travel_anywhere_manifest
             COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/manifest_test.py)
  endif()
endif()
