# SDL3 for the platform layer:
#   - Emscripten: SDL3 port of Emscripten (--use-port=sdl3).
#   - Otherwise: an installed SDL3 (find_package), or a pinned release downloaded and built
#     statically (OSE_UI_FETCH_SDL3, on by default).

if(TARGET SDL3::SDL3)
  return()
endif()

if(EMSCRIPTEN)
  add_library(ose_ui_sdl3_port INTERFACE)
  target_compile_options(ose_ui_sdl3_port INTERFACE --use-port=sdl3)
  target_link_options(ose_ui_sdl3_port INTERFACE --use-port=sdl3)
  add_library(SDL3::SDL3 ALIAS ose_ui_sdl3_port)
  return()
endif()

find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  message(STATUS "ose-ui: using installed SDL3 ${SDL3_VERSION}")
  return()
endif()

if(NOT OSE_UI_FETCH_SDL3)
  message(FATAL_ERROR "SDL3 not found; install it or enable OSE_UI_FETCH_SDL3")
endif()

message(STATUS "ose-ui: building SDL3 3.4.18 from the release archive")
include(FetchContent)
# Only what the toolkit and its users need; the rest stays off so the build is short.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
  URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)
if(NOT TARGET SDL3::SDL3 AND TARGET SDL3::SDL3-static)
  add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()
