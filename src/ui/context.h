// The UI context: owns the control tree and its overlays (popups, menus, dialogs,
// tooltips), routes input (hover, focus, mouse capture, shortcuts, drag and drop), runs
// layout and draws everything into a Canvas. Actions and bindings connect XML layouts to
// application code.
#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ui/control.h"

namespace ose::ui {

// One registry for buttons, menu items, shortcuts and the command palette.
struct Action {
    std::string name;      // "file.save"
    std::string label;     // "Save"
    std::string icon;      // icon name (theme.h)
    std::string shortcut;  // "Ctrl+S", "F5", "Shift+Ctrl+Z"
    std::string help;
    std::function<void()> run;
    std::function<bool()> enabled;  // null = always
    std::function<bool()> checked;  // null = not checkable
};

class ActionRegistry {
public:
    Action& add(Action a);
    Action* find(std::string_view name);
    const Action* find(std::string_view name) const;
    bool run(std::string_view name);
    bool isEnabled(std::string_view name) const;
    bool isChecked(std::string_view name) const;
    // Runs the action whose shortcut matches a key press. Returns true if one ran.
    bool handleShortcut(const InputEvent& e);
    std::vector<const Action*> all() const;
    static bool matches(std::string_view shortcut, const InputEvent& e);

private:
    std::vector<std::unique_ptr<Action>> actions_;
    std::unordered_map<std::string, Action*> byName_;
};

// Named values the XML can bind to: "{play.running}" sets a property from a provider
// every frame; "!" negates. Action names with ".enabled" / ".checked" work without
// registering anything.
class Bindings {
public:
    void set(std::string name, std::function<std::string()> fn) { values_[std::move(name)] = std::move(fn); }
    std::optional<std::string> get(std::string_view expr, const ActionRegistry* actions) const;

private:
    std::unordered_map<std::string, std::function<std::string()>> values_;
};

enum class PopupPlacement { Below, Right, AtPoint, Center };

class Context {
public:
    explicit Context(std::shared_ptr<Theme> theme);
    ~Context();

    void setRoot(std::unique_ptr<Control> root);
    Control* root() const { return root_.get(); }
    std::unique_ptr<Control> takeRoot();
    void resize(float width, float height);
    float width() const { return width_; }
    float height() const { return height_; }

    // Input, per frame processing (bindings, layout) and drawing.
    void dispatch(const InputEvent& e);
    void process(float dt);
    void draw(Canvas& canvas);

    // Overlays: popups close when the mouse goes down outside them (unless modal).
    Control* openPopup(std::unique_ptr<Control> popup, Rect anchor, PopupPlacement place = PopupPlacement::Below, bool modal = false);
    void closePopup(Control* popup);
    void closePopupsAbove(Control* popup);  // keeps `popup`, closes later ones (submenus)
    void closeAllPopups();
    bool isPopupOpen(const Control* popup) const;
    size_t popupCount() const { return overlays_.size(); }
    Signal<Control*> popupClosed;

    Control* focus() const { return isAlive(focus_) ? focus_ : nullptr; }
    void setFocus(Control* c);
    Control* hovered() const { return isAlive(hover_) ? hover_ : nullptr; }
    Control* capture() const { return isAlive(capture_) ? capture_ : nullptr; }
    // Exclusive input: while set, every mouse and key event goes to this control, whatever is
    // under the pointer or focused (a game view in play mode, with the pointer grabbed).
    void setInputGrab(Control* c) { grab_ = c; }
    Control* inputGrab() const { return isAlive(grab_) ? grab_ : nullptr; }
    void setCapture(Control* c) { capture_ = c; }
    Cursor cursor() const { return cursor_; }
    Vec2 mousePos() const { return mouse_; }
    int mods() const { return mods_; }
    bool mouseDown(MouseButton b) const { return (buttons_ & (1 << int(b))) != 0; }
    bool dragging() const { return drag_.has_value(); }
    const DragPayload* dragPayload() const { return drag_ ? &*drag_ : nullptr; }
    void cancelDrag() { drag_.reset(); }

    ActionRegistry& actions() { return actions_; }
    Bindings& bindings() { return bindings_; }
    const Theme& theme() const { return *theme_; }
    std::shared_ptr<Theme> themePtr() const { return theme_; }
    void setTheme(std::shared_ptr<Theme> t) { theme_ = std::move(t); }
    GlyphAtlas& atlas() { return atlas_; }
    double time() const { return time_; }
    uint64_t frame() const { return frame_; }

    // Platform services (set by the application).
    std::function<std::string()> getClipboard;
    std::function<void(const std::string&)> setClipboard;
    std::function<void(bool on)> textInput;  // on-screen keyboard / IME on focus changes

    // Files dropped from the OS go to the control under the pointer as a payload of type
    // "files" (data: std::vector<std::string>), or to this callback when nothing takes them.
    std::function<void(const std::vector<std::string>&, Vec2)> filesDropped;

private:
    Control* pick(Vec2 p, bool forDrop = false) const;
    Control* pickIn(Control* c, Vec2 p, bool forDrop) const;
    bool deliver(Control* target, const InputEvent& e);  // bubbles to parents
    void updateHover(Vec2 p);
    void processTree(Control* c, float dt);
    void drawTree(Control* c, Canvas& canvas);
    void layoutOverlay(size_t i);

    std::shared_ptr<Theme> theme_;
    GlyphAtlas atlas_;
    std::unique_ptr<Control> root_;
    struct Overlay {
        std::unique_ptr<Control> control;
        Rect anchor;
        PopupPlacement place;
        bool modal;
    };
    std::vector<Overlay> overlays_;
    std::vector<std::unique_ptr<Control>> graveyard_;  // closed popups, deleted after the event
    Control* focus_ = nullptr;
    Control* hover_ = nullptr;
    Control* capture_ = nullptr;
    Control* grab_ = nullptr;
    Control* pressed_ = nullptr;  // where the drag started
    Vec2 pressPos_;
    bool dragChecked_ = false;
    std::optional<DragPayload> drag_;
    Control* dropTarget_ = nullptr;
    Vec2 mouse_;
    int mods_ = 0;
    int buttons_ = 0;
    Cursor cursor_ = Cursor::Arrow;
    float width_ = 0, height_ = 0;
    double time_ = 0;
    uint64_t frame_ = 0;
    // Tooltip.
    Control* tipFor_ = nullptr;
    double hoverSince_ = 0;
    std::unique_ptr<Control> tooltip_;
    ActionRegistry actions_;
    Bindings bindings_;
};

}  // namespace ose::ui
