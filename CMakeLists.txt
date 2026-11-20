cmake_minimum_required(VERSION 3.16)
project(ImpactDuel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_CLIENT "Compiler le client graphique (raylib)" ON)
option(BUILD_TESTS "Compiler les tests de simulation" ON)

# --- Noyau commun (simulation + protocole + réseau) -------------------------
add_library(fg_core STATIC
  src/core/characters.cpp
  src/core/sim.cpp
  src/core/protocol.cpp
  src/net/net.cpp)
target_include_directories(fg_core PUBLIC src)
if(WIN32)
  target_link_libraries(fg_core PUBLIC ws2_32)
endif()
if(MSVC)
  target_compile_options(fg_core PRIVATE /W3)
else()
  target_compile_options(fg_core PRIVATE -Wall -Wextra)
endif()

# --- Serveur -----------------------------------------------------------------
add_executable(impact_server src/server/main.cpp)
target_link_libraries(impact_server PRIVATE fg_core)
find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(impact_server PRIVATE Threads::Threads)
endif()

# --- Tests -------------------------------------------------------------------
if(BUILD_TESTS)
  add_executable(impact_tests tests/sim_test.cpp)
  target_link_libraries(impact_tests PRIVATE fg_core)
  enable_testing()
  add_test(NAME simulation COMMAND impact_tests)
endif()

# --- Client (raylib, téléchargé automatiquement si absent) ------------------
if(BUILD_CLIENT)
  find_package(raylib 5.0 QUIET)
  if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 5.5
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
  endif()

  add_executable(impact_duel
    src/client/main.cpp
    src/client/render.cpp
    src/client/audio.cpp
    src/client/ai.cpp)
  target_link_libraries(impact_duel PRIVATE fg_core raylib)
  if(APPLE)
    target_link_libraries(impact_duel PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
  endif()
  # Client complet piloté par script, sans fenêtre (fausse raylib) : vérifie menus et modes
  if(BUILD_TESTS)
    add_executable(impact_headless
      src/client/main.cpp src/client/render.cpp src/client/audio.cpp src/client/ai.cpp
      tests/headless/raylib_stub.cpp)
    target_include_directories(impact_headless PRIVATE $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(impact_headless PRIVATE fg_core)
    add_test(NAME client_headless COMMAND impact_headless)
    set_tests_properties(client_headless PROPERTIES ENVIRONMENT "STUB_SCENARIO=local")
  endif()

  if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(impact_duel PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
      target_link_options(impact_duel PRIVATE /ENTRY:mainCRTStartup)
    endif()
  endif()
endif()
