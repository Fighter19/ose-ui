// Containers place their children (Godot's Container classes): boxes, grids, margins,
// panels, splits, scrolling, tabs and flow.
#pragma once
#include "ui/control.h"

namespace ose::ui {

class Container : public Control {
public:
    const char* typeName() const override { return "Container"; }
    // Children's minimum sizes with their visibility.
protected:
    static void fitChild(Control* c, const Rect& area);  // honours the child's size flags
};

class BoxContainer : public Container {
public:
    explicit BoxContainer(bool vertical) : vertical_(vertical) {}
    const char* typeName() const override { return vertical_ ? "VBox" : "HBox"; }
    Vec2 computeMinSize() override;
    void layout() override;
    bool setProperty(std::string_view key, std::string_view value) override;
    float separation = -1;  // -1: theme constant "separation"
    Align alignment = Align::Begin;  // when no child expands

protected:
    bool vertical_;
    float sep() const { return separation >= 0 ? separation : constant("separation"); }
};
class HBox : public BoxContainer {
public:
    HBox() : BoxContainer(false) {}
};
class VBox : public BoxContainer {
public:
    VBox() : BoxContainer(true) {}
};

class GridContainer : public Container {
public:
    const char* typeName() const override { return "Grid"; }
    Vec2 computeMinSize() override;
    void layout() override;
    bool setProperty(std::string_view key, std::string_view value) override;
    int columns = 2;
    float hSeparation = -1, vSeparation = -1;
};

class MarginContainer : public Container {
public:
    const char* typeName() const override { return "Margin"; }
    Vec2 computeMinSize() override;
    void layout() override;
    bool setProperty(std::string_view key, std::string_view value) override;
    float margins[4] = {4, 4, 4, 4};
};

class CenterContainer : public Container {
public:
    const char* typeName() const override { return "Center"; }
    Vec2 computeMinSize() override;
    void layout() override;
};

// A style box behind the children, which sit inside its content margins.
class PanelContainer : public Container {
public:
    const char* typeName() const override { return "PanelContainer"; }
    Vec2 computeMinSize() override;
    void layout() override;
    void draw(Canvas& c) override;
};

// Just a style box ("panel"); children are placed by anchors.
class Panel : public Control {
public:
    const char* typeName() const override { return "Panel"; }
    void draw(Canvas& c) override;
};

// Two children with a draggable divider.
class SplitContainer : public Container {
public:
    explicit SplitContainer(bool vertical) : vertical_(vertical) {}
    const char* typeName() const override { return "Split"; }
    Vec2 computeMinSize() override;
    void layout() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    Cursor cursor(Vec2 p) const override;
    bool setProperty(std::string_view key, std::string_view value) override;
    // Position of the divider: a fraction of the size, or (fixed > 0) pixels from the
    // start (fixedEnd: from the end), so side panels keep their width when the window grows.
    float ratio = 0.5f;
    float fixed = 0;
    bool fixedEnd = false;
    Signal<> dragged;
    bool vertical() const { return vertical_; }

private:
    Rect grabber() const;
    bool vertical_;
    bool dragging_ = false;
    float dragOffset_ = 0;
};
class HSplit : public SplitContainer {
public:
    HSplit() : SplitContainer(false) {}
};
class VSplit : public SplitContainer {
public:
    VSplit() : SplitContainer(true) {}
};

class ScrollContainer : public Container {
public:
    ScrollContainer() { clipContents = true; }
    const char* typeName() const override { return "Scroll"; }
    Vec2 computeMinSize() override;
    void layout() override;
    void drawOver(Canvas& c) override;
    bool event(const InputEvent& e) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    void scrollTo(float y) { scroll_.y = y; }
    void ensureVisible(const Rect& r);  // scrolls so r (global) is inside
    bool horizontal = false, vertical = true;
    Vec2 scroll() const { return scroll_; }

private:
    Vec2 content_;
    Vec2 scroll_;
    int dragBar_ = -1;  // 0 vertical, 1 horizontal
    float dragStart_ = 0, dragScroll_ = 0;
    Rect bar(int axis, bool grab) const;
};

// Pages with a tab bar; each child is a page titled by its `title`.
class TabContainer : public Container {
public:
    const char* typeName() const override { return "TabContainer"; }
    Vec2 computeMinSize() override;
    void layout() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    bool connectSignal(std::string_view s, std::function<void()> fn) override;
    int current() const { return current_; }
    void setCurrent(int i);
    Signal<int> tabChanged;
    // Tabs can be dragged to other tab containers (docking) when set.
    bool dragTabs = false;
    bool canDrop(Vec2 p, const DragPayload& d) override;
    void drop(Vec2 p, const DragPayload& d) override;
    bool dragData(Vec2 p, DragPayload& out) override;
    float tabBarHeight() const;

private:
    std::vector<Rect> tabRects() const;
    int current_ = 0;
    int hoverTab_ = -1;
};

class FlowContainer : public Container {
public:
    const char* typeName() const override { return "Flow"; }
    Vec2 computeMinSize() override;
    void layout() override;
    float separation = 4;
};

}  // namespace ose::ui
