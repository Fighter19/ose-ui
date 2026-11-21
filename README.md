# ose-ui

A retained-mode UI toolkit for tools and editors: controls, containers, themes, XML layouts
with hot reload, actions and shortcuts, docking, menus, lists, trees, tables, text editing and
drag and drop. It was split out of [OpenSE](https://github.com/Fighter19/OpenSE)'s `engine/ui`
and is used by OpenSE and by [Blutmond](https://github.com/Fighter19/blutmond).

- **No dependencies** in the toolkit itself; `stb_truetype` (public domain) is included.
- **Rendering** goes through a small interface (`ui/renderer.h`). Included back end:
  **OpenGL ES 2.0 / WebGL 1** (`render/gles2`), which also runs on GLES 3 / WebGL 2 contexts.
  It draws with 32-bit indices when `GL_OES_element_index_uint` is there and falls back to
  16-bit indices otherwise.
- **Platform layer** (optional): **SDL3** (`platform/sdl3`): window, GL context, input,
  clipboard, cursors, dropped files, main loop, also in the browser with Emscripten.
- **Themes**: the flat editor theme in **dark** and **light** (`Theme::makeDark`,
  `Theme::makeLight`) and a **classic** light-grey theme with raised and sunken 3D edges
  (`Theme::makeClassic`), plus overrides from XML theme files. `Context::setTheme` switches
  while running.

## Layout

| Path | Content |
|---|---|
| `src/ui/` | the toolkit (namespace `ose::ui`) |
| `src/core/`, `src/fs/` | small helpers it needs (maths, strings, logging, reading files) |
| `src/render/gles2/` | GLES 2 / WebGL 1 renderer |
| `src/platform/sdl3/` | SDL3 platform layer |
| `examples/demo/` | demo window |
| `fonts/` | DejaVu and Font Awesome 4.7 (licences in `fonts/LICENSE.md`) |
| `third_party/` | `stb_truetype.h` (public domain), `doctest.h` (MIT, tests only) |

## Build

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

SDL3 is taken from the system if installed (`find_package(SDL3)`); otherwise CMake downloads
the SDL3 3.4.18 release archive (pinned by SHA-256) and builds it statically. Switch that off
with `-DOSE_UI_FETCH_SDL3=OFF`; leave out the platform layer with `-DOSE_UI_BUILD_SDL3=OFF`.
Building SDL3 on Ubuntu 24.04 needs its development packages:

```
sudo apt install libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxss-dev libxfixes-dev libxtst-dev libxinerama-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libdrm-dev libgbm-dev libasound2-dev libpulse-dev libegl-dev libgles-dev
```

The tests run headless: the renderer test on an EGL surfaceless context (Mesa llvmpipe), the
demo through SDL3's offscreen video driver. Pictures land in `build/test-output/`.

Browser (WebGL 1), with the emsdk environment active; SDL3 comes from Emscripten's port:

```
emcmake cmake -S . -B build-web -DOSE_UI_BUILD_TESTS=OFF
cmake --build build-web
```

## Using it

```cpp
#include "platform/sdl3/ui_sdl3.h"
#include "ui/loader.h"

ose::ui::sdl3::App app;
app.init({.title = "Tool", .theme = ose::ui::Theme::makeClassic(OSE_UI_FONT_DIR)});
ose::ui::LayoutLoader loader(app.ui());
app.ui().setRoot(loader.loadText("<Panel anchors=\"full\"><Button text=\"Hello\"/></Panel>"));
app.run();
```

Without SDL3, create a GLES 2 context yourself, feed `ui::InputEvent`s to `Context::dispatch`,
call `Context::process`, `Context::draw(canvas)` and `Renderer::render(canvas, w, h)` per frame.
