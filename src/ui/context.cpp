#include "ui/context.h"

#include <algorithm>

#include "core/util.h"

namespace ose::ui {

// ---------------------------------------------------------------------------------- actions
Action& ActionRegistry::add(Action a) {
    if (Action* existing = find(a.name)) {
        *existing = std::move(a);
        return *existing;
    }
    actions_.push_back(std::make_unique<Action>(std::move(a)));
    Action* p = actions_.back().get();
    byName_[p->name] = p;
    return *p;
}

Action* ActionRegistry::find(std::string_view name) {
    auto it = byName_.find(std::string(name));
    return it == byName_.end() ? nullptr : it->second;
}

const Action* ActionRegistry::find(std::string_view name) const {
    auto it = byName_.find(std::string(name));
    return it == byName_.end() ? nullptr : it->second;
}

bool ActionRegistry::isEnabled(std::string_view name) const {
    const Action* a = find(name);
    return a && (!a->enabled || a->enabled());
}

bool ActionRegistry::isChecked(std::string_view name) const {
    const Action* a = find(name);
    return a && a->checked && a->checked();
}

bool ActionRegistry::run(std::string_view name) {
    Action* a = find(name);
    if (!a || !a->run || (a->enabled && !a->enabled())) return false;
    a->run();
    return true;
}

bool ActionRegistry::matches(std::string_view shortcut, const InputEvent& e) {
    if (shortcut.empty() || e.type != InputEvent::Type::KeyDown) return false;
    int mods = 0;
    std::string key;
    size_t start = 0;
    for (size_t i = 0; i <= shortcut.size(); ++i) {
        if (i < shortcut.size() && shortcut[i] != '+') continue;
        std::string part = toLower(trim(shortcut.substr(start, i - start)));
        start = i + 1;
        if (part.empty()) {
            key = "+";
            continue;
        }
        if (part == "ctrl" || part == "control" || part == "cmd") mods |= Mod::Ctrl;
        else if (part == "shift") mods |= Mod::Shift;
        else if (part == "alt") mods |= Mod::Alt;
        else key = part;
    }
    int em = e.mods & (Mod::Ctrl | Mod::Shift | Mod::Alt);
    if (e.mods & Mod::Super) em |= Mod::Ctrl;  // Cmd on macOS
    return em == mods && iequals(key, e.key);
}

bool ActionRegistry::handleShortcut(const InputEvent& e) {
    for (auto& a : actions_)
        if (matches(a->shortcut, e) && run(a->name)) return true;
    return false;
}

std::vector<const Action*> ActionRegistry::all() const {
    std::vector<const Action*> out;
    for (const auto& a : actions_) out.push_back(a.get());
    return out;
}

std::optional<std::string> Bindings::get(std::string_view expr, const ActionRegistry* actions) const {
    expr = trim(expr);
    bool neg = false;
    while (expr.starts_with('!')) {
        neg = !neg;
        expr = trim(expr.substr(1));
    }
    std::optional<std::string> v;
    auto it = values_.find(std::string(expr));
    if (it != values_.end()) v = it->second();
    else if (actions) {
        if (expr.ends_with(".enabled")) v = actions->isEnabled(expr.substr(0, expr.size() - 8)) ? "1" : "0";
        else if (expr.ends_with(".checked")) v = actions->isChecked(expr.substr(0, expr.size() - 8)) ? "1" : "0";
    }
    if (!v) return std::nullopt;
    if (neg) return parseBool(*v) ? std::string("0") : std::string("1");
    return v;
}

// ---------------------------------------------------------------------------------- context
Context::Context(std::shared_ptr<Theme> theme) : theme_(std::move(theme)) {}

Context::~Context() {
    overlays_.clear();
    if (root_) root_->ctx_ = nullptr;
}

void Context::setRoot(std::unique_ptr<Control> root) {
    if (root_) root_->ctx_ = nullptr;
    root_ = std::move(root);
    if (root_) root_->ctx_ = this;
}

std::unique_ptr<Control> Context::takeRoot() {
    if (root_) root_->ctx_ = nullptr;
    return std::move(root_);
}

void Context::resize(float w, float h) {
    width_ = w;
    height_ = h;
}

Control* Context::openPopup(std::unique_ptr<Control> popup, Rect anchor, PopupPlacement place, bool modal) {
    popup->ctx_ = this;
    Control* p = popup.get();
    overlays_.push_back({std::move(popup), anchor, place, modal});
    return p;
}

void Context::closePopup(Control* popup) {
    for (size_t i = 0; i < overlays_.size(); ++i)
        if (overlays_[i].control.get() == popup) {
            // Popups opened from this one close with it.
            for (size_t k = overlays_.size(); k-- > i;) {
                Control* c = overlays_[k].control.get();
                if (focus_ && c->isAncestorOf(focus_)) focus_ = nullptr;
                graveyard_.push_back(std::move(overlays_[k].control));
                overlays_.erase(overlays_.begin() + long(k));
                popupClosed.emit(c);
            }
            return;
        }
}

void Context::closePopupsAbove(Control* popup) {
    for (size_t i = 0; i < overlays_.size(); ++i)
        if (overlays_[i].control.get() == popup) {
            if (i + 1 < overlays_.size()) closePopup(overlays_[i + 1].control.get());
            return;
        }
}

void Context::closeAllPopups() {
    if (!overlays_.empty()) closePopup(overlays_[0].control.get());
}

bool Context::isPopupOpen(const Control* popup) const {
    for (const auto& o : overlays_)
        if (o.control.get() == popup) return true;
    return false;
}

void Context::setFocus(Control* c) {
    Control* old = focus();
    if (old == c) return;
    focus_ = c;
    if (old) old->focusChanged(false);
    if (c) c->focusChanged(true);
}

Control* Context::pickIn(Control* c, Vec2 p, bool forDrop) const {
    if (!c->visible() || (!c->hitTest(p) && c->clipContents)) return nullptr;
    for (size_t i = c->childCount(); i-- > 0;)
        if (Control* hit = pickIn(c->child(i), p, forDrop)) return hit;
    if (c->mouseFilter == MouseFilter::Ignore) return nullptr;
    return c->hitTest(p) ? c : nullptr;
}

Control* Context::pick(Vec2 p, bool forDrop) const {
    for (size_t i = overlays_.size(); i-- > 0;) {
        Control* o = overlays_[i].control.get();
        if (Control* hit = pickIn(o, p, forDrop)) return hit;
        if (overlays_[i].modal) return nullptr;  // nothing below a modal dialog
    }
    return root_ ? pickIn(root_.get(), p, forDrop) : nullptr;
}

bool Context::deliver(Control* target, const InputEvent& e) {
    for (Control* c = target; c; c = c->parent()) {
        if (!isAlive(c)) return true;
        if (c->isDisabled()) {
            if (c->mouseFilter == MouseFilter::Stop && e.isMouse()) return true;
            continue;
        }
        if (c->event(e)) return true;
        if (c->mouseFilter == MouseFilter::Stop && e.isMouse()) return true;
    }
    return false;
}

void Context::updateHover(Vec2 p) {
    Control* h = capture() ? capture() : pick(p);
    Control* old = hovered();
    if (h != old) {
        if (old) old->mouseExited();
        hover_ = h;
        if (h) h->mouseEntered();
        hoverSince_ = time_;
        tipFor_ = nullptr;
    }
    cursor_ = h ? h->cursor(p) : Cursor::Arrow;
    if (dragging()) cursor_ = dropTarget_ ? Cursor::Hand : Cursor::Forbidden;
}

void Context::dispatch(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.isMouse()) {
        mouse_ = e.pos;
        mods_ = e.mods;
    }
    if (e.type == T::KeyDown || e.type == T::KeyUp) mods_ = e.mods;
    if (Control* g = inputGrab(); g && (e.isMouse() || e.type == T::KeyDown || e.type == T::KeyUp || e.type == T::Text)) {
        if (e.type == T::MouseDown) buttons_ |= 1 << int(e.button);
        if (e.type == T::MouseUp) buttons_ &= ~(1 << int(e.button));
        g->event(e);
        return;
    }
    switch (e.type) {
        case T::MouseMove: {
            // Drag and drop: start once the pointer moved far enough from the press.
            if (pressed_ && isAlive(pressed_) && !drag_ && !dragChecked_ && mouseDown(MouseButton::Left)) {
                Vec2 d = e.pos - pressPos_;
                if (d.x * d.x + d.y * d.y > 36) {
                    dragChecked_ = true;
                    for (Control* c = pressed_; c; c = c->parent()) {
                        DragPayload p;
                        if (c->dragData(pressPos_, p)) {
                            drag_ = std::move(p);
                            capture_ = nullptr;
                            break;
                        }
                    }
                }
            }
            if (drag_) {
                dropTarget_ = nullptr;
                for (Control* c = pick(e.pos, true); c; c = c->parent())
                    if (c->canDrop(e.pos, *drag_)) {
                        dropTarget_ = c;
                        break;
                    }
                updateHover(e.pos);
                return;
            }
            updateHover(e.pos);
            Control* target = capture() ? capture() : hovered();
            if (target) deliver(target, e);
            return;
        }
        case T::MouseDown: {
            buttons_ |= 1 << int(e.button);
            tipFor_ = nullptr;
            Control* hit = pick(e.pos);
            // A press outside the open popups closes them (modal dialogs stay).
            if (!overlays_.empty()) {
                Control* top = nullptr;
                for (size_t i = overlays_.size(); i-- > 0;)
                    if (overlays_[i].control->isAncestorOf(hit)) top = overlays_[i].control.get();
                if (!top) {
                    bool modal = false;
                    for (const auto& o : overlays_) modal |= o.modal;
                    if (modal) return;
                    closeAllPopups();
                    graveyard_.clear();
                    updateHover(e.pos);
                    return;  // the click only closes the popup
                }
                closePopupsAbove(top);
            }
            if (hit) {
                // Focus: the first control up the chain that takes it.
                // (Clicks inside the focused control keep its focus.)
                Control* cur = focus();
                if (!(cur && cur->isAncestorOf(hit))) {
                    Control* f = nullptr;
                    for (Control* c = hit; c; c = c->parent())
                        if (c->focusMode != FocusMode::None && !c->isDisabled()) {
                            f = c;
                            break;
                        }
                    setFocus(f);
                }
                capture_ = hit;
                pressed_ = hit;
                pressPos_ = e.pos;
                dragChecked_ = false;
                deliver(hit, e);
            } else {
                setFocus(nullptr);
            }
            graveyard_.clear();
            return;
        }
        case T::MouseUp: {
            buttons_ &= ~(1 << int(e.button));
            if (drag_) {
                if (dropTarget_ && isAlive(dropTarget_)) dropTarget_->drop(e.pos, *drag_);
                drag_.reset();
                dropTarget_ = nullptr;
                pressed_ = nullptr;
                capture_ = nullptr;
                updateHover(e.pos);
                return;
            }
            Control* target = capture() ? capture() : pick(e.pos);
            if (buttons_ == 0) capture_ = nullptr;
            pressed_ = nullptr;
            if (target) deliver(target, e);
            graveyard_.clear();
            updateHover(e.pos);
            return;
        }
        case T::Wheel: {
            Control* target = pick(e.pos);
            if (target) deliver(target, e);
            return;
        }
        case T::KeyDown:
        case T::KeyUp:
        case T::Text: {
            if (e.type == T::KeyDown && e.key == "escape" && drag_) {
                drag_.reset();
                return;
            }
            Control* f = focus();
            if (f && f->visibleInTree() && deliver(f, e)) return;
            if (e.type == T::KeyDown && e.key == "escape" && !overlays_.empty()) {
                closePopup(overlays_.back().control.get());
                return;
            }
            if (e.type == T::KeyDown && actions_.handleShortcut(e)) return;
            // Keys nobody took go to the control under the pointer (viewports).
            if (Control* h = hovered(); h && h != f) deliver(h, e);
            return;
        }
        case T::FilesDropped: {
            DragPayload p;
            p.type = "files";
            p.data = e.files;
            for (Control* c = pick(e.pos, true); c; c = c->parent())
                if (c->canDrop(e.pos, p)) {
                    c->drop(e.pos, p);
                    return;
                }
            if (filesDropped) filesDropped(e.files, e.pos);
            return;
        }
        case T::Leave:
            if (Control* old = hovered()) old->mouseExited();
            hover_ = nullptr;
            return;
    }
}

void Context::processTree(Control* c, float dt) {
    for (auto& b : c->binders) b();
    c->process(dt);
    for (size_t i = 0; i < c->childCount(); ++i) processTree(c->child(i), dt);
}

void Context::layoutOverlay(size_t i) {
    Overlay& o = overlays_[i];
    Control* c = o.control.get();
    Vec2 m = c->minimumSize();
    Rect r{0, 0, std::max(m.x, c->customMinSize.x), m.y};
    switch (o.place) {
        case PopupPlacement::Below:
            r.x = o.anchor.x;
            r.y = o.anchor.bottom();
            if (r.y + r.h > height_) r.y = std::max(0.0f, o.anchor.y - r.h);
            break;
        case PopupPlacement::Right:
            r.x = o.anchor.right();
            r.y = o.anchor.y;
            if (r.x + r.w > width_) r.x = std::max(0.0f, o.anchor.x - r.w);
            break;
        case PopupPlacement::AtPoint:
            r.x = o.anchor.x;
            r.y = o.anchor.y;
            break;
        case PopupPlacement::Center:
            r.x = (width_ - r.w) * 0.5f;
            r.y = (height_ - r.h) * 0.4f;
            break;
    }
    r.x = clampv(r.x, 0.0f, std::max(0.0f, width_ - r.w));
    r.y = clampv(r.y, 0.0f, std::max(0.0f, height_ - r.h));
    r.h = std::min(r.h, height_);
    c->setRect(r);
    c->layoutTree();
}

void Context::process(float dt) {
    time_ += dt;
    ++frame_;
    graveyard_.clear();
    if (root_) {
        processTree(root_.get(), dt);
        root_->setRect({0, 0, width_, height_});
        root_->layoutTree();
    }
    for (size_t i = 0; i < overlays_.size(); ++i) {
        processTree(overlays_[i].control.get(), dt);
        layoutOverlay(i);
    }
    // Tooltip after hovering still for a while.
    Control* h = hovered();
    if (h && !h->tooltip.empty() && !dragging() && buttons_ == 0 && time_ - hoverSince_ > 0.6) tipFor_ = h;
}

void Context::drawTree(Control* c, Canvas& canvas) {
    if (!c->visible()) return;
    if (c->clipContents) canvas.pushClip(c->rect());
    c->draw(canvas);
    for (size_t i = 0; i < c->childCount(); ++i) drawTree(c->child(i), canvas);
    c->drawOver(canvas);
    if (c->clipContents) canvas.popClip();
}

void Context::draw(Canvas& canvas) {
    uint32_t gen = atlas_.generation();
    for (int attempt = 0; attempt < 2; ++attempt) {
        canvas.clear(width_, height_);
        if (root_) drawTree(root_.get(), canvas);
        for (auto& o : overlays_) {
            if (o.modal) canvas.rect({0, 0, width_, height_}, {0, 0, 0, 0.35f});
            drawTree(o.control.get(), canvas);
        }
        const Theme& th = *theme_;
        if (th.font) {
            // Drop target highlight and the drag preview.
            if (drag_) {
                if (dropTarget_ && isAlive(dropTarget_))
                    canvas.border(dropTarget_->rect(), th.color("*", "accent"), 2, 3);
                std::string label = drag_->label.empty() ? drag_->type : drag_->label;
                float px = th.fontSize;
                float w = th.font->measure(label, px) + 16;
                Rect r{mouse_.x + 14, mouse_.y + 10, w, th.font->lineHeight(px) + 8};
                canvas.styleBox(r, th.box("Tooltip", "panel"));
                canvas.textIn(r.shrink(8, 0, 8, 0), label, *th.font, px, th.color("Tooltip", "font_color"));
            }
            if (tipFor_ && isAlive(tipFor_)) {
                float px = th.fontSize;
                const std::string& t = tipFor_->tooltip;
                float w = std::min(th.font->measure(t, px) + 16, 480.0f);
                Rect r{mouse_.x + 12, mouse_.y + 18, w, th.font->lineHeight(px) + 10};
                if (r.right() > width_) r.x = width_ - r.w - 2;
                if (r.bottom() > height_) r.y = mouse_.y - r.h - 4;
                canvas.styleBox(r, th.box("Tooltip", "panel"));
                canvas.textIn(r.shrink(8, 0, 8, 0), t, *th.font, px, th.color("Tooltip", "font_color"));
            }
        }
        // The glyph atlas restarted while drawing: earlier glyphs point at old places.
        if (atlas_.generation() == gen) break;
        gen = atlas_.generation();
    }
}

}  // namespace ose::ui
