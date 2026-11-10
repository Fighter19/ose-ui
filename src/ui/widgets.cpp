#include "ui/widgets.h"

#include <cmath>
#include <cstdio>

#include "core/util.h"
#include "ui/context.h"

namespace ose::ui {

std::string formatNumberShort(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", std::max(0, decimals), v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

// ---------------------------------------------------------------------------------- label
std::vector<std::string> Label::wrapLines(float width) const {
    std::vector<std::string> lines;
    const Font& f = bold && theme().boldFont ? *theme().boldFont : font();
    float px = fontSize() * sizeScale;
    for (auto para : split(text, '\n', false)) {
        std::string line;
        for (auto word : splitWhitespace(para)) {
            std::string cand = line.empty() ? std::string(word) : line + " " + std::string(word);
            if (!line.empty() && f.measure(cand, px) > width) {
                lines.push_back(line);
                line = std::string(word);
            } else {
                line = cand;
            }
        }
        lines.push_back(line);
    }
    return lines;
}

Vec2 Label::computeMinSize() {
    const Font& f = bold && theme().boldFont ? *theme().boldFont : font();
    float px = fontSize() * sizeScale;
    float lh = std::ceil(f.lineHeight(px));
    if (wrap) {
        float w = rect_.w > 0 ? rect_.w : 200;
        return {0, lh * float(std::max<size_t>(1, wrapLines(w).size()))};
    }
    float w = 0;
    int lines = 0;
    for (auto l : split(text, '\n', false)) {
        w = std::max(w, f.measure(l, px));
        ++lines;
    }
    return {clipText ? 0.0f : std::ceil(w), lh * float(std::max(1, lines))};
}

void Label::draw(Canvas& c) {
    const Font& f = bold && theme().boldFont ? *theme().boldFont : font();
    float px = fontSize() * sizeScale;
    Color col = colorOverride.w > 0 ? colorOverride : (isDisabled() ? color("font_disabled_color") : (dim ? color("font_dim_color") : color("font_color")));
    float lh = std::ceil(f.lineHeight(px));
    std::vector<std::string> lines;
    if (wrap) lines = wrapLines(rect_.w);
    else
        for (auto l : split(text, '\n', false)) lines.emplace_back(l);
    if (lines.size() == 1) {
        c.textIn(rect_, lines[0], f, px, col, align, ellipsis);
        return;
    }
    float y = rect_.y + std::floor((rect_.h - lh * float(lines.size())) * 0.5f);
    for (const auto& l : lines) {
        c.textIn({rect_.x, y, rect_.w, lh}, l, f, px, col, align, ellipsis);
        y += lh;
    }
}

bool Label::setProperty(std::string_view key, std::string_view v) {
    if (key == "text") text = std::string(v);
    else if (key == "align") align = v == "center" ? Align::Center : (v == "end" || v == "right" ? Align::End : Align::Begin);
    else if (key == "bold") bold = parseBool(v);
    else if (key == "dim") dim = parseBool(v);
    else if (key == "wrap") wrap = parseBool(v);
    else if (key == "ellipsis") ellipsis = parseBool(v);
    else if (key == "clip-text") clipText = parseBool(v);
    else if (key == "color") colorOverride = parseColor(v);
    else if (key == "scale") sizeScale = parseFloat(v, 1);
    else return Control::setProperty(key, v);
    return true;
}

// ---------------------------------------------------------------------------------- button
const StyleBox& Button::currentBox() const {
    if (isDisabled()) return box("disabled");
    if (down_ || pressed) return box("pressed");
    if (hovered()) return box("hover");
    return box("normal");
}

Color Button::currentFontColor() const {
    if (isDisabled()) return color("font_disabled_color");
    if (down_ || pressed) return color("font_pressed_color");
    if (hovered()) return color("font_hover_color");
    return color("font_color");
}

Vec2 Button::computeMinSize() {
    const StyleBox& b = box("normal");
    float w = 0, px = fontSize();
    if (!text.empty()) w += font().measure(text, px);
    if (!icon.empty()) w += px * 1.25f + (text.empty() ? 0 : 6);
    if (!shortcutText.empty()) w += font().measure(shortcutText, px) + 24;
    float h = std::ceil(font().lineHeight(px));
    return {std::ceil(w + b.margin[0] + b.margin[2]), std::ceil(h + b.margin[1] + b.margin[3])};
}

void Button::draw(Canvas& c) {
    const StyleBox& b = currentBox();
    c.styleBox(rect_, b);
    if (hasFocus() && focusMode == FocusMode::All) c.styleBox(rect_, box("focus"));
    Rect inner = rect_.shrink(b.margin[0], b.margin[1], b.margin[2], b.margin[3]);
    Color fc = currentFontColor();
    float px = fontSize();
    float textW = text.empty() ? 0 : font().measure(text, px);
    float iconW = icon.empty() ? 0 : px * 1.25f;
    float total = textW + iconW + (textW > 0 && iconW > 0 ? 6 : 0);
    float x = align == Align::Center ? inner.x + std::floor((inner.w - total) * 0.5f) : inner.x;
    if (!icon.empty()) {
        c.icon({x, inner.y, iconW, inner.h}, iconCodepoint(icon), iconFont(), px, fc);
        x += iconW + 6;
    }
    if (!text.empty()) c.textIn({x, inner.y, inner.right() - x, inner.h}, text, font(), px, fc, Align::Begin);
    if (!shortcutText.empty()) c.textIn(inner, shortcutText, font(), px, color("font_dim_color"), Align::End, false);
}

void Button::setPressed(bool p) {
    if (pressed == p) return;
    pressed = p;
    toggled.emit(p);
}

void Button::activate() {
    if (toggleMode) setPressed(!pressed);
    if (!action.empty())
        if (Context* ctx = context()) ctx->actions().run(action);
    clicked.emit();
}

bool Button::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (e.type == T::MouseDown && e.button == MouseButton::Left) {
        down_ = true;
        return true;
    }
    if (e.type == T::MouseUp && e.button == MouseButton::Left) {
        bool inside = rect_.contains(e.pos);
        bool was = down_;
        down_ = false;
        if (was && inside) activate();
        return true;
    }
    if (e.type == T::KeyDown && (e.key == "space" || e.key == "enter") && hasFocus()) {
        activate();
        return true;
    }
    return false;
}

bool Button::setProperty(std::string_view key, std::string_view v) {
    if (key == "text") text = std::string(v);
    else if (key == "toggle") toggleMode = parseBool(v);
    else if (key == "pressed" || key == "checked") pressed = parseBool(v);
    else if (key == "flat") {
        flat = parseBool(v);
        if (flat && themeType.empty()) themeType = "ToolButton";
    } else if (key == "align") align = v == "center" ? Align::Center : Align::Begin;
    else if (key == "shortcut") shortcutText = std::string(v);
    else if (key == "action") action = std::string(v);
    else return Control::setProperty(key, v);
    return true;
}

bool Button::connectSignal(std::string_view s, std::function<void()> fn) {
    if (s == "pressed" || s == "clicked") {
        clicked.connect(std::move(fn));
        return true;
    }
    if (s == "toggled") {
        toggled.connect([fn](bool) { fn(); });
        return true;
    }
    return Control::connectSignal(s, std::move(fn));
}

// ---------------------------------------------------------------------------------- checkbox
Vec2 CheckBox::computeMinSize() {
    Vec2 m = Button::computeMinSize();
    m.x += fontSize() + 6;
    return m;
}

void CheckBox::draw(Canvas& c) {
    const StyleBox& b = hovered() ? box("hover") : box("normal");
    c.styleBox(rect_, b);
    Rect inner = rect_.shrink(b.margin[0], b.margin[1], b.margin[2], b.margin[3]);
    float s = std::floor(fontSize());
    Rect boxR{inner.x, inner.y + std::floor((inner.h - s) * 0.5f), s, s};
    c.roundRect(boxR, theme().box("LineEdit", "normal").bg, 2);
    c.border(boxR, pressed ? color("accent") : theme().box("LineEdit", "normal").border, 1, 2);
    if (pressed) c.icon(boxR, iconCodepoint("check"), iconFont(), s * 0.8f, color("accent"));
    Color fc = isDisabled() ? color("font_disabled_color") : color("font_color");
    if (!text.empty()) c.textIn(inner.shrink(s + 6, 0, 0, 0), text, font(), fontSize(), fc);
}

// ---------------------------------------------------------------------------------- simple
void Separator::draw(Canvas& c) {
    Color col = color("color");
    if (vertical_) c.rect({std::floor(rect_.center().x), rect_.y + 2, 1, rect_.h - 4}, col);
    else c.rect({rect_.x + 2, std::floor(rect_.center().y), rect_.w - 4, 1}, col);
}

bool Separator::setProperty(std::string_view key, std::string_view v) {
    if (key == "vertical") vertical_ = parseBool(v);
    else return Control::setProperty(key, v);
    return true;
}

bool ColorRect::setProperty(std::string_view key, std::string_view v) {
    if (key == "color") fill = parseColor(v);
    else if (key == "radius") radius = parseFloat(v);
    else return Control::setProperty(key, v);
    return true;
}

void TextureRect::draw(Canvas& c) {
    if (!texture) return;
    Rect r = rect_;
    if (keepAspect && textureSize.x > 0 && textureSize.y > 0) {
        float s = std::min(r.w / textureSize.x, r.h / textureSize.y);
        float w = textureSize.x * s, h = textureSize.y * s;
        r = {r.x + std::floor((r.w - w) * 0.5f), r.y + std::floor((r.h - h) * 0.5f), w, h};
    }
    c.image(r, texture, uv);
}

void ProgressBar::draw(Canvas& c) {
    c.styleBox(rect_, box("background"));
    float v = clampv(value, 0.0f, 1.0f);
    if (v > 0) c.styleBox({rect_.x, rect_.y, std::max(4.0f, rect_.w * v), rect_.h}, box("fill"));
    if (!text.empty()) c.textIn(rect_, text, font(), fontSize() * 0.9f, color("font_color"), Align::Center);
}

bool ProgressBar::setProperty(std::string_view key, std::string_view v) {
    if (key == "value") value = parseFloat(v);
    else if (key == "text") text = std::string(v);
    else return Control::setProperty(key, v);
    return true;
}

// ---------------------------------------------------------------------------------- slider
void Slider::setValue(float v, bool notify) {
    v = clampv(v, minValue, maxValue);
    if (step > 0) v = minValue + std::round((v - minValue) / step) * step;
    if (v == value) return;
    value = v;
    if (notify) valueChanged.emit(v);
}

void Slider::draw(Canvas& c) {
    float cy = std::floor(rect_.center().y);
    Rect track{rect_.x + 6, cy - 2, rect_.w - 12, 4};
    c.roundRect(track, theme().box("LineEdit", "normal").bg, 2);
    float f = maxValue > minValue ? (value - minValue) / (maxValue - minValue) : 0;
    c.roundRect({track.x, track.y, track.w * f, track.h}, color("accent"), 2);
    float x = track.x + track.w * f;
    c.roundRect({x - 6, cy - 6, 12, 12}, hovered() || dragging_ ? color("font_hover_color") : color("font_color"), 6);
}

bool Slider::event(const InputEvent& e) {
    using T = InputEvent::Type;
    auto fromX = [&](float x) {
        float f = clampv((x - rect_.x - 6) / std::max(1.0f, rect_.w - 12), 0.0f, 1.0f);
        setValue(minValue + f * (maxValue - minValue));
    };
    if (e.type == T::MouseDown && e.button == MouseButton::Left) {
        dragging_ = true;
        fromX(e.pos.x);
        return true;
    }
    if (e.type == T::MouseMove && dragging_) {
        fromX(e.pos.x);
        return true;
    }
    if (e.type == T::MouseUp && dragging_) {
        dragging_ = false;
        return true;
    }
    if (e.type == T::Wheel) {
        float s = step > 0 ? step : (maxValue - minValue) / 20;
        setValue(value + (e.delta.y > 0 ? s : -s));
        return true;
    }
    return false;
}

bool Slider::setProperty(std::string_view key, std::string_view v) {
    if (key == "value") value = parseFloat(v);
    else if (key == "min") minValue = parseFloat(v);
    else if (key == "max") maxValue = parseFloat(v, 1);
    else if (key == "step") step = parseFloat(v);
    else return Control::setProperty(key, v);
    return true;
}

bool Slider::connectSignal(std::string_view s, std::function<void()> fn) {
    if (s == "value_changed") {
        valueChanged.connect([fn](float) { fn(); });
        return true;
    }
    return Control::connectSignal(s, std::move(fn));
}

// ---------------------------------------------------------------------------------- line edit
Vec2 LineEdit::computeMinSize() {
    const StyleBox& b = box("normal");
    return {std::ceil(40 + b.margin[0] + b.margin[2]), std::ceil(font().lineHeight(fontSize()) + b.margin[1] + b.margin[3])};
}

float LineEdit::textX() const { return rect_.x + box("normal").margin[0] - scrollX_; }

void LineEdit::setText(std::string t) {
    if (t == text_) return;
    text_ = std::move(t);
    caret_ = selStart_ = std::min(caret_, text_.size());
    caret_ = selStart_ = text_.size();
    scrollX_ = 0;
}

void LineEdit::selectAll() {
    selStart_ = 0;
    caret_ = text_.size();
}

void LineEdit::eraseSelection() {
    if (!hasSelection()) return;
    size_t a = std::min(selStart_, caret_), b = std::max(selStart_, caret_);
    text_.erase(a, b - a);
    caret_ = selStart_ = a;
}

void LineEdit::insert(std::string_view s) {
    if (readOnly) return;
    eraseSelection();
    text_.insert(caret_, s);
    caret_ += s.size();
    selStart_ = caret_;
    edited_ = true;
    textChanged.emit(text_);
}

void LineEdit::draw(Canvas& c) {
    const StyleBox& b = hasFocus() ? box("focus") : box("normal");
    c.styleBox(rect_, b);
    Rect inner = rect_.shrink(b.margin[0], 0, b.margin[2], 0);
    float px = fontSize();
    // Keep the caret visible.
    float caretX = font().measure(std::string_view(text_).substr(0, caret_), px);
    if (caretX - scrollX_ > inner.w - 2) scrollX_ = caretX - inner.w + 2;
    if (caretX - scrollX_ < 0) scrollX_ = caretX;
    c.pushClip(inner);
    float lh = font().lineHeight(px);
    float ty = std::floor(rect_.y + (rect_.h - lh) * 0.5f);
    if (hasSelection() && hasFocus()) {
        size_t a = std::min(selStart_, caret_), e = std::max(selStart_, caret_);
        float x0 = font().measure(std::string_view(text_).substr(0, a), px), x1 = font().measure(std::string_view(text_).substr(0, e), px);
        c.rect({inner.x + x0 - scrollX_, ty, x1 - x0, lh}, color("selection_color"));
    }
    if (text_.empty() && !placeholder.empty() && !hasFocus())
        c.text({inner.x, ty}, placeholder, font(), px, color("placeholder_color"));
    else
        c.text({inner.x - scrollX_, ty}, text_, font(), px, isDisabled() ? color("font_disabled_color") : color("font_color"));
    if (hasFocus() && !readOnly) {
        Context* ctx = context();
        double t = ctx ? ctx->time() - blinkStart_ : 0;
        if (std::fmod(t, 1.0) < 0.6) c.rect({inner.x + caretX - scrollX_, ty, 1, lh}, color("caret_color"));
    }
    c.popClip();
}

void LineEdit::focusChanged(bool on) {
    if (Context* ctx = context()) {
        blinkStart_ = ctx->time();
        if (ctx->textInput) ctx->textInput(on);
    }
    if (on && selectOnFocus && !mouseSelecting_) selectAll();
    if (!on) {
        selStart_ = caret_;
        if (edited_) {
            edited_ = false;
            submitted.emit(text_);
        }
    }
}

bool LineEdit::event(const InputEvent& e) {
    using T = InputEvent::Type;
    Context* ctx = context();
    if (ctx) blinkStart_ = ctx->time();
    float px = fontSize();
    auto hit = [&](float x) { return font().hitTest(text_, px, x - textX()); };
    if (e.type == T::MouseDown && e.button == MouseButton::Left) {
        if (e.clicks >= 2) {
            selectAll();
            return true;
        }
        bool hadFocus = hasFocus();
        if (!hadFocus) grabFocus();
        mouseSelecting_ = true;
        caret_ = hit(e.pos.x);
        if (!(e.mods & Mod::Shift)) selStart_ = caret_;
        return true;
    }
    if (e.type == T::MouseMove && mouseSelecting_) {
        caret_ = hit(e.pos.x);
        return true;
    }
    if (e.type == T::MouseUp && mouseSelecting_) {
        mouseSelecting_ = false;
        return true;
    }
    if (!hasFocus()) return false;
    if (e.type == T::Text) {
        insert(e.text);
        return true;
    }
    if (e.type != T::KeyDown) return e.type == T::KeyUp;
    bool shift = e.mods & Mod::Shift, ctrl = e.mods & (Mod::Ctrl | Mod::Super);
    auto move = [&](size_t to) {
        caret_ = to;
        if (!shift) selStart_ = caret_;
    };
    const std::string& k = e.key;
    if (k == "left") move(!shift && hasSelection() ? std::min(caret_, selStart_) : prevUtf8(text_, caret_));
    else if (k == "right") move(!shift && hasSelection() ? std::max(caret_, selStart_) : nextUtf8(text_, caret_));
    else if (k == "home") move(0);
    else if (k == "end") move(text_.size());
    else if (k == "backspace" && !readOnly) {
        if (!hasSelection()) selStart_ = prevUtf8(text_, caret_);
        eraseSelection();
        edited_ = true;
        textChanged.emit(text_);
    } else if (k == "delete" && !readOnly) {
        if (!hasSelection()) selStart_ = nextUtf8(text_, caret_);
        eraseSelection();
        edited_ = true;
        textChanged.emit(text_);
    } else if (k == "enter" || k == "kp_enter") {
        edited_ = false;
        submitted.emit(text_);
        selectAll();
    } else if (k == "escape") {
        releaseFocus();
    } else if (ctrl && k == "a") selectAll();
    else if (ctrl && (k == "c" || k == "x")) {
        if (hasSelection() && ctx && ctx->setClipboard) {
            size_t a = std::min(selStart_, caret_), b = std::max(selStart_, caret_);
            ctx->setClipboard(text_.substr(a, b - a));
            if (k == "x" && !readOnly) {
                eraseSelection();
                edited_ = true;
                textChanged.emit(text_);
            }
        }
    } else if (ctrl && k == "v") {
        if (ctx && ctx->getClipboard) {
            std::string clip = ctx->getClipboard();
            std::erase(clip, '\n');
            std::erase(clip, '\r');
            insert(clip);
        }
    } else if (k == "tab") {
        return false;
    } else if (ctrl) {
        return false;  // leave shortcuts to the application
    }
    return true;
}

bool LineEdit::setProperty(std::string_view key, std::string_view v) {
    if (key == "text") setText(std::string(v));
    else if (key == "placeholder") placeholder = std::string(v);
    else if (key == "readonly") readOnly = parseBool(v);
    else return Control::setProperty(key, v);
    return true;
}

bool LineEdit::connectSignal(std::string_view s, std::function<void()> fn) {
    if (s == "text_changed") {
        textChanged.connect([fn](const std::string&) { fn(); });
        return true;
    }
    if (s == "submitted") {
        submitted.connect([fn](const std::string&) { fn(); });
        return true;
    }
    return Control::connectSignal(s, std::move(fn));
}

// ---------------------------------------------------------------------------------- spin box
void SpinBox::setValue(double v, bool notify) {
    v = std::clamp(v, minValue, maxValue);
    bool changed = v != value_;
    value_ = v;
    if (!hasFocus()) setText(formatNumberShort(v, decimals) + suffix);
    if (changed && notify) valueChanged.emit(v);
}

void SpinBox::commit() {
    std::string t = text_;
    if (!suffix.empty() && t.ends_with(suffix)) t.resize(t.size() - suffix.size());
    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);
    if (end != t.c_str()) setValue(v);
    setText(formatNumberShort(value_, decimals) + suffix);
}

void SpinBox::focusChanged(bool on) {
    LineEdit::focusChanged(on);
    if (!on) commit();
}

bool SpinBox::event(const InputEvent& e) {
    using T = InputEvent::Type;
    if (!hasFocus()) {
        // Scrub with the mouse; a click without moving starts typing.
        if (e.type == T::MouseDown && e.button == MouseButton::Left) {
            scrubbing_ = true;
            scrubbed_ = false;
            scrubStartX_ = e.pos.x;
            scrubStartValue_ = value_;
            return true;
        }
        if (e.type == T::MouseMove && scrubbing_) {
            float dx = e.pos.x - scrubStartX_;
            if (std::fabs(dx) > 3) scrubbed_ = true;
            if (scrubbed_) {
                double s = step > 0 ? step : 1;
                if (e.mods & Mod::Shift) s *= 0.1;
                setValue(scrubStartValue_ + std::round(dx / 4.0) * s);
            }
            return true;
        }
        if (e.type == T::MouseUp && scrubbing_) {
            scrubbing_ = false;
            if (!scrubbed_) {
                grabFocus();
                selectAll();
            }
            return true;
        }
        if (e.type == T::Wheel && hovered()) {
            setValue(value_ + (e.delta.y > 0 ? step : -step));
            return true;
        }
        return false;
    }
    if (e.type == T::KeyDown && (e.key == "up" || e.key == "down")) {
        commit();
        setValue(value_ + (e.key == "up" ? step : -step));
        setText(formatNumberShort(value_, decimals) + suffix);
        selectAll();
        return true;
    }
    if (e.type == T::KeyDown && (e.key == "enter" || e.key == "kp_enter")) {
        commit();
        selectAll();
        return true;
    }
    return LineEdit::event(e);
}

bool SpinBox::setProperty(std::string_view key, std::string_view v) {
    if (key == "value") setValue(parseFloat(v), false);
    else if (key == "min") minValue = parseFloat(v);
    else if (key == "max") maxValue = parseFloat(v);
    else if (key == "step") step = parseFloat(v, 1);
    else if (key == "decimals") decimals = parseInt(v, 2);
    else if (key == "suffix") suffix = std::string(v);
    else return LineEdit::setProperty(key, v);
    return true;
}

bool SpinBox::connectSignal(std::string_view s, std::function<void()> fn) {
    if (s == "value_changed") {
        valueChanged.connect([fn](double) { fn(); });
        return true;
    }
    return LineEdit::connectSignal(s, std::move(fn));
}

}  // namespace ose::ui
