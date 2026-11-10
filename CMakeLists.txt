cmake_minimum_required(VERSION 3.20)
project(ose_ui LANGUAGES C CXX)

# ose-ui: a retained-mode UI toolkit (controls, containers, themes, XML layouts, docking, text)
# that draws through a small renderer interface. Back ends: OpenGL ES 2.0 / WebGL 1. Platform
# layer: SDL3 (optional). No other dependencies; stb_truetype is included.

option(OSE_UI_BUILD_GLES2 "Build the GLES 2 / WebGL 1 renderer" ON)
option(OSE_UI_BUILD_SDL3 "Build the SDL3 platform layer and the demo" ON)
option(OSE_UI_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(OSE_UI_FETCH_SDL3 "Download and build SDL3 if no installed SDL3 is found" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(OSE_UI_WARNINGS /W4)
else()
  set(OSE_UI_WARNINGS -Wall -Wextra)
endif()

# --- Toolkit ------------------------------------------------------------------------------

file(GLOB OSE_UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp)
add_library(ose_ui STATIC ${OSE_UI_SOURCES}
  src/core/math.cpp src/core/util.cpp src/core/log.cpp src/fs/filesystem.cpp)
target_include_directories(ose_ui PUBLIC src PRIVATE third_party/stb)
if(EMSCRIPTEN)
  # Fonts and themes are packed into the page's virtual file system (see the demo).
  target_compile_definitions(ose_ui PUBLIC OSE_UI_FONT_DIR="/fonts" OSE_UI_THEME_DIR="/themes")
else()
  target_compile_definitions(ose_ui PUBLIC OSE_UI_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    OSE_UI_THEME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/themes")
endif()
target_compile_options(ose_ui PRIVATE ${OSE_UI_WARNINGS})

# --- GLES 2 renderer ----------------------------------------------------------------------

if(OSE_UI_BUILD_GLES2)
  add_library(ose_ui_gles2 STATIC src/render/gles2/gles2_renderer.cpp)
  target_link_libraries(ose_ui_gles2 PUBLIC ose_ui)
  target_compile_options(ose_ui_gles2 PRIVATE ${OSE_UI_WARNINGS})
  if(NOT EMSCRIPTEN)
    find_library(OSE_UI_GLESV2 GLESv2 REQUIRED)
    target_link_libraries(ose_ui_gles2 PUBLIC ${OSE_UI_GLESV2})
  endif()
endif()

# --- SDL3 platform layer and demo ---------------------------------------------------------

if(OSE_UI_BUILD_SDL3 AND OSE_UI_BUILD_GLES2)
  include(cmake/sdl3.cmake)
  add_library(ose_ui_sdl3 STATIC src/platform/sdl3/ui_sdl3.cpp)
  target_link_libraries(ose_ui_sdl3 PUBLIC ose_ui ose_ui_gles2 SDL3::SDL3)
  target_compile_options(ose_ui_sdl3 PRIVATE ${OSE_UI_WARNINGS})

  add_executable(ose_ui_demo examples/demo/main.cpp)
  target_link_libraries(ose_ui_demo PRIVATE ose_ui_sdl3)
  if(EMSCRIPTEN)
    set_target_properties(ose_ui_demo PROPERTIES SUFFIX ".html")
    target_link_options(ose_ui_demo PRIVATE -sMAX_WEBGL_VERSION=1 -sMIN_WEBGL_VERSION=1 -sALLOW_MEMORY_GROWTH=1
      "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/fonts@/fonts" "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/themes@/themes")
  endif()
endif()

# --- Tests --------------------------------------------------------------------------------

if(OSE_UI_BUILD_TESTS)
  enable_testing()
  add_executable(ose_ui_tests tests/main.cpp tests/test_ui.cpp)
  target_include_directories(ose_ui_tests PRIVATE third_party/doctest)
  target_link_libraries(ose_ui_tests PRIVATE ose_ui)
  add_test(NAME ose_ui_tests COMMAND ose_ui_tests)

  # Rendering test on a headless EGL context (Mesa llvmpipe), if EGL is available.
  find_library(OSE_UI_EGL EGL)
  if(OSE_UI_BUILD_GLES2 AND OSE_UI_EGL AND NOT EMSCRIPTEN)
    add_executable(ose_ui_render_tests tests/main.cpp tests/test_gles2_render.cpp)
    target_include_directories(ose_ui_render_tests PRIVATE third_party/doctest third_party/stb)
    target_link_libraries(ose_ui_render_tests PRIVATE ose_ui_gles2 ${OSE_UI_EGL})
    target_compile_definitions(ose_ui_render_tests PRIVATE OSE_UI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output")
    add_test(NAME ose_ui_render_tests COMMAND ose_ui_render_tests)
  endif()

  # The demo through SDL3's offscreen video driver (EGL), rendering a few frames.
  if(TARGET ose_ui_demo AND NOT EMSCRIPTEN)
    add_test(NAME ose_ui_demo_offscreen
      COMMAND ose_ui_demo --frames 3 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/test-output/demo.ppm)
    set_tests_properties(ose_ui_demo_offscreen PROPERTIES
      ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;EGL_PLATFORM=surfaceless;GALLIUM_DRIVER=llvmpipe")
  endif()
endif()
