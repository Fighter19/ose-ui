#include "ui/theme.h"

#include <unordered_map>

#include "core/util.h"
#include "ui/xml.h"

namespace ose::ui {

template <class M>
const typename M::mapped_type* Theme::find(const M& m, std::string_view type, std::string_view name) const {
    std::string t(type);
    for (int depth = 0; depth < 8; ++depth) {
        auto it = m.find(key(t, name));
        if (it != m.end()) return &it->second;
        auto b = base_.find(t);
        if (b == base_.end()) break;
        t = b->second;
    }
    auto it = m.find(key("*", name));
    return it != m.end() ? &it->second : nullptr;
}

const StyleBox& Theme::box(std::string_view type, std::string_view name) const {
    static const StyleBox empty = [] {
        StyleBox s;
        for (float& m : s.margin) m = 0;
        return s;
    }();
    const StyleBox* b = find(boxes_, type, name);
    return b ? *b : empty;
}

bool Theme::hasBox(std::string_view type, std::string_view name) const { return find(boxes_, type, name) != nullptr; }

Color Theme::color(std::string_view type, std::string_view name) const {
    const Color* c = find(colors_, type, name);
    return c ? *c : Color{1, 0, 1, 1};
}

float Theme::constant(std::string_view type, std::string_view name) const {
    const float* c = find(constants_, type, name);
    return c ? *c : 0.0f;
}

std::shared_ptr<Theme> Theme::makeDark(const std::string& fontDir) {
    auto t = std::make_shared<Theme>();
    t->font = Font::loadFile(fontDir + "/DejaVuSans.ttf");
    t->boldFont = Font::loadFile(fontDir + "/DejaVuSans-Bold.ttf");
    t->monoFont = Font::loadFile(fontDir + "/DejaVuSansMono.ttf");
    t->iconFont = Font::loadFile(fontDir + "/fontawesome-webfont.ttf");
    if (!t->boldFont) t->boldFont = t->font;
    if (!t->monoFont) t->monoFont = t->font;

    const Color bg = parseColor("#1c1d20"), panel = parseColor("#25272b"), panelAlt = parseColor("#2e3035"),
                input = parseColor("#18191c"), button = parseColor("#363940"), buttonHover = parseColor("#42464e"),
                accent = parseColor("#3d8bfd"), selection = parseColor("#2b5797"), border = parseColor("#101113"),
                text = parseColor("#d7d9de"), dim = parseColor("#8b9099"), disabled = parseColor("#5c6068");
    Theme& th = *t;
    // Defaults for every type.
    th.setColor("*", "font_color", text);
    th.setColor("*", "font_hover_color", parseColor("#ffffff"));
    th.setColor("*", "font_pressed_color", parseColor("#ffffff"));
    th.setColor("*", "font_disabled_color", disabled);
    th.setColor("*", "font_dim_color", dim);
    th.setColor("*", "accent", accent);
    th.setColor("*", "selection", selection);
    th.setColor("*", "font_selected_color", parseColor("#ffffff"));  // text on a selected row
    th.setColor("*", "background", bg);
    th.setColor("*", "warning", parseColor("#e5a50a"));
    th.setColor("*", "error", parseColor("#e5484d"));
    th.setColor("*", "success", parseColor("#46a758"));
    th.setConstant("*", "separation", 4);
    th.setConstant("*", "h_separation", 4);
    th.setConstant("*", "v_separation", 4);

    th.setBox("Panel", "panel", StyleBox::flat(panel, 0, 0));
    th.setBox("PanelContainer", "panel", StyleBox::flat(panel, 0, 6));
    th.setBox("Background", "panel", StyleBox::flat(bg, 0, 0));
    th.setBase("Background", "Panel");
    th.setBox("Toolbar", "panel", StyleBox::flat(panelAlt, 0, 3).withBorder(border, 0));
    th.setBase("Toolbar", "PanelContainer");
    th.setBox("Header", "panel", StyleBox::flat(panelAlt, 0, 4));
    th.setBase("Header", "PanelContainer");
    th.setBox("StatusBar", "panel", StyleBox::flat(panelAlt, 0, 3));
    th.setBase("StatusBar", "PanelContainer");

    StyleBox btn = StyleBox::flat(button, 3, 5).withMargins(8, 4, 8, 4);
    th.setBox("Button", "normal", btn);
    StyleBox bh = btn;
    bh.bg = buttonHover;
    th.setBox("Button", "hover", bh);
    StyleBox bp = btn;
    bp.bg = accent;
    th.setBox("Button", "pressed", bp);
    StyleBox bd = btn;
    bd.bg = withAlpha(button, 0.5f);
    th.setBox("Button", "disabled", bd);
    StyleBox focus;
    focus.border = withAlpha(accent, 0.8f);
    for (float& w : focus.borderWidth) w = 1;
    focus.radius = 3;
    th.setBox("*", "focus", focus);

    // Flat buttons for toolbars and tool palettes.
    th.setBase("ToolButton", "Button");
    StyleBox tb = btn;
    tb.bg = {0, 0, 0, 0};
    tb.withMargins(6, 4, 6, 4);
    th.setBox("ToolButton", "normal", tb);
    StyleBox tbh = tb;
    tbh.bg = withAlpha(buttonHover, 0.8f);
    th.setBox("ToolButton", "hover", tbh);
    StyleBox tbp = tb;
    tbp.bg = selection;
    th.setBox("ToolButton", "pressed", tbp);
    th.setBox("ToolButton", "disabled", tb);
    // Big play button.
    th.setBase("PlayButton", "Button");
    StyleBox pb = btn;
    pb.bg = parseColor("#2f7d32");
    th.setBox("PlayButton", "normal", pb);
    StyleBox pbh = pb;
    pbh.bg = parseColor("#3a9a3e");
    th.setBox("PlayButton", "hover", pbh);
    StyleBox pbp = pb;
    pbp.bg = parseColor("#246428");
    th.setBox("PlayButton", "pressed", pbp);

    th.setBase("CheckBox", "ToolButton");
    th.setBase("OptionButton", "Button");
    th.setBase("MenuButton", "ToolButton");

    StyleBox le = StyleBox::flat(input, 3, 4).withBorder(border, 1).withMargins(6, 4, 6, 4);
    th.setBox("LineEdit", "normal", le);
    StyleBox lef = le;
    lef.border = accent;
    th.setBox("LineEdit", "focus", lef);
    th.setColor("LineEdit", "placeholder_color", dim);
    th.setColor("LineEdit", "selection_color", withAlpha(selection, 0.9f));
    th.setColor("LineEdit", "caret_color", text);
    th.setBase("SpinBox", "LineEdit");

    StyleBox popup = StyleBox::flat(panelAlt, 4, 4).withBorder(border, 1);
    th.setBox("PopupMenu", "panel", popup);
    th.setBox("PopupMenu", "hover", StyleBox::flat(selection, 3, 0));
    th.setColor("PopupMenu", "font_accelerator_color", dim);
    th.setColor("PopupMenu", "separator", parseColor("#3c3f46"));
    th.setConstant("PopupMenu", "item_height", 24);
    th.setBox("MenuBar", "panel", StyleBox::flat(panelAlt, 0, 0));

    th.setBox("TabContainer", "panel", StyleBox::flat(panel, 0, 0));
    th.setBox("TabContainer", "tabbar_background", StyleBox::flat(parseColor("#1f2124"), 0, 0));
    StyleBox ts = StyleBox::flat(panel, 0, 0).withMargins(10, 5, 10, 5);
    ts.radius = 3;
    th.setBox("TabContainer", "tab_selected", ts);
    StyleBox tu = ts;
    tu.bg = {0, 0, 0, 0};
    th.setBox("TabContainer", "tab_unselected", tu);
    StyleBox thv = ts;
    thv.bg = withAlpha(buttonHover, 0.6f);
    th.setBox("TabContainer", "tab_hover", thv);
    th.setColor("TabContainer", "font_unselected_color", dim);
    th.setColor("TabContainer", "tab_accent", accent);

    th.setBox("Tree", "panel", StyleBox::flat(input, 0, 2));
    th.setBox("Tree", "selected", StyleBox::flat(selection, 2, 0));
    th.setBox("Tree", "hover", StyleBox::flat(withAlpha(buttonHover, 0.5f), 2, 0));
    th.setColor("Tree", "guide_color", parseColor("#3a3d44"));
    th.setConstant("Tree", "item_height", 22);
    th.setConstant("Tree", "indent", 16);
    th.setBase("ItemList", "Tree");

    th.setBox("ScrollBar", "scroll", StyleBox::flat({0, 0, 0, 0.15f}, 4, 0));
    th.setBox("ScrollBar", "grabber", StyleBox::flat(parseColor("#4a4e57"), 4, 0));
    th.setBox("ScrollBar", "grabber_hover", StyleBox::flat(parseColor("#5d626d"), 4, 0));
    th.setConstant("ScrollBar", "width", 10);

    th.setConstant("Split", "separation", 5);
    th.setColor("Split", "grabber", border);
    th.setColor("Split", "grabber_hover", accent);

    th.setBox("Tooltip", "panel", StyleBox::flat(parseColor("#101114"), 3, 6).withBorder(parseColor("#3c3f46"), 1));
    th.setBox("ProgressBar", "background", StyleBox::flat(input, 3, 0));
    th.setBox("ProgressBar", "fill", StyleBox::flat(accent, 3, 0));
    th.setColor("Separator", "color", parseColor("#3a3d44"));
    th.setBox("Window", "panel", StyleBox::flat(panel, 5, 0).withBorder(border, 1));
    th.setBox("Window", "titlebar", StyleBox::flat(panelAlt, 5, 6));
    th.setBox("Viewport", "frame", StyleBox().withBorder(border, 1));
    th.setColor("Viewport", "focus_border", withAlpha(accent, 0.7f));
    return t;
}

bool Theme::loadOverrides(std::string_view text, std::string* error) {
    auto doc = parseXml(text);
    if (!doc) {
        if (error) *error = doc.error();
        return false;
    }
    for (const auto& n : doc->children) {
        std::string type(n.get("type", "*")), name(n.get("name"));
        if (n.name == "color") {
            setColor(type, name, parseColor(n.get("value")));
        } else if (n.name == "constant") {
            setConstant(type, name, parseFloat(n.get("value")));
        } else if (n.name == "base") {
            setBase(type, std::string(n.get("base")));
        } else if (n.name == "box") {
            StyleBox b = hasBox(type, name) ? box(type, name) : StyleBox{};
            if (n.has("bg")) b.bg = parseColor(n.get("bg"));
            if (n.has("border")) b.border = parseColor(n.get("border"));
            if (n.has("border_width")) {
                auto p = splitWhitespace(n.get("border_width"));
                for (int k = 0; k < 4; ++k) b.borderWidth[k] = parseFloat(p.empty() ? "0" : p[size_t(std::min<int>(k, int(p.size()) - 1))]);
            }
            if (n.has("radius")) b.radius = parseFloat(n.get("radius"));
            if (n.has("bevel")) {
                std::string_view v = n.get("bevel");
                b.bevel = v == "raised" ? StyleBox::Bevel::Raised : v == "sunken" ? StyleBox::Bevel::Sunken : StyleBox::Bevel::None;
            }
            if (n.has("margin")) {
                auto p = splitWhitespace(n.get("margin"));
                for (int k = 0; k < 4; ++k) b.margin[k] = parseFloat(p.empty() ? "0" : p[size_t(std::min<int>(k, int(p.size()) - 1))]);
            }
            setBox(type, name, b);
        }
    }
    return true;
}

uint32_t iconCodepoint(std::string_view name) {
    // Font Awesome 4.7 names (fa-<name>) for the icons the editor uses.
    static const std::unordered_map<std::string_view, uint32_t> table = {
        {"play", 0xf04b}, {"stop", 0xf04d}, {"pause", 0xf04c}, {"step-forward", 0xf051}, {"fast-forward", 0xf050},
        {"folder", 0xf07b}, {"folder-open", 0xf07c}, {"file", 0xf15b}, {"file-o", 0xf016}, {"save", 0xf0c7},
        {"undo", 0xf0e2}, {"redo", 0xf01e}, {"cube", 0xf1b2}, {"cubes", 0xf1b3}, {"lightbulb", 0xf0eb},
        {"search", 0xf002}, {"cog", 0xf013}, {"pointer", 0xf245}, {"square-o", 0xf096}, {"square", 0xf0c8},
        {"scissors", 0xf0c4}, {"paint-brush", 0xf1fc}, {"eye", 0xf06e}, {"eye-slash", 0xf070}, {"move", 0xf047},
        {"refresh", 0xf021}, {"plus", 0xf067}, {"minus", 0xf068}, {"times", 0xf00d}, {"check", 0xf00c},
        {"caret-down", 0xf0d7}, {"caret-right", 0xf0da}, {"chevron-down", 0xf078}, {"chevron-right", 0xf054},
        {"image", 0xf03e}, {"music", 0xf001}, {"volume-up", 0xf028}, {"magic", 0xf0d0}, {"bolt", 0xf0e7},
        {"home", 0xf015}, {"sun", 0xf185}, {"moon", 0xf186}, {"film", 0xf008}, {"gamepad", 0xf11b},
        {"street-view", 0xf21d}, {"globe", 0xf0ac}, {"map", 0xf279}, {"map-marker", 0xf041}, {"th", 0xf00a},
        {"th-large", 0xf009}, {"list", 0xf03a}, {"sliders", 0xf1de}, {"tags", 0xf02c}, {"star", 0xf005},
        {"star-o", 0xf006}, {"trash", 0xf1f8}, {"copy", 0xf0c5}, {"paste", 0xf0ea}, {"terminal", 0xf120},
        {"warning", 0xf071}, {"info", 0xf05a}, {"error", 0xf057}, {"bars", 0xf0c9}, {"video", 0xf03d},
        {"crosshairs", 0xf05b}, {"group", 0xf247}, {"ungroup", 0xf248}, {"clone", 0xf24d}, {"link", 0xf0c1},
        {"unlink", 0xf127}, {"lock", 0xf023}, {"unlock", 0xf09c}, {"expand", 0xf065}, {"compress", 0xf066},
        {"fullscreen", 0xf0b2}, {"pencil", 0xf040}, {"eyedropper", 0xf1fb}, {"fire", 0xf06d}, {"bug", 0xf188},
        {"flask", 0xf0c3}, {"wrench", 0xf0ad}, {"exchange", 0xf0ec}, {"circle", 0xf111}, {"circle-o", 0xf10c},
        {"dot-circle", 0xf192}, {"hand", 0xf256}, {"object", 0xf1b2}, {"users", 0xf0c0}, {"road", 0xf018},
        {"arrows-v", 0xf07d}, {"arrows-h", 0xf07e}, {"external", 0xf08e}, {"filter", 0xf0b0}, {"sitemap", 0xf0e8},
        {"cogs", 0xf085}, {"camera", 0xf030}, {"puzzle", 0xf12e},
        {"floppy-o", 0xf0c7}, {"repeat", 0xf01e}, {"picture-o", 0xf03e}, {"lightbulb-o", 0xf0eb}, {"mouse-pointer", 0xf245},
        {"folder-o", 0xf114}, {"folder-open-o", 0xf115}, {"share-square-o", 0xf045}, {"sign-out", 0xf08b}, {"sun-o", 0xf185},
        {"clock-o", 0xf017}, {"level-up", 0xf148}, {"times-circle", 0xf057}, {"exclamation-triangle", 0xf071},
        {"info-circle", 0xf05a}, {"columns", 0xf0db}, {"trash-o", 0xf014},
        {"key", 0xf084}, {"tint", 0xf043}, {"adjust", 0xf042}, {"crop", 0xf125}, {"object-group", 0xf247},
    };
    auto it = table.find(name);
    return it == table.end() ? 0 : it->second;
}

std::shared_ptr<Theme> Theme::makeClassic(const std::string& fontDir) {
    // Start from the dark theme so every control type is covered, then repaint.
    auto t = makeDark(fontDir);
    Theme& th = *t;
    t->fontSize = 12;
    const Color face = parseColor("#c0c0c0"), window = parseColor("#ffffff"), text = parseColor("#000000"),
                dim = parseColor("#404040"), disabled = parseColor("#808080"), select = parseColor("#000080"),
                selectText = parseColor("#ffffff"), title = parseColor("#000080");
    using B = StyleBox::Bevel;

    th.setColor("*", "font_color", text);
    th.setColor("*", "font_hover_color", text);
    th.setColor("*", "font_pressed_color", text);
    th.setColor("*", "font_disabled_color", disabled);
    th.setColor("*", "font_dim_color", dim);
    th.setColor("*", "accent", title);
    th.setColor("*", "selection", select);
    th.setColor("*", "font_selected_color", selectText);  // text on a selected row
    th.setColor("*", "background", face);

    th.setBox("Panel", "panel", StyleBox::flat(face, 0, 0));
    th.setBox("PanelContainer", "panel", StyleBox::beveled(face, B::Raised, 6));
    th.setBox("Background", "panel", StyleBox::flat(parseColor("#808080"), 0, 0));
    th.setBox("Toolbar", "panel", StyleBox::beveled(face, B::Raised, 3));
    th.setBox("Header", "panel", StyleBox::flat(title, 0, 4));
    th.setColor("Header", "font_color", selectText);
    th.setBox("StatusBar", "panel", StyleBox::beveled(face, B::Sunken, 3));

    StyleBox btn = StyleBox::beveled(face, B::Raised).withMargins(8, 4, 8, 4);
    th.setBox("Button", "normal", btn);
    th.setBox("Button", "hover", btn);
    StyleBox pressed = btn;
    pressed.bevel = B::Sunken;
    th.setBox("Button", "pressed", pressed);
    th.setBox("Button", "disabled", btn);
    StyleBox focus;
    focus.border = parseColor("#000000");
    for (float& w : focus.borderWidth) w = 1;
    th.setBox("*", "focus", focus);

    StyleBox tool = StyleBox::flat({0, 0, 0, 0}, 0, 4).withMargins(6, 4, 6, 4);
    th.setBox("ToolButton", "normal", tool);
    StyleBox toolHover = tool;
    toolHover.bevel = B::Raised;
    th.setBox("ToolButton", "hover", toolHover);
    StyleBox toolPressed = tool;
    toolPressed.bevel = B::Sunken;
    th.setBox("ToolButton", "pressed", toolPressed);
    th.setBox("ToolButton", "disabled", tool);
    th.setBox("PlayButton", "normal", btn);
    th.setBox("PlayButton", "hover", btn);
    th.setBox("PlayButton", "pressed", pressed);

    StyleBox field = StyleBox::beveled(window, B::Sunken).withMargins(5, 4, 5, 4);
    th.setBox("LineEdit", "normal", field);
    th.setBox("LineEdit", "focus", field);
    th.setColor("LineEdit", "selection_color", select);
    th.setColor("LineEdit", "caret_color", text);
    th.setColor("LineEdit", "placeholder_color", disabled);

    th.setBox("PopupMenu", "panel", StyleBox::beveled(face, B::Raised, 3));
    th.setBox("PopupMenu", "hover", StyleBox::flat(select, 0, 0));
    th.setColor("PopupMenu", "font_hover_color", selectText);
    th.setColor("PopupMenu", "separator", parseColor("#808080"));
    th.setBox("MenuBar", "panel", StyleBox::flat(face, 0, 0));

    th.setBox("TabContainer", "panel", StyleBox::beveled(face, B::Raised, 2));
    th.setBox("TabContainer", "tabbar_background", StyleBox::flat(face, 0, 0));
    StyleBox tabSel = StyleBox::beveled(face, B::Raised).withMargins(10, 5, 10, 5);
    th.setBox("TabContainer", "tab_selected", tabSel);
    StyleBox tabUnsel = tabSel;
    tabUnsel.bg = parseColor("#b0b0b0");
    th.setBox("TabContainer", "tab_unselected", tabUnsel);
    th.setBox("TabContainer", "tab_hover", tabSel);
    th.setColor("TabContainer", "font_unselected_color", dim);
    th.setColor("TabContainer", "tab_accent", title);

    th.setBox("Tree", "panel", StyleBox::beveled(window, B::Sunken, 3));
    th.setBox("Tree", "selected", StyleBox::flat(select, 0, 0));
    th.setColor("Tree", "font_selected_color", selectText);
    th.setBox("Tree", "hover", StyleBox::flat({0, 0, 0, 0}, 0, 0));
    th.setColor("Tree", "guide_color", parseColor("#808080"));
    th.setBox("ItemList", "panel", StyleBox::beveled(window, B::Sunken, 3));
    th.setBox("ItemList", "selected", StyleBox::flat(select, 0, 0));

    th.setBox("ScrollBar", "scroll", StyleBox::flat(parseColor("#dfdfdf"), 0, 0));
    th.setBox("ScrollBar", "grabber", StyleBox::beveled(face, B::Raised, 0));
    th.setBox("ScrollBar", "grabber_hover", StyleBox::beveled(face, B::Raised, 0));
    th.setConstant("ScrollBar", "width", 16);
    th.setColor("Split", "grabber", face);
    th.setColor("Split", "grabber_hover", parseColor("#808080"));
    th.setBox("Tooltip", "panel", StyleBox::flat(parseColor("#ffffe1"), 0, 4).withBorder(text, 1));
    th.setColor("Tooltip", "font_color", text);
    th.setBox("ProgressBar", "background", StyleBox::beveled(face, B::Sunken, 0));
    th.setBox("ProgressBar", "fill", StyleBox::flat(select, 0, 0));
    th.setColor("Separator", "color", parseColor("#808080"));
    th.setBox("Window", "panel", StyleBox::beveled(face, B::Raised, 0));
    th.setBox("Window", "titlebar", StyleBox::flat(title, 0, 4));
    th.setColor("Window", "title_color", selectText);
    th.setBox("Viewport", "frame", StyleBox::beveled({0, 0, 0, 0}, B::Sunken, 0));
    return t;
}

}  // namespace ose::ui

