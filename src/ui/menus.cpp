#include "ui/menus.h"

#include <cmath>

#include "core/util.h"
#include "ui/context.h"

namespace ose::ui {

// ---------------------------------------------------------------------------------- popup menu
PopupMenu::PopupMenu(std::vector<MenuItem> items) : items_(std::move(items)) {}

void PopupMenu::resolve(MenuItem& m) const {
    if (m.action.empty()) return;
    Context* ctx = context();
    if (!ctx) return;
    if (const Action* a = ctx->actions().find(m.action)) {
        if (m.label.empty()) m.label = a->label;
        if (m.shortcut.empty()) m.shortcut = a->shortcut;
        if (m.icon.empty()) m.icon = a->icon;
    }
}

bool PopupMenu::itemEnabled(const MenuItem& m) const {
    if (m.separator) return false;
    if (m.enabled && !m.enabled()) return false;
    if (!m.action.empty())
        if (Context* ctx = context(); ctx && ctx->actions().find(m.action)) return ctx->actions().isEnabled(m.action);
    return true;
}

bool PopupMenu::itemChecked(const MenuItem& m) const {
    if (m.checked) return m.checked();
    if (!m.action.empty())
        if (Context* ctx = context()) return ctx->actions().isChecked(m.action);
    return false;
}

float PopupMenu::itemHeight() const {
    float h = constant("item_height");
    return h > 0 ? h : std::ceil(font().lineHeight(fontSize()) + 8);
}

Vec2 PopupMenu::computeMinSize() {
    const StyleBox& b = box("panel");
    float w = 120, h = 0;
    for (auto& m : items_) {
        resolve(m);
        if (m.separator) {
            h += 7;
            continue;
        }
        float lw = font().measure(m.label, fontSize()) + 28 + 16;
        if (!m.shortcut.empty()) lw += font().measure(m.shortcut, fontSize()) + 24;
        if (!m.submenu.empty()) lw += 16;
        w = std::max(w, lw);
        h += itemHeight();
    }
    return {std::ceil(w + b.margin[0] + b.margin[2]), std::ceil(h + b.margin[1] + b.margin[3])};
}

Rect PopupMenu::itemRect(int idx) const {
    const StyleBox& b = box("panel");
    float y = rect_.y + b.margin[1];
    for (int i = 0; i < int(items_.size()); ++i) {
        float h = items_[size_t(i)].separator ? 7 : itemHeight();
        if (i == idx) return {rect_.x + b.margin[0], y, rect_.w - b.margin[0] - b.margin[2], h};
        y += h;
    }
    return {};
}

int PopupMenu::itemAt(Vec2 p) const {
    for (int i = 0; i < int(items_.size()); ++i)
        if (itemRect(i).contains(p)) return i;
    return -1;
}

void PopupMenu::draw(Canvas& c) {
    c.styleBox(rect_, box("panel"));
    for (int i = 0; i < int(items_.size()); ++i) {
        const MenuItem& m = items_[size_t(i)];
        Rect r = itemRect(i);
        if (m.separator) {
            c.rect({r.x + 4, std::floor(r.center().y), r.w - 8, 1}, color("separator"));
            continue;
        }
        bool en = itemEnabled(m);
        if ((i == hover_ || i == submenuIndex_) && en) c.styleBox(r, box("hover"));
        Color fc = en ? color("font_color") : color("font_disabled_color");
        Rect iconR{r.x + 4, r.y, 20, r.h};
        if (itemChecked(m)) c.icon(iconR, iconCodepoint("check"), iconFont(), fontSize() * 0.85f, color("accent"));
        else if (!m.icon.empty()) c.icon(iconR, iconCodepoint(m.icon), iconFont(), fontSize() * 0.9f, fc);
        Rect text = r.shrink(28, 0, 12, 0);
        c.textIn(text, m.label, font(), fontSize(), fc);
        if (!m.shortcut.empty()) c.textIn(text, m.shortcut, font(), fontSize(), color("font_accelerator_color"), Align::End, false);
        if (!m.submenu.empty()) c.icon({r.right() - 16, r.y, 12, r.h}, iconCodepoint("caret-right"), iconFont(), fontSize() * 0.8f, fc);
    }
}

void PopupMenu::activate(int i) {
    MenuItem m = items_[size_t(i)];
    Context* ctx = context();
    activated.emit(i);
    if (ctx) ctx->closeAllPopups();
    if (m.run) m.run();
    else if (!m.action.empty() && ctx) ctx->actions().run(m.action);
}

bool PopupMenu::event(const InputEvent& e) {
    using T = InputEvent::Type;
    Context* ctx = context();
    if (e.type == T::MouseMove) {
        int h = itemAt(e.pos);
        if (h != hover_) {
            hover_ = h;
            // Submenus open on hover.
            if (h >= 0 && !items_[size_t(h)].submenu.empty() && itemEnabled(items_[size_t(h)]) && ctx) {
                if (submenuIndex_ != h) {
                    ctx->closePopupsAbove(this);
                    submenu_ = PopupMenu::open(*ctx, items_[size_t(h)].submenu, itemRect(h), PopupPlacement::Right);
                    submenuIndex_ = h;
                }
            } else if (h >= 0 && submenuIndex_ >= 0 && ctx) {
                ctx->closePopupsAbove(this);
                submenu_ = nullptr;
                submenuIndex_ = -1;
            }
        }
        return true;
    }
    if (e.type == T::MouseUp && e.button == MouseButton::Left) {
        int i = itemAt(e.pos);
        if (i >= 0 && itemEnabled(items_[size_t(i)]) && items_[size_t(i)].submenu.empty()) activate(i);
        return true;
    }
    if (e.type == T::KeyDown) {
        int n = int(items_.size());
        if (e.key == "down" || e.key == "up") {
            int d = e.key == "down" ? 1 : -1;
            for (int k = 0; k < n; ++k) {
                hover_ = ((hover_ < 0 ? (d > 0 ? -1 : 0) : hover_) + d + n) % n;
                if (itemEnabled(items_[size_t(hover_)])) break;
            }
            return true;
        }
        if ((e.key == "enter" || e.key == "space") && hover_ >= 0) {
            activate(hover_);
            return true;
        }
    }
    return e.isMouse();
}

PopupMenu* PopupMenu::open(Context& ctx, std::vector<MenuItem> items, Rect anchor, PopupPlacement place) {
    auto menu = std::make_unique<PopupMenu>(std::move(items));
    menu->focusMode = FocusMode::All;
    PopupMenu* p = menu.get();
    ctx.openPopup(std::move(menu), anchor, place);
    ctx.setFocus(p);
    return p;
}

// ---------------------------------------------------------------------------------- menu bar
std::vector<Rect> MenuBar::titleRects() const {
    std::vector<Rect> out;
    float x = rect_.x + 4;
    for (const auto& m : menus_) {
        float w = font().measure(m.title, fontSize()) + 18;
        out.push_back({x, rect_.y, w, rect_.h});
        x += w;
    }
    return out;
}

Vec2 MenuBar::computeMinSize() {
    float w = 8;
    for (const auto& m : menus_) w += font().measure(m.title, fontSize()) + 18;
    return {w, std::ceil(font().lineHeight(fontSize()) + 10)};
}

void MenuBar::draw(Canvas& c) {
    c.styleBox(rect_, box("panel"));
    auto rects = titleRects();
    Context* ctx = context();
    if (open_ >= 0 && ctx && !ctx->isPopupOpen(popup_)) open_ = -1;
    for (size_t i = 0; i < rects.size(); ++i) {
        bool hot = int(i) == open_ || int(i) == hover_;
        if (hot) c.styleBox(rects[i].shrink(0, 2, 0, 2), theme().box("ToolButton", int(i) == open_ ? "pressed" : "hover"));
        c.textIn(rects[i], menus_[i].title, font(), fontSize(), color("font_color"), Align::Center);
    }
}

void MenuBar::openMenu(int i) {
    Context* ctx = context();
    if (!ctx) return;
    ctx->closeAllPopups();
    auto rects = titleRects();
    popup_ = PopupMenu::open(*ctx, menus_[size_t(i)].items, rects[size_t(i)], PopupPlacement::Below);
    open_ = i;
}

bool MenuBar::event(const InputEvent& e) {
    using T = InputEvent::Type;
    auto rects = titleRects();
    int at = -1;
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].contains(e.pos)) at = int(i);
    if (e.type == T::MouseMove) {
        hover_ = at;
        Context* ctx = context();
        // Moving across titles while a menu is open switches menus.
        if (at >= 0 && open_ >= 0 && at != open_ && ctx && ctx->isPopupOpen(popup_)) openMenu(at);
        return true;
    }
    if (e.type == T::MouseDown && e.button == MouseButton::Left && at >= 0) {
        Context* ctx = context();
        if (open_ == at && ctx && ctx->isPopupOpen(popup_)) {
            ctx->closeAllPopups();
            open_ = -1;
        } else {
            openMenu(at);
        }
        return true;
    }
    return e.isMouse();
}

// ---------------------------------------------------------------------------------- option button
void OptionButton::setItems(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_ >= int(items_.size())) selected_ = items_.empty() ? -1 : 0;
    if (selected_ < 0 && !items_.empty()) selected_ = 0;
    text = selectedText();
}

void OptionButton::select(int i, bool notify) {
    if (i < 0 || size_t(i) >= items_.size()) return;
    bool changed = i != selected_;
    selected_ = i;
    text = selectedText();
    if (changed && notify) itemSelected.emit(i);
}

Vec2 OptionButton::computeMinSize() {
    Vec2 m = Button::computeMinSize();
    float w = 0;
    for (const auto& s : items_) w = std::max(w, font().measure(s, fontSize()));
    const StyleBox& b = box("normal");
    m.x = std::max(m.x, std::ceil(w + b.margin[0] + b.margin[2] + 18));
    return m;
}

void OptionButton::draw(Canvas& c) {
    Button::draw(c);
    const StyleBox& b = currentBox();
    Rect inner = rect_.shrink(b.margin[0], b.margin[1], b.margin[2], b.margin[3]);
    c.icon({inner.right() - 10, inner.y, 10, inner.h}, iconCodepoint("caret-down"), iconFont(), fontSize() * 0.85f, currentFontColor());
}

void OptionButton::activate() {
    Context* ctx = context();
    if (!ctx) return;
    std::vector<MenuItem> menu;
    for (size_t i = 0; i < items_.size(); ++i) {
        MenuItem m;
        m.label = items_[i];
        int idx = int(i);
        m.run = [this, idx] { select(idx); };
        m.checked = [this, idx] { return selected_ == idx; };
        menu.push_back(std::move(m));
    }
    auto* p = PopupMenu::open(*ctx, std::move(menu), rect_, PopupPlacement::Below);
    p->customMinSize.x = rect_.w;
}

bool OptionButton::setProperty(std::string_view key, std::string_view v) {
    if (key == "items") {
        std::vector<std::string> it;
        for (auto s : split(v, ';', false)) it.emplace_back(trim(s));
        setItems(std::move(it));
    } else if (key == "selected") select(parseInt(v), false);
    else return Button::setProperty(key, v);
    return true;
}

bool OptionButton::connectSignal(std::string_view s, std::function<void()> fn) {
    if (s == "item_selected") {
        itemSelected.connect([fn](int) { fn(); });
        return true;
    }
    return Button::connectSignal(s, std::move(fn));
}

void MenuButton::activate() {
    Context* ctx = context();
    if (!ctx) return;
    PopupMenu::open(*ctx, itemsProvider ? itemsProvider() : items, rect_, PopupPlacement::Below);
}

}  // namespace ose::ui
