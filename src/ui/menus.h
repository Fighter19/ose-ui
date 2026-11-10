// Menus: popup menus (with submenus), the menu bar, drop-down option buttons and menu
// buttons. Items run actions from the context's registry or callbacks.
#pragma once
#include "ui/context.h"
#include "ui/widgets.h"

namespace ose::ui {

struct MenuItem {
    std::string label;
    std::string icon;
    std::string shortcut;  // display text; filled from the action when empty
    std::string action;    // action name (label, shortcut, enabled, checked come from it)
    std::function<void()> run;
    std::function<bool()> enabled;
    std::function<bool()> checked;
    bool separator = false;
    std::vector<MenuItem> submenu;
    static MenuItem sep() {
        MenuItem m;
        m.separator = true;
        return m;
    }
};

class PopupMenu : public Control {
public:
    explicit PopupMenu(std::vector<MenuItem> items);
    const char* typeName() const override { return "PopupMenu"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    void mouseExited() override { hover_ = -1; }
    // Opens a menu as an overlay of `ctx` next to `anchor`.
    static PopupMenu* open(Context& ctx, std::vector<MenuItem> items, Rect anchor, PopupPlacement place = PopupPlacement::Below);
    Signal<int> activated;  // index of the item that ran

private:
    float itemHeight() const;
    int itemAt(Vec2 p) const;
    Rect itemRect(int i) const;
    void resolve(MenuItem& m) const;
    bool itemEnabled(const MenuItem& m) const;
    bool itemChecked(const MenuItem& m) const;
    void activate(int i);
    std::vector<MenuItem> items_;
    int hover_ = -1;
    PopupMenu* submenu_ = nullptr;
    int submenuIndex_ = -1;
};

class MenuBar : public Control {
public:
    const char* typeName() const override { return "MenuBar"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    void mouseExited() override { hover_ = -1; }
    void addMenu(std::string title, std::vector<MenuItem> items) { menus_.push_back({std::move(title), std::move(items)}); }
    void clearMenus() { menus_.clear(); }

private:
    std::vector<Rect> titleRects() const;
    void openMenu(int i);
    struct Menu {
        std::string title;
        std::vector<MenuItem> items;
    };
    std::vector<Menu> menus_;
    int hover_ = -1, open_ = -1;
    PopupMenu* popup_ = nullptr;
};

// A button showing the chosen item of a list; opens the list when clicked.
class OptionButton : public Button {
public:
    OptionButton() { align = Align::Begin; }
    const char* typeName() const override { return "OptionButton"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    bool connectSignal(std::string_view s, std::function<void()> fn) override;
    void setItems(std::vector<std::string> items);
    const std::vector<std::string>& items() const { return items_; }
    int selected() const { return selected_; }
    void select(int i, bool notify = true);
    std::string selectedText() const { return selected_ >= 0 && size_t(selected_) < items_.size() ? items_[size_t(selected_)] : std::string(); }
    Signal<int> itemSelected;

protected:
    void activate() override;

private:
    std::vector<std::string> items_;
    int selected_ = -1;
};

// A button that opens a menu.
class MenuButton : public Button {
public:
    MenuButton() { themeType = "MenuButton"; }
    const char* typeName() const override { return "MenuButton"; }
    std::vector<MenuItem> items;
    std::function<std::vector<MenuItem>()> itemsProvider;  // built when opened, when set

protected:
    void activate() override;
};

}  // namespace ose::ui
