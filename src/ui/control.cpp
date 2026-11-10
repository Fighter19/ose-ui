#include "ui/control.h"

#include <algorithm>
#include <unordered_set>

#include "core/util.h"
#include "ui/context.h"

namespace ose::ui {

namespace {
std::unordered_set<const Control*>& live() {
    static std::unordered_set<const Control*> s;
    return s;
}
}  // namespace

bool isAlive(const Control* c) { return c && live().count(c) != 0; }

bool parseBool(std::string_view v) {
    v = trim(v);
    return !(v.empty() || v == "0" || iequals(v, "false") || iequals(v, "no") || iequals(v, "off"));
}

Vec2 parseSize(std::string_view v) {
    auto p = splitWhitespace(v);
    if (p.empty()) return {};
    float w = parseFloat(p[0]);
    return {w, p.size() > 1 ? parseFloat(p[1]) : w};
}

int parseSizeFlags(std::string_view v) {
    int f = 0;
    for (auto part : splitWhitespace(v)) {
        std::string s = toLower(part);
        if (s == "fill") f |= SizeFlags::Fill;
        else if (s == "expand") f |= SizeFlags::Expand;
        else if (s == "shrink_center" || s == "center") f |= SizeFlags::ShrinkCenter;
        else if (s == "shrink_end" || s == "end") f |= SizeFlags::ShrinkEnd;
        else if (s == "shrink_begin" || s == "begin") f |= 0;
    }
    return f;
}

Control::Control() { live().insert(this); }

Control::~Control() {
    children_.clear();
    live().erase(this);
}

Control* Control::addChild(std::unique_ptr<Control> c, int index) {
    c->parent_ = this;
    Control* p = c.get();
    if (index < 0 || size_t(index) >= children_.size()) children_.push_back(std::move(c));
    else children_.insert(children_.begin() + index, std::move(c));
    return p;
}

std::unique_ptr<Control> Control::removeChild(Control* c) {
    for (auto it = children_.begin(); it != children_.end(); ++it)
        if (it->get() == c) {
            std::unique_ptr<Control> out = std::move(*it);
            children_.erase(it);
            out->parent_ = nullptr;
            return out;
        }
    return nullptr;
}

void Control::clearChildren() { children_.clear(); }

void Control::moveChild(Control* c, int index) {
    auto owned = removeChild(c);
    if (owned) addChild(std::move(owned), index);
}

int Control::indexOf(const Control* c) const {
    for (size_t i = 0; i < children_.size(); ++i)
        if (children_[i].get() == c) return int(i);
    return -1;
}

Control* Control::find(std::string_view want) {
    if (id == want) return this;
    for (auto& c : children_)
        if (Control* f = c->find(want)) return f;
    return nullptr;
}

bool Control::isAncestorOf(const Control* c) const {
    for (const Control* p = c; p; p = p->parent_)
        if (p == this) return true;
    return false;
}

Context* Control::context() const {
    const Control* c = this;
    while (c->parent_) c = c->parent_;
    return c->ctx_;
}

void Control::setVisible(bool v) {
    if (visible_ == v) return;
    visible_ = v;
    visibilityChanged.emit(v);
}

bool Control::visibleInTree() const {
    for (const Control* c = this; c; c = c->parent_)
        if (!c->visible_) return false;
    return true;
}

bool Control::isDisabled() const {
    for (const Control* c = this; c; c = c->parent_)
        if (c->disabled) return true;
    return false;
}

void Control::setAnchorPreset(std::string_view p) {
    auto set = [&](float l, float t, float r, float b) {
        anchors[0] = l, anchors[1] = t, anchors[2] = r, anchors[3] = b;
        for (float& o : offsets) o = 0;
    };
    if (p == "full") set(0, 0, 1, 1);
    else if (p == "top") set(0, 0, 1, 0);
    else if (p == "bottom") set(0, 1, 1, 1);
    else if (p == "left") set(0, 0, 0, 1);
    else if (p == "right") set(1, 0, 1, 1);
    else if (p == "center") set(0.5f, 0.5f, 0.5f, 0.5f);
    else if (p == "top-left") set(0, 0, 0, 0);
    else if (p == "top-right") set(1, 0, 1, 0);
    else if (p == "bottom-left") set(0, 1, 0, 1);
    else if (p == "bottom-right") set(1, 1, 1, 1);
}

void Control::setRect(const Rect& r) {
    bool changed = !(r == rect_);
    rect_ = r;
    if (changed) resized.emit();
}

Vec2 Control::minimumSize() {
    Context* ctx = context();
    uint64_t f = ctx ? ctx->frame() : 0;
    if (minFrame_ != f || !ctx) {
        Vec2 m = computeMinSize();
        cachedMin_ = {std::max(m.x, customMinSize.x), std::max(m.y, customMinSize.y)};
        minFrame_ = f;
    }
    return cachedMin_;
}

Vec2 Control::computeMinSize() { return {}; }

void Control::layout() {
    for (auto& c : children_) {
        if (!c->visible_) continue;
        Vec2 m = c->minimumSize();
        float l = rect_.x + c->anchors[0] * rect_.w + c->offsets[0];
        float t = rect_.y + c->anchors[1] * rect_.h + c->offsets[1];
        float r = rect_.x + c->anchors[2] * rect_.w + c->offsets[2];
        float b = rect_.y + c->anchors[3] * rect_.h + c->offsets[3];
        float w = std::max(r - l, m.x), h = std::max(b - t, m.y);
        // Centred anchors grow both ways.
        if (c->anchors[0] == c->anchors[2] && c->anchors[0] == 0.5f) l -= (w - (r - l)) * 0.5f;
        if (c->anchors[1] == c->anchors[3] && c->anchors[1] == 0.5f) t -= (h - (b - t)) * 0.5f;
        if (c->anchors[0] == 1 && c->anchors[2] == 1) l = r - w;
        if (c->anchors[1] == 1 && c->anchors[3] == 1) t = b - h;
        c->setRect({l, t, w, h});
    }
}

void Control::layoutTree() {
    layout();
    for (auto& c : children_)
        if (c->visible_) c->layoutTree();
}

bool Control::hovered() const {
    Context* ctx = context();
    return ctx && ctx->hovered() == this;
}

bool Control::hasFocus() const {
    Context* ctx = context();
    return ctx && ctx->focus() == this;
}

void Control::grabFocus() {
    if (Context* ctx = context()) ctx->setFocus(this);
}

void Control::releaseFocus() {
    if (Context* ctx = context(); ctx && ctx->focus() == this) ctx->setFocus(nullptr);
}

const Theme& Control::theme() const {
    Context* ctx = context();
    static Theme fallback;
    return ctx ? ctx->theme() : fallback;
}

const Font& Control::font() const { return *theme().font; }
float Control::fontSize() const { return theme().fontSize; }
const Font& Control::iconFont() const { return theme().iconFont ? *theme().iconFont : *theme().font; }

bool Control::setProperty(std::string_view key, std::string_view v) {
    if (key == "id") id = std::string(v);
    else if (key == "theme-type" || key == "variation") themeType = std::string(v);
    else if (key == "tooltip") tooltip = std::string(v);
    else if (key == "title" || key == "name") title = std::string(v);
    else if (key == "icon") icon = std::string(v);
    else if (key == "min-size") customMinSize = parseSize(v);
    else if (key == "min-width") customMinSize.x = parseFloat(v);
    else if (key == "min-height") customMinSize.y = parseFloat(v);
    else if (key == "size-flags-h" || key == "h") sizeFlagsH = parseSizeFlags(v);
    else if (key == "size-flags-v" || key == "v") sizeFlagsV = parseSizeFlags(v);
    else if (key == "expand") {
        if (parseBool(v)) sizeFlagsH |= SizeFlags::Expand, sizeFlagsV |= SizeFlags::Expand;
    } else if (key == "stretch") stretchRatio = parseFloat(v, 1);
    else if (key == "anchors") {
        auto p = splitWhitespace(v);
        if (p.size() == 1) setAnchorPreset(p[0]);
        else
            for (size_t k = 0; k < 4 && k < p.size(); ++k) anchors[k] = parseFloat(p[k]);
    } else if (key == "offsets") {
        auto p = splitWhitespace(v);
        for (size_t k = 0; k < 4 && k < p.size(); ++k) offsets[k] = parseFloat(p[k]);
    } else if (key == "visible") setVisible(parseBool(v));
    else if (key == "disabled") disabled = parseBool(v);
    else if (key == "enabled") disabled = !parseBool(v);
    else if (key == "clip") clipContents = parseBool(v);
    else if (key == "mouse-filter") mouseFilter = v == "pass" ? MouseFilter::Pass : (v == "ignore" ? MouseFilter::Ignore : MouseFilter::Stop);
    else if (key == "focus") focusMode = v == "all" ? FocusMode::All : (v == "click" ? FocusMode::Click : FocusMode::None);
    else return false;
    return true;
}

bool Control::connectSignal(std::string_view signal, std::function<void()> fn) {
    if (signal == "resized") {
        resized.connect(std::move(fn));
        return true;
    }
    return false;
}

}  // namespace ose::ui
