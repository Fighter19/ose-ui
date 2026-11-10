// Control: the base of every UI element (Godot's Control): a node in a tree with a rectangle,
// layout properties (anchors and offsets, minimum size, size flags), theme lookups, input
// handling and drag and drop. Containers (containers.h) place their children; other
// controls place children by anchors.
#pragma once
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ui/canvas.h"
#include "ui/core.h"
#include "ui/theme.h"

namespace ose::ui {

class Context;

namespace SizeFlags {
enum : int { Fill = 1, Expand = 2, ShrinkCenter = 4, ShrinkEnd = 8 };
}
enum class MouseFilter { Stop, Pass, Ignore };
enum class FocusMode { None, Click, All };

// What is dragged: a type tag the targets check ("asset", "outliner"...), the data, a label
// for the drag preview.
struct DragPayload {
    std::string type;
    std::any data;
    std::string label;
    std::string icon;
};

class Control {
public:
    Control();
    virtual ~Control();
    Control(const Control&) = delete;
    Control& operator=(const Control&) = delete;

    // The theme type used for lookups; themeType (a variation) overrides it.
    virtual const char* typeName() const { return "Control"; }
    std::string_view themeTypeName() const { return themeType.empty() ? std::string_view(typeName()) : std::string_view(themeType); }

    // ---- tree
    Control* parent() const { return parent_; }
    size_t childCount() const { return children_.size(); }
    Control* child(size_t i) const { return children_[i].get(); }
    template <class T, class... A>
    T* add(A&&... args) {
        auto c = std::make_unique<T>(std::forward<A>(args)...);
        T* p = c.get();
        addChild(std::move(c));
        return p;
    }
    Control* addChild(std::unique_ptr<Control> c, int index = -1);
    std::unique_ptr<Control> removeChild(Control* c);
    void clearChildren();
    void moveChild(Control* c, int index);
    int indexOf(const Control* c) const;
    // Depth-first search by id (this control included).
    Control* find(std::string_view id);
    template <class T>
    T* findAs(std::string_view id) {
        return dynamic_cast<T*>(find(id));
    }
    bool isAncestorOf(const Control* c) const;
    Context* context() const;

    // ---- properties
    std::string id;
    std::string themeType;  // theme variation, e.g. "ToolButton"
    std::string tooltip;
    std::string title;  // shown by tab containers, docks and windows
    std::string icon;   // icon name for tabs and docks
    Vec2 customMinSize;
    int sizeFlagsH = SizeFlags::Fill, sizeFlagsV = SizeFlags::Fill;
    float stretchRatio = 1;
    float anchors[4] = {0, 0, 0, 0};  // left, top, right, bottom (fractions of the parent)
    float offsets[4] = {0, 0, 0, 0};  // pixels added to the anchored edges
    bool clipContents = false;
    MouseFilter mouseFilter = MouseFilter::Stop;
    FocusMode focusMode = FocusMode::None;
    bool disabled = false;
    std::any userData;  // whatever the application attaches

    bool visible() const { return visible_; }
    void setVisible(bool v);
    bool visibleInTree() const;
    bool isDisabled() const;  // this or an ancestor
    // Anchors for the whole parent ("full"), or a preset name: full, top, bottom, left,
    // right, center, top-left, top-right, bottom-left, bottom-right.
    void setAnchorPreset(std::string_view preset);

    // ---- layout
    const Rect& rect() const { return rect_; }
    void setRect(const Rect& r);
    Vec2 minimumSize();  // max(customMinSize, computeMinSize()), cached per frame
    virtual Vec2 computeMinSize();
    virtual void layout();  // places children inside rect_ (default: by anchors)
    void layoutTree();      // layout() and then the children's layoutTree()

    // ---- drawing (before / after the children)
    virtual void draw(Canvas&) {}
    virtual void drawOver(Canvas&) {}

    // ---- input
    virtual bool event(const InputEvent&) { return false; }
    virtual Cursor cursor(Vec2) const { return Cursor::Arrow; }
    virtual void mouseEntered() {}
    virtual void mouseExited() {}
    virtual void focusChanged(bool) {}
    bool hovered() const;
    bool hasFocus() const;
    void grabFocus();
    void releaseFocus();
    // Point inside this control for picking (rect by default).
    virtual bool hitTest(Vec2 p) const { return rect_.contains(p); }

    // ---- drag and drop (Godot's get_drag_data / can_drop_data / drop_data)
    virtual bool dragData(Vec2, DragPayload&) { return false; }
    virtual bool canDrop(Vec2, const DragPayload&) { return false; }
    virtual void drop(Vec2, const DragPayload&) {}

    // ---- XML: properties and signals by name
    virtual bool setProperty(std::string_view key, std::string_view value);
    virtual bool connectSignal(std::string_view signal, std::function<void()> fn);
    // Called every frame before layout (bindings, animations).
    virtual void process(float) {}
    std::vector<std::function<void()>> binders;

    // ---- theme
    const Theme& theme() const;
    const StyleBox& box(std::string_view name) const { return theme().box(themeTypeName(), name); }
    Color color(std::string_view name) const { return theme().color(themeTypeName(), name); }
    float constant(std::string_view name) const { return theme().constant(themeTypeName(), name); }
    const Font& font() const;
    float fontSize() const;
    const Font& iconFont() const;

    Signal<> resized;
    Signal<bool> visibilityChanged;

protected:
    friend class Context;
    Control* parent_ = nullptr;
    Context* ctx_ = nullptr;  // set on the root only
    std::vector<std::unique_ptr<Control>> children_;
    Rect rect_;
    bool visible_ = true;
    Vec2 cachedMin_;
    uint64_t minFrame_ = ~0ull;
};

// Whether a control still exists (the context keeps plain pointers to hovered/focused ones).
bool isAlive(const Control* c);

// Helpers for property parsing.
bool parseBool(std::string_view v);
Vec2 parseSize(std::string_view v);  // "w h" or "w"
int parseSizeFlags(std::string_view v);  // "fill expand shrink_center shrink_end"

}  // namespace ose::ui
