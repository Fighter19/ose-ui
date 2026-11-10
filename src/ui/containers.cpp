#include "ui/containers.h"

#include <algorithm>
#include <cmath>

#include "core/util.h"
#include "ui/context.h"

namespace ose::ui {

void Container::fitChild(Control* c, const Rect& area) {
    Vec2 m = c->minimumSize();
    Rect r = area;
    if (!(c->sizeFlagsH & SizeFlags::Fill)) {
        r.w = std::min(m.x, area.w);
        if (c->sizeFlagsH & SizeFlags::ShrinkCenter) r.x = area.x + std::floor((area.w - r.w) * 0.5f);
        else if (c->sizeFlagsH & SizeFlags::ShrinkEnd) r.x = area.right() - r.w;
    }
    if (!(c->sizeFlagsV & SizeFlags::Fill)) {
        r.h = std::min(m.y, area.h);
        if (c->sizeFlagsV & SizeFlags::ShrinkCenter) r.y = area.y + std::floor((area.h - r.h) * 0.5f);
        else if (c->sizeFlagsV & SizeFlags::ShrinkEnd) r.y = area.bottom() - r.h;
    }
    c->setRect(r);
}

// ---------------------------------------------------------------------------------- box
Vec2 BoxContainer::computeMinSize() {
    Vec2 m;
    int n = 0;
    for (auto& c : children_) {
        if (!c->visible()) continue;
        Vec2 cm = c->minimumSize();
        if (vertical_) m.y += cm.y, m.x = std::max(m.x, cm.x);
        else m.x += cm.x, m.y = std::max(m.y, cm.y);
        ++n;
    }
    if (n > 1) (vertical_ ? m.y : m.x) += sep() * float(n - 1);
    return m;
}

void BoxContainer::layout() {
    std::vector<Control*> vis;
    float minTotal = 0, stretchTotal = 0;
    for (auto& c : children_)
        if (c->visible()) {
            vis.push_back(c.get());
            Vec2 m = c->minimumSize();
            minTotal += vertical_ ? m.y : m.x;
            int flags = vertical_ ? c->sizeFlagsV : c->sizeFlagsH;
            if (flags & SizeFlags::Expand) stretchTotal += c->stretchRatio;
        }
    if (vis.empty()) return;
    float s = sep();
    float avail = (vertical_ ? rect_.h : rect_.w) - minTotal - s * float(vis.size() - 1);
    float extra = std::max(0.0f, avail);
    float pos = vertical_ ? rect_.y : rect_.x;
    if (stretchTotal <= 0 && extra > 0) {
        if (alignment == Align::Center) pos += std::floor(extra * 0.5f);
        else if (alignment == Align::End) pos += extra;
    }
    for (Control* c : vis) {
        Vec2 m = c->minimumSize();
        float size = vertical_ ? m.y : m.x;
        int flags = vertical_ ? c->sizeFlagsV : c->sizeFlagsH;
        if ((flags & SizeFlags::Expand) && stretchTotal > 0) size += std::floor(extra * c->stretchRatio / stretchTotal);
        Rect area = vertical_ ? Rect{rect_.x, pos, rect_.w, size} : Rect{pos, rect_.y, size, rect_.h};
        // Along the box axis the child gets the slot; across it the size flags apply.
        if (vertical_) {
            int keep = c->sizeFlagsV;
            c->sizeFlagsV |= SizeFlags::Fill;
            fitChild(c, area);
            c->sizeFlagsV = keep;
        } else {
            int keep = c->sizeFlagsH;
            c->sizeFlagsH |= SizeFlags::Fill;
            fitChild(c, area);
            c->sizeFlagsH = keep;
        }
        pos += size + s;
    }
}

bool BoxContainer::setProperty(std::string_view key, std::string_view v) {
    if (key == "separation") separation = parseFloat(v);
    else if (key == "align") alignment = v == "center" ? Align::Center : (v == "end" ? Align::End : Align::Begin);
    else return Container::setProperty(key, v);
    return true;
}

// ---------------------------------------------------------------------------------- grid
Vec2 GridContainer::computeMinSize() {
    std::vector<float> colW(size_t(std::max(1, columns)), 0);
    std::vector<float> rowH;
    int i = 0;
    for (auto& c : children_) {
        if (!c->visible()) continue;
        Vec2 m = c->minimumSize();
        size_t col = size_t(i % columns), row = size_t(i / columns);
        if (rowH.size() <= row) rowH.push_back(0);
        colW[col] = std::max(colW[col], m.x);
        rowH[row] = std::max(rowH[row], m.y);
        ++i;
    }
    float hs = hSeparation >= 0 ? hSeparation : constant("h_separation"), vs = vSeparation >= 0 ? vSeparation : constant("v_separation");
    Vec2 out;
    for (float w : colW) out.x += w;
    for (float h : rowH) out.y += h;
    out.x += hs * float(std::max<size_t>(1, colW.size()) - 1);
    out.y += vs * float(std::max<size_t>(1, rowH.size()) - 1);
    return out;
}

void GridContainer::layout() {
    int cols = std::max(1, columns);
    std::vector<float> colW(size_t(cols), 0);
    std::vector<bool> colExpand(size_t(cols), false);
    std::vector<float> rowH;
    std::vector<bool> rowExpand;
    std::vector<Control*> vis;
    for (auto& c : children_)
        if (c->visible()) vis.push_back(c.get());
    for (size_t i = 0; i < vis.size(); ++i) {
        Vec2 m = vis[i]->minimumSize();
        size_t col = i % size_t(cols), row = i / size_t(cols);
        if (rowH.size() <= row) rowH.push_back(0), rowExpand.push_back(false);
        colW[col] = std::max(colW[col], m.x);
        rowH[row] = std::max(rowH[row], m.y);
        if (vis[i]->sizeFlagsH & SizeFlags::Expand) colExpand[col] = true;
        if (vis[i]->sizeFlagsV & SizeFlags::Expand) rowExpand[row] = true;
    }
    float hs = hSeparation >= 0 ? hSeparation : constant("h_separation"), vs = vSeparation >= 0 ? vSeparation : constant("v_separation");
    auto distribute = [](std::vector<float>& sizes, const std::vector<bool>& expand, float total, float sep) {
        float used = sep * float(std::max<size_t>(1, sizes.size()) - 1);
        int ex = 0;
        for (size_t i = 0; i < sizes.size(); ++i) used += sizes[i], ex += expand[i];
        float extra = total - used;
        if (extra > 0 && ex > 0)
            for (size_t i = 0; i < sizes.size(); ++i)
                if (expand[i]) sizes[i] += std::floor(extra / float(ex));
    };
    distribute(colW, colExpand, rect_.w, hs);
    distribute(rowH, rowExpand, rect_.h, vs);
    float y = rect_.y;
    for (size_t row = 0; row < rowH.size(); ++row) {
        float x = rect_.x;
        for (size_t col = 0; col < size_t(cols); ++col) {
            size_t i = row * size_t(cols) + col;
            if (i < vis.size()) fitChild(vis[i], {x, y, colW[col], rowH[row]});
            x += colW[col] + hs;
        }
        y += rowH[row] + vs;
    }
}

bool GridContainer::setProperty(std::string_view key, std::string_view v) {
    if (key == "columns") columns = std::max(1, parseInt(v, 2));
    else if (key == "h-separation") hSeparation = parseFloat(v);
    else if (key == "v-separation") vSeparation = parseFloat(v);
    else return Container::setProperty(key, v);
    return true;
}

// ---------------------------------------------------------------------------------- margin
Vec2 MarginContainer::computeMinSize() {
    Vec2 m;
    for (auto& c : children_)
        if (c->visible()) {
            Vec2 cm = c->minimumSize();
            m.x = std::max(m.x, cm.x), m.y = std::max(m.y, cm.y);
        }
    return {m.x + margins[0] + margins[2], m.y + margins[1] + margins[3]};
}

void MarginContainer::layout() {
    Rect inner = rect_.shrink(margins[0], margins[1], margins[2], margins[3]);
    for (auto& c : children_)
        if (c->visible()) fitChild(c.get(), inner);
}

bool MarginContainer::setProperty(std::string_view key, std::string_view v) {
    if (key == "margins" || key == "margin") {
        auto p = splitWhitespace(v);
        if (p.size() == 1)
            for (float& m : margins) m = parseFloat(p[0]);
        else if (p.size() == 2) margins[0] = margins[2] = parseFloat(p[0]), margins[1] = margins[3] = parseFloat(p[1]);
        else
            for (size_t k = 0; k < 4 && k < p.size(); ++k) margins[k] = parseFloat(p[k]);
        return true;
    }
    return Container::setProperty(key, v);
}

// ---------------------------------------------------------------------------------- center
Vec2 CenterContainer::computeMinSize() {
    Vec2 m;
    for (auto& c : children_)
        if (c->visible()) {
            Vec2 cm = c->minimumSize();
            m.x = std::max(m.x, cm.x), m.y = std::max(m.y, cm.y);
        }
    return m;
}

void CenterContainer::layout() {
    for (auto& c : children_) {
        if (!c->visible()) continue;
        Vec2 m = c->minimumSize();
        c->setRect({std::floor(rect_.x + (rect_.w - m.x) * 0.5f), std::floor(rect_.y + (rect_.h - m.y) * 0.5f), m.x, m.y});
    }
}

// ---------------------------------------------------------------------------------- panels
Vec2 PanelContainer::computeMinSize() {
    const StyleBox& b = box("panel");
    Vec2 m;
    for (auto& c : children_)
        if (c->visible()) {
            Vec2 cm = c->minimumSize();
            m.x = std::max(m.x, cm.x), m.y = std::max(m.y, cm.y);
        }
    return {m.x + b.margin[0] + b.margin[2], m.y + b.margin[1] + b.margin[3]};
}

void PanelContainer::layout() {
    const StyleBox& b = box("panel");
    Rect inner = rect_.shrink(b.margin[0], b.margin[1], b.margin[2], b.margin[3]);
    for (auto& c : children_)
        if (c->visible()) fitChild(c.get(), inner);
}

void PanelContainer::draw(Canvas& c) { c.styleBox(rect_, box("panel")); }
void Panel::draw(Canvas& c) { c.styleBox(rect_, box("panel")); }

// ---------------------------------------------------------------------------------- split
Vec2 SplitContainer::computeMinSize() {
    float s = constant("separation");
    Vec2 m;
    int n = 0;
    for (auto& c : children_) {
        if (!c->visible() || n >= 2) continue;
        Vec2 cm = c->minimumSize();
        if (vertical_) m.y += cm.y, m.x = std::max(m.x, cm.x);
        else m.x += cm.x, m.y = std::max(m.y, cm.y);
        ++n;
    }
    if (n == 2) (vertical_ ? m.y : m.x) += s;
    return m;
}

Rect SplitContainer::grabber() const {
    float s = constant("separation");
    float total = vertical_ ? rect_.h : rect_.w;
    float pos = fixed > 0 ? (fixedEnd ? total - fixed - s : fixed) : std::floor((total - s) * ratio);
    return vertical_ ? Rect{rect_.x, rect_.y + pos, rect_.w, s} : Rect{rect_.x + pos, rect_.y, s, rect_.h};
}

void SplitContainer::layout() {
    std::vector<Control*> vis;
    for (auto& c : children_)
        if (c->visible()) vis.push_back(c.get());
    if (vis.empty()) return;
    if (vis.size() == 1) {
        vis[0]->setRect(rect_);
        return;
    }
    float s = constant("separation");
    float total = vertical_ ? rect_.h : rect_.w;
    Vec2 m0 = vis[0]->minimumSize(), m1 = vis[1]->minimumSize();
    float a = fixed > 0 ? (fixedEnd ? total - fixed - s : fixed) : std::floor((total - s) * ratio);
    float min0 = vertical_ ? m0.y : m0.x, min1 = vertical_ ? m1.y : m1.x;
    a = clampv(a, min0, std::max(min0, total - s - min1));
    float b = total - s - a;
    if (vertical_) {
        vis[0]->setRect({rect_.x, rect_.y, rect_.w, a});
        vis[1]->setRect({rect_.x, rect_.y + a + s, rect_.w, b});
    } else {
        vis[0]->setRect({rect_.x, rect_.y, a, rect_.h});
        vis[1]->setRect({rect_.x + a + s, rect_.y, b, rect_.h});
    }
}

void SplitContainer::draw(Canvas& c) {
    int n = 0;
    for (auto& ch : children_) n += ch->visible();
    if (n < 2) return;
    Rect g = grabber();
    bool hot = dragging_ || (hovered() && g.contains(context() ? context()->mousePos() : Vec2{}));
    c.rect(g, hot ? withAlpha(color("grabber_hover"), 0.8f) : color("grabber"));
}

Cursor SplitContainer::cursor(Vec2 p) const {
    if (grabber().contains(p) || dragging_) return vertical_ ? Cursor::ResizeV : Cursor::ResizeH;
    return Cursor::Arrow;
}

bool SplitContainer::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.type == T::MouseDown && e.button == MouseButton::Left && grabber().contains(e.pos)) {
        dragging_ = true;
        Rect g = grabber();
        dragOffset_ = vertical_ ? e.pos.y - g.y : e.pos.x - g.x;
        return true;
    }
    if (e.type == T::MouseMove && dragging_) {
        float s = constant("separation");
        float total = vertical_ ? rect_.h : rect_.w;
        float pos = (vertical_ ? e.pos.y - rect_.y : e.pos.x - rect_.x) - dragOffset_;
        pos = clampv(pos, 0.0f, std::max(0.0f, total - s));
        if (fixed > 0) fixed = std::max(1.0f, fixedEnd ? total - s - pos : pos);
        else ratio = total - s > 0 ? pos / (total - s) : 0.5f;
        dragged.emit();
        return true;
    }
    if (e.type == T::MouseUp && dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

bool SplitContainer::setProperty(std::string_view key, std::string_view v) {
    if (key == "ratio") ratio = clampv(parseFloat(v, 0.5f), 0.0f, 1.0f);
    else if (key == "fixed") fixed = parseFloat(v);
    else if (key == "fixed-end") fixed = parseFloat(v), fixedEnd = true;
    else return Container::setProperty(key, v);
    return true;
}

// ---------------------------------------------------------------------------------- scroll
Vec2 ScrollContainer::computeMinSize() {
    Vec2 m;
    for (auto& c : children_)
        if (c->visible()) {
            Vec2 cm = c->minimumSize();
            if (!horizontal) m.x = std::max(m.x, cm.x);
            if (!vertical) m.y = std::max(m.y, cm.y);
        }
    return m;
}

void ScrollContainer::layout() {
    float bw = constant("width") > 0 ? theme().constant("ScrollBar", "width") : 10;
    for (auto& c : children_) {
        if (!c->visible()) continue;
        Vec2 m = c->minimumSize();
        content_ = {horizontal ? std::max(m.x, rect_.w) : rect_.w, vertical ? std::max(m.y, rect_.h) : rect_.h};
        bool vbar = vertical && content_.y > rect_.h;
        if (vbar && !horizontal) content_.x = std::max(m.x, rect_.w - bw);
        float maxX = std::max(0.0f, content_.x - rect_.w), maxY = std::max(0.0f, content_.y - rect_.h);
        scroll_.x = clampv(scroll_.x, 0.0f, maxX);
        scroll_.y = clampv(scroll_.y, 0.0f, maxY);
        c->setRect({rect_.x - std::floor(scroll_.x), rect_.y - std::floor(scroll_.y), content_.x, content_.y});
        break;
    }
}

Rect ScrollContainer::bar(int axis, bool grab) const {
    float bw = theme().constant("ScrollBar", "width");
    if (bw <= 0) bw = 10;
    if (axis == 0) {
        Rect track{rect_.right() - bw, rect_.y, bw, rect_.h};
        if (!grab) return track;
        float frac = content_.y > 0 ? rect_.h / content_.y : 1;
        float h = std::max(20.0f, track.h * frac);
        float maxS = std::max(1.0f, content_.y - rect_.h);
        return {track.x + 2, track.y + (track.h - h) * (scroll_.y / maxS), bw - 4, h};
    }
    Rect track{rect_.x, rect_.bottom() - bw, rect_.w, bw};
    if (!grab) return track;
    float frac = content_.x > 0 ? rect_.w / content_.x : 1;
    float w = std::max(20.0f, track.w * frac);
    float maxS = std::max(1.0f, content_.x - rect_.w);
    return {track.x + (track.w - w) * (scroll_.x / maxS), track.y + 2, w, bw - 4};
}

void ScrollContainer::drawOver(Canvas& c) {
    const Theme& th = theme();
    if (vertical && content_.y > rect_.h + 0.5f) {
        c.styleBox(bar(0, false), th.box("ScrollBar", "scroll"));
        c.styleBox(bar(0, true), th.box("ScrollBar", dragBar_ == 0 ? "grabber_hover" : "grabber"));
    }
    if (horizontal && content_.x > rect_.w + 0.5f) {
        c.styleBox(bar(1, false), th.box("ScrollBar", "scroll"));
        c.styleBox(bar(1, true), th.box("ScrollBar", dragBar_ == 1 ? "grabber_hover" : "grabber"));
    }
}

bool ScrollContainer::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.type == T::Wheel) {
        if (vertical && content_.y > rect_.h && !(e.mods & Mod::Shift)) scroll_.y -= e.delta.y * 48;
        else if (horizontal) scroll_.x -= (e.delta.y + e.delta.x) * 48;
        else return false;
        return true;
    }
    if (e.type == T::MouseDown && e.button == MouseButton::Left) {
        for (int axis = 0; axis < 2; ++axis) {
            bool on = axis == 0 ? vertical && content_.y > rect_.h : horizontal && content_.x > rect_.w;
            if (on && bar(axis, false).contains(e.pos)) {
                dragBar_ = axis;
                dragStart_ = axis == 0 ? e.pos.y : e.pos.x;
                dragScroll_ = axis == 0 ? scroll_.y : scroll_.x;
                if (!bar(axis, true).contains(e.pos)) {
                    // Page towards the click.
                    float page = axis == 0 ? rect_.h : rect_.w;
                    Rect g = bar(axis, true);
                    bool after = axis == 0 ? e.pos.y > g.y : e.pos.x > g.x;
                    (axis == 0 ? scroll_.y : scroll_.x) += after ? page : -page;
                    dragBar_ = -1;
                }
                return true;
            }
        }
    }
    if (e.type == T::MouseMove && dragBar_ >= 0) {
        Rect track = bar(dragBar_, false), g = bar(dragBar_, true);
        float trackLen = dragBar_ == 0 ? track.h - g.h : track.w - g.w;
        float content = dragBar_ == 0 ? content_.y - rect_.h : content_.x - rect_.w;
        float d = (dragBar_ == 0 ? e.pos.y : e.pos.x) - dragStart_;
        float v = dragScroll_ + (trackLen > 0 ? d / trackLen * content : 0);
        (dragBar_ == 0 ? scroll_.y : scroll_.x) = v;
        return true;
    }
    if (e.type == T::MouseUp && dragBar_ >= 0) {
        dragBar_ = -1;
        return true;
    }
    return false;
}

void ScrollContainer::ensureVisible(const Rect& r) {
    if (r.y < rect_.y) scroll_.y -= rect_.y - r.y;
    else if (r.bottom() > rect_.bottom()) scroll_.y += r.bottom() - rect_.bottom();
}

bool ScrollContainer::setProperty(std::string_view key, std::string_view v) {
    if (key == "horizontal") horizontal = parseBool(v);
    else if (key == "vertical") vertical = parseBool(v);
    else return Container::setProperty(key, v);
    return true;
}

// ---------------------------------------------------------------------------------- tabs
float TabContainer::tabBarHeight() const {
    const StyleBox& t = box("tab_selected");
    return std::ceil(font().lineHeight(fontSize()) + t.margin[1] + t.margin[3]);
}

std::vector<Rect> TabContainer::tabRects() const {
    std::vector<Rect> out;
    float x = rect_.x + 2, h = tabBarHeight();
    const StyleBox& t = box("tab_selected");
    for (auto& c : children_) {
        float w = font().measure(c->title.empty() ? c->id : c->title, fontSize()) + t.margin[0] + t.margin[2];
        if (!c->icon.empty()) w += fontSize() + 4;
        out.push_back({x, rect_.y, std::ceil(w), h});
        x += std::ceil(w) + 1;
    }
    return out;
}

Vec2 TabContainer::computeMinSize() {
    Vec2 m;
    for (auto& c : children_) {
        Vec2 cm = c->minimumSize();
        m.x = std::max(m.x, cm.x), m.y = std::max(m.y, cm.y);
    }
    auto tabs = tabRects();
    float tw = tabs.empty() ? 0 : tabs.back().right() - rect_.x;
    return {std::max(m.x, std::min(tw, 200.0f)), m.y + tabBarHeight()};
}

void TabContainer::setCurrent(int i) {
    if (children_.empty()) return;
    i = clampv(i, 0, int(children_.size()) - 1);
    bool changed = i != current_;
    current_ = i;
    for (size_t k = 0; k < children_.size(); ++k) children_[k]->setVisible(int(k) == current_);
    if (changed) tabChanged.emit(current_);
}

void TabContainer::layout() {
    if (!children_.empty()) {
        current_ = clampv(current_, 0, int(children_.size()) - 1);
        for (size_t k = 0; k < children_.size(); ++k) children_[k]->setVisible(int(k) == current_);
    }
    float h = tabBarHeight();
    Rect content{rect_.x, rect_.y + h, rect_.w, std::max(0.0f, rect_.h - h)};
    for (auto& c : children_)
        if (c->visible()) c->setRect(content);
}

void TabContainer::draw(Canvas& c) {
    float h = tabBarHeight();
    c.styleBox({rect_.x, rect_.y, rect_.w, h}, box("tabbar_background"));
    c.styleBox({rect_.x, rect_.y + h, rect_.w, rect_.h - h}, box("panel"));
    auto tabs = tabRects();
    c.pushClip({rect_.x, rect_.y, rect_.w, h});
    for (size_t i = 0; i < tabs.size(); ++i) {
        bool sel = int(i) == current_, hov = int(i) == hoverTab_;
        const StyleBox& sb = box(sel ? "tab_selected" : (hov ? "tab_hover" : "tab_unselected"));
        Rect r = tabs[i];
        StyleBox flatBottom = sb;
        c.styleBox(r, flatBottom);
        if (sel) c.rect({r.x, r.y, r.w, 2}, color("tab_accent"));
        Rect inner = r.shrink(sb.margin[0], 0, sb.margin[2], 0);
        Color fc = sel ? color("font_color") : color("font_unselected_color");
        const Control* page = children_[i].get();
        if (!page->icon.empty()) {
            c.icon({inner.x, inner.y, fontSize(), inner.h}, iconCodepoint(page->icon), iconFont(), fontSize() * 0.9f, fc);
            inner = inner.shrink(fontSize() + 4, 0, 0, 0);
        }
        c.textIn(inner, page->title.empty() ? page->id : page->title, font(), fontSize(), fc);
    }
    c.popClip();
}

bool TabContainer::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.type == T::MouseMove) {
        int h = -1;
        auto tabs = tabRects();
        for (size_t i = 0; i < tabs.size(); ++i)
            if (tabs[i].contains(e.pos)) h = int(i);
        hoverTab_ = h;
        return false;
    }
    if (e.type == T::MouseDown && e.button == MouseButton::Left) {
        auto tabs = tabRects();
        for (size_t i = 0; i < tabs.size(); ++i)
            if (tabs[i].contains(e.pos)) {
                setCurrent(int(i));
                return true;
            }
    }
    return false;
}

bool TabContainer::dragData(Vec2 p, DragPayload& out) {
    if (!dragTabs) return false;
    auto tabs = tabRects();
    for (size_t i = 0; i < tabs.size(); ++i)
        if (tabs[i].contains(p)) {
            out.type = "tab";
            out.data = static_cast<Control*>(children_[i].get());
            out.label = children_[i]->title;
            return true;
        }
    return false;
}

bool TabContainer::canDrop(Vec2 p, const DragPayload& d) {
    if (!dragTabs || d.type != "tab") return false;
    Control* page = std::any_cast<Control*>(d.data);
    if (!isAlive(page) || page->isAncestorOf(this)) return false;
    return p.y < rect_.y + tabBarHeight() || page->parent() != this;
}

void TabContainer::drop(Vec2 p, const DragPayload& d) {
    Control* page = std::any_cast<Control*>(d.data);
    if (!isAlive(page)) return;
    Control* from = page->parent();
    if (!from) return;
    // Insert before the tab under the pointer.
    auto tabs = tabRects();
    int at = int(children_.size());
    for (size_t i = 0; i < tabs.size(); ++i)
        if (p.x < tabs[i].x + tabs[i].w * 0.5f) {
            at = int(i);
            break;
        }
    if (from == this) {
        int old = indexOf(page);
        moveChild(page, at > old ? at - 1 : at);
        setCurrent(indexOf(page));
        return;
    }
    auto owned = from->removeChild(page);
    if (auto* tc = dynamic_cast<TabContainer*>(from)) tc->setCurrent(tc->current());
    owned->setVisible(true);
    addChild(std::move(owned), at);
    setCurrent(indexOf(page));
}

bool TabContainer::setProperty(std::string_view key, std::string_view v) {
    if (key == "current") current_ = parseInt(v);
    else if (key == "drag-tabs") dragTabs = parseBool(v);
    else return Container::setProperty(key, v);
    return true;
}

bool TabContainer::connectSignal(std::string_view s, std::function<void()> fn) {
    if (s == "tab_changed") {
        tabChanged.connect([fn](int) { fn(); });
        return true;
    }
    return Container::connectSignal(s, std::move(fn));
}

// ---------------------------------------------------------------------------------- flow
Vec2 FlowContainer::computeMinSize() {
    Vec2 m;
    for (auto& c : children_)
        if (c->visible()) {
            Vec2 cm = c->minimumSize();
            m.x = std::max(m.x, cm.x), m.y = std::max(m.y, cm.y);
        }
    // Height depends on the width it gets: estimate from the last layout.
    if (rect_.w > 0) {
        float x = 0, y = 0, rowH = 0;
        for (auto& c : children_) {
            if (!c->visible()) continue;
            Vec2 cm = c->minimumSize();
            if (x > 0 && x + cm.x > rect_.w) x = 0, y += rowH + separation, rowH = 0;
            x += cm.x + separation;
            rowH = std::max(rowH, cm.y);
        }
        m.y = y + rowH;
    }
    return m;
}

void FlowContainer::layout() {
    float x = rect_.x, y = rect_.y, rowH = 0;
    for (auto& c : children_) {
        if (!c->visible()) continue;
        Vec2 cm = c->minimumSize();
        if (x > rect_.x && x + cm.x > rect_.right()) x = rect_.x, y += rowH + separation, rowH = 0;
        c->setRect({x, y, cm.x, cm.y});
        x += cm.x + separation;
        rowH = std::max(rowH, cm.y);
    }
}

}  // namespace ose::ui
