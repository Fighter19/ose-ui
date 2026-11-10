// Lists: a tree (outliner, file lists) and a grid of items with thumbnails (asset browser).
// Both draw only the rows in view, so thousands of items stay cheap. Put them in a
// ScrollContainer.
#pragma once
#include "ui/widgets.h"

namespace ose::ui {

class Tree;

class TreeItem {
public:
    std::string text;
    std::string secondary;  // dim text on the right (a class name...)
    std::string icon;
    Color iconColor{0, 0, 0, 0};  // alpha 0 = font colour
    // An optional toggle on the right (e.g. visibility): icon when on / off.
    std::string toggleIcon, toggleIconOff;
    bool toggleOn = true;
    bool expanded = true;
    bool selectable = true;
    std::any data;
    uint64_t key = 0;

    TreeItem* add(std::string text, std::string icon = {});
    TreeItem* parent() const { return parent_; }
    size_t childCount() const { return children_.size(); }
    TreeItem* child(size_t i) const { return children_[i].get(); }
    void clear() { children_.clear(); }
    int depth() const;

private:
    friend class Tree;
    TreeItem* parent_ = nullptr;
    std::vector<std::unique_ptr<TreeItem>> children_;
};

class Tree : public Control {
public:
    Tree();
    const char* typeName() const override { return "Tree"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    void mouseExited() override { hover_ = nullptr; }
    bool dragData(Vec2 p, DragPayload& out) override;
    bool canDrop(Vec2 p, const DragPayload& d) override;
    void drop(Vec2 p, const DragPayload& d) override;

    TreeItem* root() { return &root_; }
    void clear();
    bool hideRoot = true;
    bool multiSelect = true;
    const std::vector<TreeItem*>& selection() const { return selection_; }
    void select(TreeItem* item, bool add = false, bool notify = true);
    void clearSelection(bool notify = true);
    bool isSelected(const TreeItem* item) const;
    TreeItem* itemAt(Vec2 p);
    Rect itemRect(const TreeItem* item);
    TreeItem* findByKey(uint64_t key);

    Signal<> selectionChanged;
    Signal<TreeItem*> activated;      // double click / Enter
    Signal<TreeItem*> toggleClicked;  // the toggle icon
    Signal<TreeItem*, Vec2> contextMenu;  // right click (screen position)
    // Drag and drop hooks: what an item drags, whether a payload drops onto an item (null =
    // empty space), and the drop itself.
    std::function<bool(TreeItem*, DragPayload&)> dragProvider;
    std::function<bool(TreeItem*, const DragPayload&)> dropFilter;
    std::function<void(TreeItem*, const DragPayload&)> onDrop;

private:
    void flatten(TreeItem* item, std::vector<TreeItem*>& out);
    const std::vector<TreeItem*>& rows();
    float itemHeight() const;
    TreeItem root_;
    std::vector<TreeItem*> selection_;
    std::vector<TreeItem*> rows_;
    uint64_t rowsFrame_ = ~0ull;
    TreeItem* hover_ = nullptr;
    TreeItem* anchor_ = nullptr;  // for shift selection
    TreeItem* dropItem_ = nullptr;
};

// A grid of tiles (thumbnail + label).
class ItemList : public Control {
public:
    struct Item {
        std::string text;
        std::string icon;
        Color iconColor{0, 0, 0, 0};
        TextureHandle texture;
        std::string tooltip;
        std::any data;
    };
    ItemList() {
        focusMode = FocusMode::Click;
        clipContents = false;
    }
    const char* typeName() const override { return "ItemList"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    bool dragData(Vec2 p, DragPayload& out) override;

    std::vector<Item> items;
    float tileSize = 96;   // thumbnail edge; 0 = list mode (one row per item)
    int selected = -1;
    Signal<int> selectedChanged;
    Signal<int> activated;
    Signal<int, Vec2> contextMenu;
    std::function<bool(int, DragPayload&)> dragProvider;
    // Asked every frame for the texture of each item in view (load thumbnails lazily).
    std::function<void(int)> wantThumbnail;
    int itemAt(Vec2 p) const;
    Rect itemRect(int i) const;

private:
    int columns() const;
    Vec2 cell() const;
};

// Where the application draws a 3D or 2D view. It renders before the UI is drawn (see
// collectViewports), receives input while hovered or focused, and can draw overlays.
class ViewportControl : public Control {
public:
    ViewportControl() {
        focusMode = FocusMode::Click;
        clipContents = true;
    }
    const char* typeName() const override { return "Viewport"; }
    void draw(Canvas& c) override;
    void drawOver(Canvas& c) override;
    bool event(const InputEvent& e) override { return onInput ? onInput(e) : false; }
    Cursor cursor(Vec2) const override { return cursorShape; }
    bool canDrop(Vec2 p, const DragPayload& d) override { return onCanDrop ? onCanDrop(p, d) : false; }
    void drop(Vec2 p, const DragPayload& d) override {
        if (onDrop) onDrop(p, d);
    }
    std::function<void(const Rect&)> onRender;           // draw the scene (before the UI)
    std::function<bool(const InputEvent&)> onInput;
    std::function<void(Canvas&, const Rect&)> onOverlay;  // UI-space drawing over it
    std::function<bool(Vec2, const DragPayload&)> onCanDrop;
    std::function<void(Vec2, const DragPayload&)> onDrop;
    Cursor cursorShape = Cursor::Arrow;
    bool transparent = true;  // leave the area for the scene (no background)
};
void collectViewports(Control* root, std::vector<ViewportControl*>& out);

// A dialog window: title bar and content (its children), shown as a popup.
class Window : public Control {
public:
    Window() { focusMode = FocusMode::All; }
    const char* typeName() const override { return "Window"; }
    Vec2 computeMinSize() override;
    void layout() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    float titleHeight() const;
    std::function<void()> onClose;  // the close button (closes the popup by default)
};

}  // namespace ose::ui
