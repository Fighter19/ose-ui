#include "ui/lists.h"

#include <algorithm>
#include <cmath>

#include "ui/context.h"

namespace ose::ui {

// ---------------------------------------------------------------------------------- tree items
TreeItem* TreeItem::add(std::string t, std::string ic) {
    auto item = std::make_unique<TreeItem>();
    item->text = std::move(t);
    item->icon = std::move(ic);
    item->parent_ = this;
    children_.push_back(std::move(item));
    return children_.back().get();
}

int TreeItem::depth() const {
    int d = 0;
    for (const TreeItem* p = parent_; p; p = p->parent_) ++d;
    return d;
}

// ---------------------------------------------------------------------------------- tree
Tree::Tree() {
    focusMode = FocusMode::Click;
    root_.expanded = true;
}

void Tree::clear() {
    root_.clear();
    selection_.clear();
    hover_ = anchor_ = dropItem_ = nullptr;
    rowsFrame_ = ~0ull;
}

float Tree::itemHeight() const {
    float h = constant("item_height");
    return h > 0 ? h : std::ceil(font().lineHeight(fontSize()) + 6);
}

void Tree::flatten(TreeItem* item, std::vector<TreeItem*>& out) {
    if (item != &root_ || !hideRoot) out.push_back(item);
    if (item->expanded || item == &root_)
        for (auto& c : item->children_) flatten(c.get(), out);
}

const std::vector<TreeItem*>& Tree::rows() {
    Context* ctx = context();
    uint64_t f = ctx ? ctx->frame() : 0;
    if (rowsFrame_ != f) {
        rows_.clear();
        flatten(&root_, rows_);
        rowsFrame_ = f;
    }
    return rows_;
}

Vec2 Tree::computeMinSize() {
    rowsFrame_ = ~0ull;
    return {60, float(rows().size()) * itemHeight() + 4};
}

Rect Tree::itemRect(const TreeItem* item) {
    const auto& r = rows();
    float h = itemHeight();
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i] == item) return {rect_.x, rect_.y + 2 + float(i) * h, rect_.w, h};
    return {};
}

TreeItem* Tree::itemAt(Vec2 p) {
    if (!rect_.contains(p)) return nullptr;
    float h = itemHeight();
    int i = int(std::floor((p.y - rect_.y - 2) / h));
    const auto& r = rows();
    return i >= 0 && size_t(i) < r.size() ? r[size_t(i)] : nullptr;
}

TreeItem* Tree::findByKey(uint64_t key) {
    std::vector<TreeItem*> stack{&root_};
    while (!stack.empty()) {
        TreeItem* t = stack.back();
        stack.pop_back();
        if (t->key == key && t != &root_) return t;
        for (auto& c : t->children_) stack.push_back(c.get());
    }
    return nullptr;
}

bool Tree::isSelected(const TreeItem* item) const { return std::find(selection_.begin(), selection_.end(), item) != selection_.end(); }

void Tree::select(TreeItem* item, bool add, bool notify) {
    if (!add) selection_.clear();
    if (item && item->selectable && !isSelected(item)) selection_.push_back(item);
    anchor_ = item;
    if (notify) selectionChanged.emit();
}

void Tree::clearSelection(bool notify) {
    selection_.clear();
    if (notify) selectionChanged.emit();
}

void Tree::draw(Canvas& c) {
    c.styleBox(rect_, box("panel"));
    const auto& r = rows();
    float h = itemHeight(), indent = constant("indent") > 0 ? constant("indent") : 16;
    Rect clip = c.clip();
    int first = std::max(0, int((clip.y - rect_.y - 2) / h));
    int last = std::min(int(r.size()), int((clip.bottom() - rect_.y) / h) + 1);
    float px = fontSize();
    for (int i = first; i < last; ++i) {
        TreeItem* it = r[size_t(i)];
        Rect row{rect_.x, rect_.y + 2 + float(i) * h, rect_.w, h};
        // A selected row has its own text colour (white on the selection bar); what is dim
        // elsewhere is the same colour a little fainter, and icons give up their own colour
        // so they stay readable on the bar.
        bool sel = isSelected(it);
        Color fc = sel ? color("font_selected_color") : color("font_color");
        Color dimc = sel ? withAlpha(fc, 0.8f) : color("font_dim_color");
        if (sel) c.styleBox(row.shrink(2, 0, 2, 0), box("selected"));
        else if (it == hover_) c.styleBox(row.shrink(2, 0, 2, 0), box("hover"));
        if (it == dropItem_) c.border(row.shrink(2, 0, 2, 0), color("accent"), 1, 2);
        int depth = it->depth() - (hideRoot ? 1 : 0);
        float x = row.x + 4 + float(depth) * indent;
        if (!it->children_.empty())
            c.icon({x, row.y, 12, h}, iconCodepoint(it->expanded ? "caret-down" : "caret-right"), iconFont(), px * 0.85f, dimc);
        x += 14;
        if (!it->icon.empty()) {
            c.icon({x, row.y, 16, h}, iconCodepoint(it->icon), iconFont(), px * 0.9f, it->iconColor.w > 0 && !sel ? it->iconColor : dimc);
            x += 20;
        }
        float right = row.right() - 6;
        if (!it->toggleIcon.empty()) {
            Rect tr{right - 18, row.y, 18, h};
            c.icon(tr, iconCodepoint(it->toggleOn ? it->toggleIcon : (it->toggleIconOff.empty() ? it->toggleIcon : it->toggleIconOff)),
                   iconFont(), px * 0.85f, it->toggleOn ? dimc : sel ? withAlpha(fc, 0.5f) : color("font_disabled_color"));
            right -= 22;
        }
        if (!it->secondary.empty()) {
            float sw = std::min(font().measure(it->secondary, px * 0.9f) + 6, (right - x) * 0.45f);
            c.textIn({right - sw, row.y, sw, h}, it->secondary, font(), px * 0.9f, dimc, Align::End);
            right -= sw + 4;
        }
        c.textIn({x, row.y, std::max(0.0f, right - x), h}, it->text, font(), px, fc);
    }
}

bool Tree::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.type == T::MouseMove) {
        hover_ = itemAt(e.pos);
        return true;
    }
    if (e.type == T::MouseDown) {
        TreeItem* it = itemAt(e.pos);
        if (e.button == MouseButton::Right) {
            if (it && !isSelected(it)) select(it);
            contextMenu.emit(it, e.pos);
            return true;
        }
        if (e.button != MouseButton::Left) return true;
        if (!it) {
            clearSelection();
            return true;
        }
        Rect row = itemRect(it);
        float indent = constant("indent") > 0 ? constant("indent") : 16;
        float arrowX = row.x + 4 + float(it->depth() - (hideRoot ? 1 : 0)) * indent;
        if (!it->children_.empty() && e.pos.x >= arrowX && e.pos.x < arrowX + 14) {
            it->expanded = !it->expanded;
            return true;
        }
        if (!it->toggleIcon.empty() && e.pos.x >= row.right() - 24) {
            it->toggleOn = !it->toggleOn;
            toggleClicked.emit(it);
            return true;
        }
        if (e.clicks >= 2) {
            activated.emit(it);
            return true;
        }
        if ((e.mods & Mod::Shift) && multiSelect && anchor_) {
            const auto& r = rows();
            auto a = std::find(r.begin(), r.end(), anchor_), b = std::find(r.begin(), r.end(), it);
            if (a != r.end() && b != r.end()) {
                if (a > b) std::swap(a, b);
                selection_.clear();
                for (auto x = a; x <= b; ++x)
                    if ((*x)->selectable) selection_.push_back(*x);
                selectionChanged.emit();
                return true;
            }
        }
        if ((e.mods & Mod::Ctrl) && multiSelect) {
            if (isSelected(it)) {
                std::erase(selection_, it);
                selectionChanged.emit();
            } else {
                select(it, true);
            }
            return true;
        }
        select(it);
        return true;
    }
    if (e.type == T::KeyDown && hasFocus()) {
        const auto& r = rows();
        if (r.empty()) return false;
        TreeItem* cur = selection_.empty() ? nullptr : selection_.back();
        auto pos = std::find(r.begin(), r.end(), cur);
        if (e.key == "down" || e.key == "up") {
            long i = pos == r.end() ? 0 : (pos - r.begin()) + (e.key == "down" ? 1 : -1);
            i = std::clamp(i, 0L, long(r.size()) - 1);
            select(r[size_t(i)]);
            return true;
        }
        if (cur && e.key == "right") {
            cur->expanded = true;
            return true;
        }
        if (cur && e.key == "left") {
            cur->expanded = false;
            return true;
        }
        if (cur && e.key == "enter") {
            activated.emit(cur);
            return true;
        }
    }
    return false;
}

bool Tree::dragData(Vec2 p, DragPayload& out) {
    TreeItem* it = itemAt(p);
    if (!it || !dragProvider) return false;
    if (!isSelected(it)) select(it);
    return dragProvider(it, out);
}

bool Tree::canDrop(Vec2 p, const DragPayload& d) {
    dropItem_ = itemAt(p);
    bool ok = dropFilter && dropFilter(dropItem_, d);
    if (!ok) dropItem_ = nullptr;
    return ok;
}

void Tree::drop(Vec2 p, const DragPayload& d) {
    TreeItem* target = itemAt(p);
    dropItem_ = nullptr;
    if (onDrop) onDrop(target, d);
}

// ---------------------------------------------------------------------------------- item list
Vec2 ItemList::cell() const {
    float lh = std::ceil(font().lineHeight(fontSize() * 0.9f));
    if (tileSize <= 0) return {rect_.w, lh + 6};
    return {tileSize + 8, tileSize + lh + 12};
}

int ItemList::columns() const {
    if (tileSize <= 0) return 1;
    return std::max(1, int((rect_.w - 4) / cell().x));
}

Vec2 ItemList::computeMinSize() {
    Vec2 c = cell();
    int cols = rect_.w > 0 ? columns() : 1;
    int rows = (int(items.size()) + cols - 1) / cols;
    return {tileSize > 0 ? c.x + 4 : 60, float(rows) * c.y + 4};
}

Rect ItemList::itemRect(int i) const {
    Vec2 c = cell();
    int cols = columns();
    return {rect_.x + 2 + float(i % cols) * c.x, rect_.y + 2 + float(i / cols) * c.y, c.x, c.y};
}

int ItemList::itemAt(Vec2 p) const {
    if (!rect_.contains(p)) return -1;
    Vec2 c = cell();
    int cols = columns();
    int col = int((p.x - rect_.x - 2) / c.x), row = int((p.y - rect_.y - 2) / c.y);
    if (col < 0 || col >= cols || row < 0) return -1;
    int i = row * cols + col;
    return i < int(items.size()) ? i : -1;
}

void ItemList::draw(Canvas& c) {
    c.styleBox(rect_, box("panel"));
    Rect clip = c.clip();
    Vec2 cs = cell();
    int cols = columns();
    int firstRow = std::max(0, int((clip.y - rect_.y) / cs.y));
    int lastRow = int((clip.bottom() - rect_.y) / cs.y) + 1;
    float px = fontSize() * 0.9f;
    for (int row = firstRow; row <= lastRow; ++row)
        for (int col = 0; col < cols; ++col) {
            int i = row * cols + col;
            if (i >= int(items.size())) break;
            const Item& it = items[size_t(i)];
            Rect r = itemRect(i);
            bool sel = i == selected;
            Color fc = sel ? color("font_selected_color") : color("font_color");
            Color dimc = sel ? withAlpha(fc, 0.8f) : color("font_dim_color");
            if (sel) c.styleBox(r.shrink(1), box("selected"));
            else if (hovered() && r.contains(context() ? context()->mousePos() : Vec2{})) c.styleBox(r.shrink(1), box("hover"));
            if (tileSize <= 0) {
                float x = r.x + 4;
                if (!it.icon.empty()) {
                    c.icon({x, r.y, 16, r.h}, iconCodepoint(it.icon), iconFont(), px, it.iconColor.w > 0 && !sel ? it.iconColor : dimc);
                    x += 20;
                }
                c.textIn({x, r.y, r.right() - x - 4, r.h}, it.text, font(), px, fc);
                continue;
            }
            Rect thumb{r.x + 4, r.y + 4, tileSize, tileSize};
            if (wantThumbnail) wantThumbnail(i);  // every frame: handles can change (textures reloaded)
            if (it.texture) c.image(thumb, it.texture);
            else {
                c.roundRect(thumb, withAlpha(color("background"), 1), 3);
                c.icon(thumb, iconCodepoint(it.icon.empty() ? "file" : it.icon), iconFont(), tileSize * 0.4f,
                       it.iconColor.w > 0 ? it.iconColor : color("font_dim_color"));
            }
            c.textIn({r.x + 2, thumb.bottom() + 2, r.w - 4, r.bottom() - thumb.bottom() - 4}, it.text, font(), px, fc, Align::Center);
        }
}

bool ItemList::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.type == T::MouseDown) {
        int i = itemAt(e.pos);
        if (i != selected) {
            selected = i;
            selectedChanged.emit(i);
        }
        if (e.button == MouseButton::Right) contextMenu.emit(i, e.pos);
        else if (e.clicks >= 2 && i >= 0) activated.emit(i);
        return true;
    }
    if (e.type == T::MouseMove) {
        int i = itemAt(e.pos);
        tooltip = i >= 0 ? (items[size_t(i)].tooltip.empty() ? items[size_t(i)].text : items[size_t(i)].tooltip) : std::string();
        return false;
    }
    if (e.type == T::KeyDown && hasFocus() && !items.empty()) {
        int cols = columns(), n = int(items.size());
        int s = selected < 0 ? 0 : selected;
        if (e.key == "right") s = std::min(n - 1, s + 1);
        else if (e.key == "left") s = std::max(0, s - 1);
        else if (e.key == "down") s = std::min(n - 1, s + cols);
        else if (e.key == "up") s = std::max(0, s - cols);
        else if (e.key == "enter" && selected >= 0) {
            activated.emit(selected);
            return true;
        } else return false;
        selected = s;
        selectedChanged.emit(s);
        return true;
    }
    return false;
}

bool ItemList::dragData(Vec2 p, DragPayload& out) {
    int i = itemAt(p);
    if (i < 0 || !dragProvider) return false;
    return dragProvider(i, out);
}

// ---------------------------------------------------------------------------------- viewport
void ViewportControl::draw(Canvas& c) {
    if (!transparent) c.rect(rect_, color("background"));
}

void ViewportControl::drawOver(Canvas& c) {
    if (onOverlay) onOverlay(c, rect_);
    if (hasFocus()) c.border(rect_, color("focus_border"), 1);
}

void collectViewports(Control* root, std::vector<ViewportControl*>& out) {
    if (!root || !root->visible()) return;
    if (auto* v = dynamic_cast<ViewportControl*>(root)) out.push_back(v);
    for (size_t i = 0; i < root->childCount(); ++i) collectViewports(root->child(i), out);
}

// ---------------------------------------------------------------------------------- window
float Window::titleHeight() const {
    const StyleBox& t = box("titlebar");
    return std::ceil(font().lineHeight(fontSize()) + t.margin[1] + t.margin[3]);
}

Vec2 Window::computeMinSize() {
    Vec2 m{200, 0};
    for (auto& c : children_)
        if (c->visible()) {
            Vec2 cm = c->minimumSize();
            m.x = std::max(m.x, cm.x + 16);
            m.y = std::max(m.y, cm.y);
        }
    return {m.x, m.y + titleHeight() + 16};
}

void Window::layout() {
    Rect content = Rect{rect_.x, rect_.y + titleHeight(), rect_.w, rect_.h - titleHeight()}.shrink(8);
    for (auto& c : children_)
        if (c->visible()) c->setRect(content);
}

void Window::draw(Canvas& c) {
    c.styleBox(rect_, box("panel"));
    Rect tb{rect_.x, rect_.y, rect_.w, titleHeight()};
    c.styleBox(tb, box("titlebar"));
    const StyleBox& t = box("titlebar");
    c.textIn(tb.shrink(t.margin[0], 0, 28, 0), title, theme().boldFont ? *theme().boldFont : font(), fontSize(), color("font_color"));
    c.icon({tb.right() - 26, tb.y, 22, tb.h}, iconCodepoint("times"), iconFont(), fontSize(), color("font_dim_color"));
}

bool Window::event(const InputEvent& e) {
    using T = InputEvent::Type;
    Rect close{rect_.x + rect_.w - 26, rect_.y, 22, titleHeight()};
    if (e.type == T::MouseUp && e.button == MouseButton::Left && close.contains(e.pos)) {
        if (onClose) onClose();
        else if (Context* ctx = context()) ctx->closePopup(this);
        return true;
    }
    if (e.type == T::KeyDown && e.key == "escape") {
        if (onClose) onClose();
        else if (Context* ctx = context()) ctx->closePopup(this);
        return true;
    }
    return e.isMouse();
}

}  // namespace ose::ui
