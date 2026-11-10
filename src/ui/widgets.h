// Basic controls: labels, buttons, check boxes, text and number fields, sliders, progress
// bars, separators, images.
#pragma once
#include "ui/containers.h"

namespace ose::ui {

class Label : public Control {
public:
    Label() { mouseFilter = MouseFilter::Ignore; }
    explicit Label(std::string t) : Label() { text = std::move(t); }
    const char* typeName() const override { return "Label"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    std::string text;
    Align align = Align::Begin;
    bool ellipsis = true;   // shorten with "..." when it gets less room than it needs
    bool clipText = false;  // minimum width 0 (otherwise the whole text)
    bool bold = false;
    bool dim = false;       // the theme's dim text colour
    bool wrap = false;      // break into lines at the width it gets
    Color colorOverride{0, 0, 0, 0};  // alpha 0 = theme colour
    float sizeScale = 1;

private:
    std::vector<std::string> wrapLines(float width) const;
};

class Button : public Control {
public:
    Button() { focusMode = FocusMode::Click; }
    explicit Button(std::string t) : Button() { text = std::move(t); }
    const char* typeName() const override { return "Button"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    Cursor cursor(Vec2) const override { return Cursor::Hand; }
    bool setProperty(std::string_view key, std::string_view value) override;
    bool connectSignal(std::string_view s, std::function<void()> fn) override;
    void mouseExited() override { down_ = false; }

    std::string text;
    std::string shortcutText;  // drawn dimmed on the right (menus)
    bool toggleMode = false;
    bool pressed = false;      // toggle state
    bool flat = false;
    Align align = Align::Center;
    Signal<> clicked;
    Signal<bool> toggled;
    std::string action;  // run through the context's actions when clicked

    void setPressed(bool p);

protected:
    virtual void activate();
    const StyleBox& currentBox() const;
    Color currentFontColor() const;
    bool down_ = false;
};

class CheckBox : public Button {
public:
    CheckBox() {
        toggleMode = true;
        align = Align::Begin;
    }
    const char* typeName() const override { return "CheckBox"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
};

class Separator : public Control {
public:
    explicit Separator(bool vertical = false) : vertical_(vertical) { mouseFilter = MouseFilter::Ignore; }
    const char* typeName() const override { return "Separator"; }
    Vec2 computeMinSize() override { return vertical_ ? Vec2{7, 0} : Vec2{0, 7}; }
    void draw(Canvas& c) override;
    bool setProperty(std::string_view key, std::string_view value) override;

private:
    bool vertical_;
};

class ColorRect : public Control {
public:
    const char* typeName() const override { return "ColorRect"; }
    void draw(Canvas& c) override { c.roundRect(rect_, fill, radius); }
    bool setProperty(std::string_view key, std::string_view value) override;
    Color fill{1, 1, 1, 1};
    float radius = 0;
};

class TextureRect : public Control {
public:
    const char* typeName() const override { return "TextureRect"; }
    Vec2 computeMinSize() override { return keepAspect ? Vec2{} : textureSize; }
    void draw(Canvas& c) override;
    TextureHandle texture;
    Vec2 textureSize;
    bool keepAspect = true;
    Rect uv{0, 0, 1, 1};
};

class ProgressBar : public Control {
public:
    const char* typeName() const override { return "ProgressBar"; }
    Vec2 computeMinSize() override { return {32, font().lineHeight(fontSize()) + 4}; }
    void draw(Canvas& c) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    float value = 0;  // 0..1
    std::string text;
};

class Slider : public Control {
public:
    Slider() { focusMode = FocusMode::Click; }
    const char* typeName() const override { return "Slider"; }
    Vec2 computeMinSize() override { return {64, 18}; }
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    bool connectSignal(std::string_view s, std::function<void()> fn) override;
    void setValue(float v, bool notify = true);
    float value = 0, minValue = 0, maxValue = 1, step = 0;
    Signal<float> valueChanged;

private:
    bool dragging_ = false;
};

// One line of editable text.
class LineEdit : public Control {
public:
    LineEdit() {
        focusMode = FocusMode::Click;
        clipContents = true;
    }
    const char* typeName() const override { return "LineEdit"; }
    Vec2 computeMinSize() override;
    void draw(Canvas& c) override;
    bool event(const InputEvent& e) override;
    Cursor cursor(Vec2) const override { return Cursor::IBeam; }
    void focusChanged(bool on) override;
    bool setProperty(std::string_view key, std::string_view value) override;
    bool connectSignal(std::string_view s, std::function<void()> fn) override;

    const std::string& text() const { return text_; }
    void setText(std::string t);
    void selectAll();
    std::string placeholder;
    bool readOnly = false;
    bool selectOnFocus = true;
    Signal<const std::string&> textChanged;   // every edit
    Signal<const std::string&> submitted;     // Enter, or focus lost after an edit

protected:
    void insert(std::string_view s);
    void eraseSelection();
    bool hasSelection() const { return selStart_ != caret_; }
    float textX() const;
    std::string text_;
    size_t caret_ = 0, selStart_ = 0;
    float scrollX_ = 0;
    bool mouseSelecting_ = false;
    bool edited_ = false;
    double blinkStart_ = 0;
};

// A number field: type a value, drag left/right to scrub, wheel to step.
class SpinBox : public LineEdit {
public:
    SpinBox() {
        selectOnFocus = true;
        focusMode = FocusMode::None;  // focused by a click without dragging (see event)
    }
    const char* typeName() const override { return "SpinBox"; }
    bool event(const InputEvent& e) override;
    Cursor cursor(Vec2) const override { return hasFocus() ? Cursor::IBeam : Cursor::ResizeH; }
    bool setProperty(std::string_view key, std::string_view value) override;
    bool connectSignal(std::string_view s, std::function<void()> fn) override;
    void focusChanged(bool on) override;
    double value() const { return value_; }
    void setValue(double v, bool notify = true);
    double minValue = -1e9, maxValue = 1e9, step = 1;
    int decimals = 2;
    std::string suffix;
    Signal<double> valueChanged;

private:
    void commit();
    double value_ = 0;
    bool scrubbing_ = false, scrubbed_ = false;
    float scrubStartX_ = 0;
    double scrubStartValue_ = 0;
};

std::string formatNumberShort(double v, int decimals);

}  // namespace ose::ui
