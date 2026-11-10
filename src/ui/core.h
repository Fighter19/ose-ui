// UI toolkit basics: rectangles, colours, signals, input events, UTF-8.
// The toolkit (engine/ui) is a retained-mode tree of controls modelled on Godot's Control
// API, laid out from XML files. See docs/editor/plan.md, section 5.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/math.h"

namespace ose::ui {

// A texture owned by the renderer (ui/renderer.h). 0 = none; for canvas commands "none" means
// the glyph atlas.
struct TextureHandle {
    uint32_t id = 0;
    explicit operator bool() const { return id != 0; }
    bool operator==(const TextureHandle&) const = default;
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Vec2 pos() const { return {x, y}; }
    Vec2 size() const { return {w, h}; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
    Rect intersect(const Rect& o) const;
    Rect shrink(float l, float t, float r, float b) const { return {x + l, y + t, std::max(0.0f, w - l - r), std::max(0.0f, h - t - b)}; }
    Rect shrink(float m) const { return shrink(m, m, m, m); }
    bool operator==(const Rect&) const = default;
};

// Linear RGBA in 0..1 (the UI draws in display space; colours are as written).
using Color = Vec4;
// "#rrggbb", "#rrggbbaa", "r g b [a]" (0..1) or a few names ("white", "black", "transparent").
Color parseColor(std::string_view s, Color def = {1, 1, 1, 1});
inline Color withAlpha(Color c, float a) { return {c.x, c.y, c.z, a}; }
inline Color mix(Color a, Color b, float t) { return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t)}; }

// Connect callbacks; emit calls them in connection order.
template <class... Args>
class Signal {
public:
    using Fn = std::function<void(Args...)>;
    int connect(Fn fn) {
        slots_.push_back({++next_, std::move(fn)});
        return next_;
    }
    void disconnect(int id) {
        std::erase_if(slots_, [&](const Slot& s) { return s.id == id; });
    }
    void clear() { slots_.clear(); }
    bool empty() const { return slots_.empty(); }
    void emit(Args... args) const {
        auto copy = slots_;  // slots may connect/disconnect while running
        for (auto& s : copy) s.fn(args...);
    }

private:
    struct Slot {
        int id;
        Fn fn;
    };
    std::vector<Slot> slots_;
    int next_ = 0;
};

namespace Mod {
enum : int { Shift = 1, Ctrl = 2, Alt = 4, Super = 8 };
}

enum class MouseButton : int { None = 0, Left = 1, Middle = 2, Right = 3 };

struct InputEvent {
    enum class Type { MouseMove, MouseDown, MouseUp, Wheel, KeyDown, KeyUp, Text, FilesDropped, Leave };
    Type type = Type::MouseMove;
    Vec2 pos;          // pointer position in pixels (top left origin)
    Vec2 delta;        // relative motion (MouseMove) or wheel steps (Wheel, +y = up)
    MouseButton button = MouseButton::None;
    int clicks = 1;    // 2 for double clicks
    std::string key;   // lower-case key name: "a", "1", "f5", "escape", "enter", "backspace",
                       // "delete", "left", "right", "up", "down", "home", "end", "pageup",
                       // "pagedown", "tab", "space", "insert", "shift", "ctrl", "alt"
    int mods = 0;      // Mod bits
    bool repeat = false;
    std::string text;  // UTF-8 (Text)
    std::vector<std::string> files;  // FilesDropped: OS paths
    bool isMouse() const { return type == Type::MouseMove || type == Type::MouseDown || type == Type::MouseUp || type == Type::Wheel; }
};

enum class Cursor { Arrow, IBeam, Hand, ResizeH, ResizeV, Move, Crosshair, Forbidden };

// UTF-8 helpers.
uint32_t decodeUtf8(std::string_view s, size_t& i);  // advances i; returns U+FFFD on errors
void appendUtf8(std::string& out, uint32_t cp);
size_t prevUtf8(std::string_view s, size_t i);       // start of the code point before i
size_t nextUtf8(std::string_view s, size_t i);

}  // namespace ose::ui
