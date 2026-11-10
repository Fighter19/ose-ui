#include "platform/sdl3/ui_sdl3.h"

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <GLES2/gl2.h>

#include <algorithm>
#include <cctype>

#include "render/gles2/gles2_renderer.h"

namespace ose::ui::sdl3 {

namespace {

// SDL key codes to the toolkit's lower-case key names (ui/core.h InputEvent::key).
std::string keyName(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return "enter";
    case SDLK_ESCAPE: return "escape";
    case SDLK_BACKSPACE: return "backspace";
    case SDLK_DELETE: return "delete";
    case SDLK_TAB: return "tab";
    case SDLK_SPACE: return "space";
    case SDLK_LEFT: return "left";
    case SDLK_RIGHT: return "right";
    case SDLK_UP: return "up";
    case SDLK_DOWN: return "down";
    case SDLK_HOME: return "home";
    case SDLK_END: return "end";
    case SDLK_PAGEUP: return "pageup";
    case SDLK_PAGEDOWN: return "pagedown";
    case SDLK_INSERT: return "insert";
    case SDLK_LSHIFT: case SDLK_RSHIFT: return "shift";
    case SDLK_LCTRL: case SDLK_RCTRL: return "ctrl";
    case SDLK_LALT: case SDLK_RALT: return "alt";
    default: break;
    }
    if (key >= SDLK_F1 && key <= SDLK_F12) return "f" + std::to_string(int(key - SDLK_F1) + 1);
    std::string name = SDL_GetKeyName(key);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return name;
}

int modsOf(SDL_Keymod m) {
    int mods = 0;
    if (m & SDL_KMOD_SHIFT) mods |= Mod::Shift;
    if (m & SDL_KMOD_CTRL) mods |= Mod::Ctrl;
    if (m & SDL_KMOD_ALT) mods |= Mod::Alt;
    if (m & SDL_KMOD_GUI) mods |= Mod::Super;
    return mods;
}

MouseButton buttonOf(Uint8 b) {
    switch (b) {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    default: return MouseButton::None;
    }
}

SDL_SystemCursor systemCursor(Cursor c) {
    switch (c) {
    case Cursor::IBeam: return SDL_SYSTEM_CURSOR_TEXT;
    case Cursor::Hand: return SDL_SYSTEM_CURSOR_POINTER;
    case Cursor::ResizeH: return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case Cursor::ResizeV: return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case Cursor::Move: return SDL_SYSTEM_CURSOR_MOVE;
    case Cursor::Crosshair: return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case Cursor::Forbidden: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    case Cursor::Arrow: default: return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

}  // namespace

struct App::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext gl = nullptr;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Context> context;
    std::unique_ptr<Canvas> canvas;
    SDL_Cursor* cursors[8] = {};
    Cursor currentCursor = Cursor::Arrow;
    float clear[3] = {0.1f, 0.1f, 0.12f};
    float pixelScale = 1;  // window pixels per window point (high-DPI)
    Uint64 lastTicks = 0;
    bool running = true;
    std::function<void(float)> loopUpdate;

    Vec2 toPixels(float x, float y) const { return {x * pixelScale, y * pixelScale}; }

    void updateSize() {
        int pw = 0, ph = 0, ww = 0, wh = 0;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        SDL_GetWindowSize(window, &ww, &wh);
        pixelScale = ww > 0 ? float(pw) / float(ww) : 1.0f;
        context->resize(float(pw), float(ph));
    }

    void handle(const SDL_Event& ev) {
        InputEvent e;
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running = false;
            return;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
            updateSize();
            return;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            e.type = InputEvent::Type::Leave;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            e.type = InputEvent::Type::MouseMove;
            e.pos = toPixels(ev.motion.x, ev.motion.y);
            e.delta = toPixels(ev.motion.xrel, ev.motion.yrel);
            e.mods = modsOf(SDL_GetModState());
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            e.type = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? InputEvent::Type::MouseDown : InputEvent::Type::MouseUp;
            e.pos = toPixels(ev.button.x, ev.button.y);
            e.button = buttonOf(ev.button.button);
            e.clicks = ev.button.clicks;
            e.mods = modsOf(SDL_GetModState());
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            e.type = InputEvent::Type::Wheel;
            float mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            e.pos = toPixels(mx, my);
            e.delta = {ev.wheel.x, ev.wheel.y};
            e.mods = modsOf(SDL_GetModState());
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            e.type = ev.type == SDL_EVENT_KEY_DOWN ? InputEvent::Type::KeyDown : InputEvent::Type::KeyUp;
            e.key = keyName(ev.key.key);
            e.mods = modsOf(ev.key.mod);
            e.repeat = ev.key.repeat;
            break;
        case SDL_EVENT_TEXT_INPUT:
            e.type = InputEvent::Type::Text;
            e.text = ev.text.text;
            break;
        case SDL_EVENT_DROP_FILE:
            e.type = InputEvent::Type::FilesDropped;
            e.pos = toPixels(ev.drop.x, ev.drop.y);
            if (ev.drop.data) e.files.push_back(ev.drop.data);
            break;
        default:
            return;
        }
        context->dispatch(e);
    }

    bool frame(const std::function<void(float)>& update) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) handle(ev);
        if (!running) return false;

        Uint64 now = SDL_GetTicksNS();
        float dt = lastTicks ? float(double(now - lastTicks) / 1e9) : 1.0f / 60;
        lastTicks = now;
        if (update) update(dt);

        context->process(dt);
        Cursor wanted = context->cursor();
        if (wanted != currentCursor) {
            currentCursor = wanted;
            SDL_SetCursor(cursors[int(wanted)]);
        }

        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        glViewport(0, 0, pw, ph);
        glClearColor(clear[0], clear[1], clear[2], 1);
        glClear(GL_COLOR_BUFFER_BIT);
        context->draw(*canvas);
        renderer->render(*canvas, pw, ph);
        SDL_GL_SwapWindow(window);
        return running;
    }
};

App::App() : impl_(std::make_unique<Impl>()) {}

App::~App() {
    if (!impl_) return;
    impl_->canvas.reset();
    impl_->context.reset();
    impl_->renderer.reset();
    for (SDL_Cursor* c : impl_->cursors)
        if (c) SDL_DestroyCursor(c);
    if (impl_->gl) SDL_GL_DestroyContext(impl_->gl);
    if (impl_->window) SDL_DestroyWindow(impl_->window);
    if (impl_->window) SDL_Quit();
}

bool App::init(const AppDesc& desc, std::string* error) {
    auto fail = [&](const std::string& what) {
        if (error) *error = what + ": " + SDL_GetError();
        return false;
    };
    if (!SDL_Init(SDL_INIT_VIDEO)) return fail("SDL_Init");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    impl_->window = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!impl_->window) return fail("SDL_CreateWindow");
    impl_->gl = SDL_GL_CreateContext(impl_->window);
    if (!impl_->gl) return fail("SDL_GL_CreateContext");
    SDL_GL_MakeCurrent(impl_->window, impl_->gl);
    SDL_GL_SetSwapInterval(1);

    std::copy(desc.clearColor, desc.clearColor + 3, impl_->clear);
    impl_->renderer = createGles2Renderer();
    auto theme = desc.theme ? desc.theme : Theme::makeDark(OSE_UI_FONT_DIR);
    impl_->context = std::make_unique<Context>(theme);
    impl_->canvas = std::make_unique<Canvas>(impl_->context->atlas());
    for (int i = 0; i < 8; i++) impl_->cursors[i] = SDL_CreateSystemCursor(systemCursor(Cursor(i)));

    SDL_Window* window = impl_->window;
    impl_->context->getClipboard = [] {
        char* text = SDL_GetClipboardText();
        std::string s = text ? text : "";
        SDL_free(text);
        return s;
    };
    impl_->context->setClipboard = [](const std::string& s) { SDL_SetClipboardText(s.c_str()); };
    impl_->context->textInput = [window](bool on) {
        if (on) SDL_StartTextInput(window);
        else SDL_StopTextInput(window);
    };
    impl_->updateSize();
    return true;
}

Context& App::ui() { return *impl_->context; }
Renderer& App::renderer() { return *impl_->renderer; }

bool App::frame(const std::function<void(float)>& update) { return impl_->frame(update); }
void App::quit() { impl_->running = false; }

void App::run(std::function<void(float)> update) {
#ifdef __EMSCRIPTEN__
    impl_->loopUpdate = std::move(update);
    emscripten_set_main_loop_arg(
        [](void* arg) {
            auto* impl = static_cast<Impl*>(arg);
            if (!impl->frame(impl->loopUpdate)) emscripten_cancel_main_loop();
        },
        impl_.get(), 0, true);
#else
    while (frame(update)) {
    }
#endif
}

}  // namespace ose::ui::sdl3
