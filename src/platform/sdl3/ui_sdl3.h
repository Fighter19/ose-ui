// SDL3 platform layer: a window with an OpenGL ES 2.0 (WebGL 1) context, input translated to
// ui::InputEvent, clipboard, mouse cursors, dropped files and the main loop (also in the
// browser with Emscripten).
#pragma once
#include <functional>
#include <memory>
#include <string>

#include "ui/context.h"
#include "ui/renderer.h"

namespace ose::ui::sdl3 {

struct AppDesc {
    std::string title = "ose-ui";
    int width = 1280, height = 800;
    std::shared_ptr<Theme> theme;  // default: the dark theme
    float clearColor[3] = {0.1f, 0.1f, 0.12f};
};

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Creates the window, the GL context, the renderer and the UI context. Returns false with
    // an error message on failure.
    bool init(const AppDesc& desc, std::string* error = nullptr);

    Context& ui();
    Renderer& renderer();

    // One frame: events, update(dt), layout, draw, swap. Returns false once the window was
    // closed or quit() was called.
    bool frame(const std::function<void(float dt)>& update = {});
    void quit();

    // Runs frames until quit; uses the browser's frame callback under Emscripten.
    void run(std::function<void(float dt)> update = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ose::ui::sdl3
