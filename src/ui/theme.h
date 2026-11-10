// Themes in the style of Godot's: per control type a set of style boxes, colours and
// constants, looked up by name, with type variations ("ToolButton" based on "Button").
#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "ui/core.h"
#include "ui/font.h"

namespace ose::ui {

// How a rectangle is drawn: background, border, rounded corners, inner margins.
struct StyleBox {
    Color bg{0, 0, 0, 0};
    Color border{0, 0, 0, 0};
    float borderWidth[4] = {0, 0, 0, 0};  // left, top, right, bottom
    float radius = 0;
    float margin[4] = {4, 3, 4, 3};       // content margins: left, top, right, bottom
    // Classic 3D edges (two pixel rings): raised = light top/left, dark bottom/right;
    // sunken = the other way round. Drawn on top of bg; `border` is ignored when set.
    enum class Bevel { None, Raised, Sunken };
    Bevel bevel = Bevel::None;
    Color bevelHighlight{1, 1, 1, 1};     // outer top/left of a raised edge
    Color bevelLight{0.875f, 0.875f, 0.875f, 1};
    Color bevelShade{0.5f, 0.5f, 0.5f, 1};
    Color bevelShadow{0, 0, 0, 1};        // outer bottom/right of a raised edge
    bool visible() const { return bg.w > 0 || bevel != Bevel::None || (border.w > 0 && (borderWidth[0] + borderWidth[1] + borderWidth[2] + borderWidth[3]) > 0); }
    static StyleBox beveled(Color bg, Bevel b, float margin = 4) {
        StyleBox s = flat(bg, 0, margin);
        s.bevel = b;
        return s;
    }
    static StyleBox flat(Color bg, float radius = 0, float margin = 4) {
        StyleBox s;
        s.bg = bg;
        s.radius = radius;
        for (float& m : s.margin) m = margin;
        return s;
    }
    StyleBox& withBorder(Color c, float w) {
        border = c;
        for (float& b : borderWidth) b = w;
        return *this;
    }
    StyleBox& withMargins(float l, float t, float r, float b) {
        margin[0] = l, margin[1] = t, margin[2] = r, margin[3] = b;
        return *this;
    }
};

class Theme {
public:
    // Lookups walk the variation chain: "ToolButton" -> "Button" -> defaults.
    const StyleBox& box(std::string_view type, std::string_view name) const;
    Color color(std::string_view type, std::string_view name) const;
    float constant(std::string_view type, std::string_view name) const;
    void setBox(std::string type, std::string name, StyleBox b) { boxes_[key(type, name)] = b; }
    void setColor(std::string type, std::string name, Color c) { colors_[key(type, name)] = c; }
    void setConstant(std::string type, std::string name, float v) { constants_[key(type, name)] = v; }
    void setBase(std::string variation, std::string base) { base_[std::move(variation)] = std::move(base); }
    bool hasBox(std::string_view type, std::string_view name) const;

    std::shared_ptr<Font> font, boldFont, monoFont, iconFont;
    float fontSize = 13;
    float scale = 1;  // DPI scale applied to sizes by controls

    // The editor's dark theme (in the spirit of Source 2 Hammer). Fonts from `fontDir`
    // (engine/ui/fonts); returns a theme without fonts if they cannot be read.
    static std::shared_ptr<Theme> makeDark(const std::string& fontDir);
    // A classic light-grey theme with raised and sunken 3D edges, in the style of 1990s
    // desktop tools (Lunar Magic, RPG Maker 2000). Same control types as makeDark.
    static std::shared_ptr<Theme> makeClassic(const std::string& fontDir);
    // Overrides from a theme file (XML: <theme><box type= name= bg= border= .../>
    // <color type= name= value=/> <constant type= name= value=/></theme>).
    bool loadOverrides(std::string_view xml, std::string* error = nullptr);

private:
    static std::string key(std::string_view type, std::string_view name) {
        std::string k(type);
        k += '.';
        k += name;
        return k;
    }
    template <class M>
    const typename M::mapped_type* find(const M& m, std::string_view type, std::string_view name) const;
    std::unordered_map<std::string, StyleBox> boxes_;
    std::unordered_map<std::string, Color> colors_;
    std::unordered_map<std::string, float> constants_;
    std::unordered_map<std::string, std::string> base_;
};

// Font Awesome 4.7 code points by name (the icon font in engine/ui/fonts).
uint32_t iconCodepoint(std::string_view name);

}  // namespace ose::ui
