cmake_minimum_required(VERSION 3.19)
project(belfry_bats VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BELFRY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BELFRY_BUILD_TESTS "Build the test suite" ON)
option(BELFRY_WEB "Build the browser game (Emscripten + Strata's WebGL 2 backend)" OFF)

# Emscripten: the rules and tests build as they are and the tests run under
# Node; BELFRY_WEB adds the browser game. Strata compiles C++ exceptions as
# wasm exceptions, and everything in one .wasm must match.
if(EMSCRIPTEN)
  cmake_minimum_required(VERSION 3.24)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fwasm-exceptions>")
  add_link_options(-fwasm-exceptions -sALLOW_MEMORY_GROWTH=1)
endif()
if(BELFRY_WEB AND NOT EMSCRIPTEN)
  message(FATAL_ERROR "BELFRY_WEB needs Emscripten (emcmake cmake ...)")
endif()

function(belfry_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(BELFRY_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(BELFRY_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# The night: the village, the flock, the owls, the dawn. No engine and no
# GPU, so it builds and tests in seconds. Multiply-adds are never fused, so
# a night plays out the same on every compiler.
add_library(belfry_sim STATIC src/sim/autopilot.cpp src/sim/level.cpp src/sim/world.cpp)
target_include_directories(belfry_sim PUBLIC src)
belfry_warnings(belfry_sim)
if(MSVC)
  target_compile_options(belfry_sim PRIVATE /fp:precise)
else()
  target_compile_options(belfry_sim PRIVATE -ffp-contract=off)
endif()

# Sound: what the night sounds like (from the world's events) and the synth
# that makes every sound from nothing. No device and no engine.
add_library(belfry_audio STATIC src/audio/cues.cpp src/audio/synth.cpp)
target_link_libraries(belfry_audio PUBLIC belfry_sim)
belfry_warnings(belfry_audio)

if(BELFRY_BUILD_TESTS)
  enable_testing()
  foreach(t level flock audio)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE belfry_sim belfry_audio)
    target_include_directories(test_${t} PRIVATE tests)
    belfry_warnings(test_${t})
    add_test(NAME ${t} COMMAND test_${t})
  endforeach()
endif()

# ---------------------------------------------------------------- the game on Strata
# Strata is fetched at BELFRY_STRATA_REF (the v0.7.0 tag: ABI 1.12), or taken
# from a local checkout with -DFETCHCONTENT_SOURCE_DIR_STRATA=<path>.
if(BELFRY_WEB)
  include(FetchContent)
  set(BELFRY_STRATA_REF "20827c3d67e71c1f942c29e7cf8adfa4801188a8" CACHE STRING "Strata git ref (a full commit SHA)")
  set(STRATA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(STRATA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(STRATA_WITH_SIM OFF CACHE BOOL "" FORCE)
  set(STRATA_WITH_USD OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(strata
    GIT_REPOSITORY https://github.com/Preponderous-Software/strata.git
    GIT_TAG ${BELFRY_STRATA_REF})
  FetchContent_MakeAvailable(strata)

  # The sound card, through miniaudio (public domain, third_party/miniaudio/):
  # Web Audio in a browser, unlocked by the first tap, click or key.
  find_package(Threads REQUIRED)
  add_library(belfry_miniaudio STATIC src/audio/miniaudio.cpp)
  target_include_directories(belfry_miniaudio SYSTEM PUBLIC third_party/miniaudio)
  target_link_libraries(belfry_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
  add_library(belfry_sound STATIC src/audio/device.cpp)
  target_link_libraries(belfry_sound PUBLIC belfry_audio PRIVATE belfry_miniaudio)
  belfry_warnings(belfry_sound)

  add_library(belfry_play STATIC src/play/meshes.cpp src/play/view.cpp)
  target_include_directories(belfry_play PUBLIC src)
  target_link_libraries(belfry_play PUBLIC belfry_sim belfry_sound strata::static)
  belfry_warnings(belfry_play)

  # The page is belfry-bats.html + .js + .wasm.
  add_executable(belfry-web app/web_main.cpp)
  target_link_libraries(belfry-web PRIVATE belfry_play)
  belfry_warnings(belfry-web)
  set_target_properties(belfry-web PROPERTIES OUTPUT_NAME belfry-bats SUFFIX ".html")
  target_link_options(belfry-web PRIVATE
    -sSTACK_SIZE=1MB
    "-sEXPORTED_FUNCTIONS=_main,_belfry_pointer,_belfry_start,_belfry_next,_belfry_restart,_belfry_status,_belfry_resize,_belfry_sound,_belfry_pause,_belfry_autopilot,_belfry_warp,_belfry_guide"
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
  set_property(TARGET belfry-web APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
  # The loading shell (web/strata-kit, vendored from Strata) and its bridge go
  # inline into the page, with the download's size (web/strata_page.cmake).
  set_property(TARGET belfry-web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/web/strata-kit/strata-loader.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/strata-boot.js")
  add_custom_command(TARGET belfry-web POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DHTML=$<TARGET_FILE:belfry-web>"
            "-DWASM=$<TARGET_FILE_DIR:belfry-web>/$<TARGET_FILE_BASE_NAME:belfry-web>.wasm"
            "-DWEB=${CMAKE_CURRENT_SOURCE_DIR}/web" -P "${CMAKE_CURRENT_SOURCE_DIR}/web/strata_page.cmake"
    VERBATIM)
endif()
