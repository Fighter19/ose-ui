#include "ui/loader.h"

#include <mutex>
#include <unordered_map>

#include "core/util.h"
#include "fs/filesystem.h"
#include "ui/context.h"
#include "ui/dock.h"
#include "ui/lists.h"
#include "ui/menus.h"

namespace ose::ui {

namespace {

std::unordered_map<std::string, ControlFactory>& factories() {
    static std::unordered_map<std::string, ControlFactory> f;
    static std::once_flag once;
    std::call_once(once, [] {
        auto reg = [](const char* n, ControlFactory fn) { f[n] = std::move(fn); };
        reg("Control", [] { return std::make_unique<Control>(); });
        reg("HBox", [] { return std::make_unique<HBox>(); });
        reg("VBox", [] { return std::make_unique<VBox>(); });
        reg("Grid", [] { return std::make_unique<GridContainer>(); });
        reg("Margin", [] { return std::make_unique<MarginContainer>(); });
        reg("Center", [] { return std::make_unique<CenterContainer>(); });
        reg("PanelContainer", [] { return std::make_unique<PanelContainer>(); });
        reg("Panel", [] { return std::make_unique<Panel>(); });
        reg("HSplit", [] { return std::make_unique<HSplit>(); });
        reg("VSplit", [] { return std::make_unique<VSplit>(); });
        reg("Scroll", [] { return std::make_unique<ScrollContainer>(); });
        reg("Tabs", [] { return std::make_unique<TabContainer>(); });
        reg("TabContainer", [] { return std::make_unique<TabContainer>(); });
        reg("Flow", [] { return std::make_unique<FlowContainer>(); });
        reg("Label", [] { return std::make_unique<Label>(); });
        reg("Button", [] { return std::make_unique<Button>(); });
        reg("CheckBox", [] { return std::make_unique<CheckBox>(); });
        reg("Separator", [] { return std::make_unique<Separator>(); });
        reg("HSeparator", [] { return std::make_unique<Separator>(false); });
        reg("VSeparator", [] { return std::make_unique<Separator>(true); });
        reg("ColorRect", [] { return std::make_unique<ColorRect>(); });
        reg("TextureRect", [] { return std::make_unique<TextureRect>(); });
        reg("ProgressBar", [] { return std::make_unique<ProgressBar>(); });
        reg("Slider", [] { return std::make_unique<Slider>(); });
        reg("LineEdit", [] { return std::make_unique<LineEdit>(); });
        reg("SpinBox", [] { return std::make_unique<SpinBox>(); });
        reg("OptionButton", [] { return std::make_unique<OptionButton>(); });
        reg("MenuButton", [] { return std::make_unique<MenuButton>(); });
        reg("Tree", [] { return std::make_unique<Tree>(); });
        reg("ItemList", [] { return std::make_unique<ItemList>(); });
        reg("Viewport", [] { return std::make_unique<ViewportControl>(); });
        reg("Window", [] { return std::make_unique<Window>(); });
        reg("DockRoot", [] { return std::make_unique<DockRoot>(); });
    });
    return f;
}

std::vector<MenuItem> menuItems(const XmlNode& n) {
    std::vector<MenuItem> items;
    for (const auto& c : n.children) {
        if (c.name == "Separator") {
            items.push_back(MenuItem::sep());
            continue;
        }
        MenuItem m;
        m.label = std::string(c.get("label", c.get("title")));
        m.action = std::string(c.get("action"));
        m.icon = std::string(c.get("icon"));
        m.shortcut = std::string(c.get("shortcut"));
        if (c.name == "Menu") m.submenu = menuItems(c);
        items.push_back(std::move(m));
    }
    return items;
}

}  // namespace

void registerControlType(const std::string& name, ControlFactory fn) { factories()[name] = std::move(fn); }

std::unique_ptr<Control> createControl(const std::string& name) {
    auto& f = factories();
    auto it = f.find(name);
    return it == f.end() ? nullptr : it->second();
}

std::unique_ptr<Control> LayoutLoader::buildMenuBar(const XmlNode& n) {
    auto bar = std::make_unique<MenuBar>();
    for (const auto& m : n.children)
        if (m.name == "Menu") bar->addMenu(std::string(m.get("title")), menuItems(m));
    return bar;
}

void LayoutLoader::applyAttributes(Control& c, const XmlNode& n) {
    Context* ctx = &ctx_;
    for (const auto& [key, value] : n.attributes) {
        if (key == "area" || key == "src") continue;
        if (key.starts_with("on-")) {
            std::string action = value;
            std::string signal = key.substr(3);
            for (char& ch : signal)
                if (ch == '-') ch = '_';
            if (!c.connectSignal(signal, [ctx, action] { ctx->actions().run(action); }))
                errors_.push_back("line " + std::to_string(n.line) + ": <" + n.name + "> has no signal " + signal);
            continue;
        }
        if (value.size() >= 2 && value.front() == '{' && value.back() == '}') {
            // Bound property: re-read every frame, applied when it changes.
            std::string expr = value.substr(1, value.size() - 2), k = key;
            auto last = std::make_shared<std::string>("\x01");
            Control* self = &c;
            c.binders.push_back([ctx, expr, k, last, self] {
                auto v = ctx->bindings().get(expr, &ctx->actions());
                if (v && *v != *last) {
                    *last = *v;
                    self->setProperty(k, *v);
                }
            });
            continue;
        }
        if (!c.setProperty(key, value))
            errors_.push_back("line " + std::to_string(n.line) + ": <" + n.name + "> has no property " + key);
    }
    // Buttons with an action follow its state and describe it in their tooltip.
    if (auto* b = dynamic_cast<Button*>(&c); b && !b->action.empty() && !dynamic_cast<OptionButton*>(&c)) {
        std::string action = b->action;
        bool hadTip = !b->tooltip.empty();
        b->binders.push_back([ctx, action, b, hadTip] {
            ActionRegistry& reg = ctx->actions();
            const Action* a = reg.find(action);
            if (!a) return;
            b->disabled = a->enabled && !a->enabled();
            if (a->checked) b->pressed = a->checked();
            if (!hadTip) b->tooltip = a->label + (a->shortcut.empty() ? "" : " (" + a->shortcut + ")");
        });
    }
}

std::unique_ptr<Control> LayoutLoader::build(const XmlNode& n, const std::string& dir) {
    if (n.name == "MenuBar") {
        auto bar = buildMenuBar(n);
        applyAttributes(*bar, XmlNode{n.name, n.attributes, {}, {}, n.line});
        return bar;
    }
    std::unique_ptr<Control> c;
    if (n.name == "ui") c = std::make_unique<Control>();
    else c = createControl(n.name);
    if (!c) {
        errors_.push_back("line " + std::to_string(n.line) + ": unknown element <" + n.name + ">");
        return nullptr;
    }
    if (n.name == "ui") c->setAnchorPreset("full");
    applyAttributes(*c, n);
    if (auto* mb = dynamic_cast<MenuButton*>(c.get())) mb->items = menuItems(n);
    if (!n.text.empty())
        if (dynamic_cast<Label*>(c.get()) || dynamic_cast<Button*>(c.get())) c->setProperty("text", n.text);
    auto* dock = dynamic_cast<DockRoot*>(c.get());
    for (const auto& child : n.children) {
        if (dynamic_cast<MenuButton*>(c.get())) break;  // its children were menu items
        if (child.name == "include") {
            std::string path = std::string(child.get("src"));
            std::string full = dir.empty() ? path : dir + "/" + path;
            std::optional<std::string> text;
            if (readFile) text = readFile(full);
            else if (auto bytes = fs::readOsFile(full)) text = std::string(bytes->begin(), bytes->end());
            if (!text) {
                errors_.push_back("cannot read included " + full);
                continue;
            }
            if (++depth_ > 16) {
                errors_.push_back("includes nested too deep at " + full);
                --depth_;
                continue;
            }
            auto doc = parseXml(*text);
            if (!doc) {
                errors_.push_back(full + ": " + doc.error());
                --depth_;
                continue;
            }
            auto sub = build(*doc, std::string(dirName(full)));
            --depth_;
            if (!sub) continue;
            // Attributes on <include> apply to the included root (id, title, area...).
            for (const auto& [k, v] : child.attributes)
                if (k != "src" && k != "area") sub->setProperty(k, v);
            if (dock) dock->addPanel(std::move(sub), DockRoot::areaFromName(child.get("area", "center")));
            else c->addChild(std::move(sub));
            continue;
        }
        if (dock && child.name == "Dock") {
            // A dock panel: a VBox page holding the children.
            auto page = std::make_unique<VBox>();
            page->separation = 0;
            XmlNode attrs{"Dock", child.attributes, {}, {}, child.line};
            std::erase_if(attrs.attributes, [](const auto& kv) { return kv.first == "area"; });
            applyAttributes(*page, attrs);
            for (const auto& gc : child.children) {
                if (gc.name == "include") {
                    // Included panels fill the page.
                    XmlNode holder{"VBox", {}, {gc}, {}, gc.line};
                    if (auto sub = build(holder, dir))
                        while (sub->childCount()) {
                            auto ch = sub->removeChild(sub->child(0));
                            ch->sizeFlagsV |= SizeFlags::Expand;
                            page->addChild(std::move(ch));
                        }
                    continue;
                }
                if (auto sub = build(gc, dir)) page->addChild(std::move(sub));
            }
            // A panel with one child: it fills the page.
            if (page->childCount() == 1) page->child(0)->sizeFlagsV |= SizeFlags::Expand;
            dock->addPanel(std::move(page), DockRoot::areaFromName(child.get("area", "center")));
            continue;
        }
        if (auto sub = build(child, dir)) c->addChild(std::move(sub));
    }
    return c;
}

std::unique_ptr<Control> LayoutLoader::loadText(std::string_view xml, const std::string& dir) {
    auto doc = parseXml(xml);
    if (!doc) {
        errors_.push_back(doc.error());
        return nullptr;
    }
    return build(*doc, dir);
}

std::unique_ptr<Control> LayoutLoader::loadFile(const std::string& path) {
    std::optional<std::string> text;
    if (readFile) text = readFile(path);
    else if (auto bytes = fs::readOsFile(path)) text = std::string(bytes->begin(), bytes->end());
    if (!text) {
        errors_.push_back("cannot read " + path);
        return nullptr;
    }
    return loadText(*text, std::string(dirName(path)));
}

}  // namespace ose::ui
