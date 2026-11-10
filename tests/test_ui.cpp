#include "doctest.h"

#include "ui/context.h"
#include "ui/dock.h"
#include "ui/lists.h"
#include "ui/loader.h"
#include "ui/menus.h"
#include "ui/xml.h"

using namespace ose;
using namespace ose::ui;

namespace {

std::shared_ptr<Theme> theme() {
    static auto t = Theme::makeDark(OSE_UI_FONT_DIR);
    return t;
}

struct Ui {
    Context ctx{theme()};
    Canvas canvas{ctx.atlas()};
    explicit Ui(std::unique_ptr<Control> root, float w = 800, float h = 600) {
        ctx.setRoot(std::move(root));
        ctx.resize(w, h);
        frame();
    }
    void frame() {
        ctx.process(1.0f / 60);
        ctx.draw(canvas);
    }
    void click(Vec2 p, MouseButton b = MouseButton::Left, int clicks = 1) {
        InputEvent e;
        e.type = InputEvent::Type::MouseMove;
        e.pos = p;
        ctx.dispatch(e);
        e.type = InputEvent::Type::MouseDown;
        e.button = b;
        e.clicks = clicks;
        ctx.dispatch(e);
        e.type = InputEvent::Type::MouseUp;
        ctx.dispatch(e);
        frame();
    }
    void key(std::string k, int mods = 0) {
        InputEvent e;
        e.type = InputEvent::Type::KeyDown;
        e.key = std::move(k);
        e.mods = mods;
        ctx.dispatch(e);
        e.type = InputEvent::Type::KeyUp;
        ctx.dispatch(e);
    }
    void type(std::string t) {
        InputEvent e;
        e.type = InputEvent::Type::Text;
        e.text = std::move(t);
        ctx.dispatch(e);
    }
    void move(Vec2 p, bool held = false) {
        InputEvent e;
        e.type = InputEvent::Type::MouseMove;
        e.pos = p;
        ctx.dispatch(e);
        (void)held;
    }
    void press(Vec2 p) {
        move(p);
        InputEvent e;
        e.type = InputEvent::Type::MouseDown;
        e.button = MouseButton::Left;
        e.pos = p;
        ctx.dispatch(e);
    }
    void release(Vec2 p) {
        InputEvent e;
        e.type = InputEvent::Type::MouseUp;
        e.button = MouseButton::Left;
        e.pos = p;
        ctx.dispatch(e);
        frame();
    }
};

}  // namespace

TEST_CASE("ui: xml parser") {
    auto doc = parseXml(R"(<?xml version="1.0"?>
<!-- comment -->
<ui a="1" b='two &amp; three'>
  <Label text="&lt;hi&gt; &#x263A;"/>
  <Button>Click <![CDATA[<me>]]></Button>
</ui>)");
    REQUIRE_MESSAGE(doc, doc.error());
    CHECK(doc->name == "ui");
    CHECK(doc->get("b") == "two & three");
    REQUIRE(doc->children.size() == 2);
    CHECK(doc->children[0].get("text") == "<hi> \xE2\x98\xBA");
    CHECK(doc->children[1].text == "Click <me>");
    CHECK_FALSE(parseXml("<a><b></a>"));
    CHECK_FALSE(parseXml("<a x=1/>"));
    CHECK_FALSE(parseXml("<a/><b/>"));
}

TEST_CASE("ui: fonts and text measurement") {
    auto th = theme();
    REQUIRE(th->font);
    REQUIRE(th->iconFont);
    CHECK(th->font->measure("Hello", 13) > 20);
    CHECK(th->font->measure("Hello", 26) == doctest::Approx(th->font->measure("Hello", 13) * 2).epsilon(0.05));
    CHECK(th->font->hitTest("Hello", 13, 0) == 0);
    CHECK(th->font->hitTest("Hello", 13, 1000) == 5);
    CHECK(th->iconFont->hasGlyph(iconCodepoint("play")));
    GlyphAtlas atlas;
    const auto& g = atlas.glyph(*th->font, 'A', 13);
    CHECK_FALSE(g.empty);
    CHECK(g.advance > 4);
}

TEST_CASE("ui: box layout with expand and stretch") {
    auto root = std::make_unique<HBox>();
    root->setAnchorPreset("full");
    root->separation = 10;
    auto* a = root->add<ColorRect>();
    a->customMinSize = {100, 20};
    auto* b = root->add<ColorRect>();
    b->sizeFlagsH = SizeFlags::Fill | SizeFlags::Expand;
    auto* c = root->add<ColorRect>();
    c->sizeFlagsH = SizeFlags::Fill | SizeFlags::Expand;
    c->stretchRatio = 3;
    auto* d = root->add<ColorRect>();
    d->customMinSize = {20, 20};
    d->sizeFlagsV = SizeFlags::ShrinkCenter;
    Ui ui(std::move(root), 830, 100);
    CHECK(a->rect().w == 100);
    // 830 - 100 - 20 - 3 * 10 = 680 shared 1:3
    CHECK(b->rect().w == doctest::Approx(170));
    CHECK(c->rect().w == doctest::Approx(510));
    CHECK(d->rect().x == doctest::Approx(810));
    CHECK(d->rect().y == doctest::Approx(40));
    CHECK(a->rect().h == 100);
}

TEST_CASE("ui: grid, margins and anchors") {
    auto root = std::make_unique<Control>();
    root->setAnchorPreset("full");
    auto* m = root->add<MarginContainer>();
    m->setAnchorPreset("full");
    for (float& v : m->margins) v = 10;
    auto* g = m->add<GridContainer>();
    g->columns = 2;
    g->hSeparation = g->vSeparation = 0;
    auto* x = g->add<Label>("x");
    auto* y = g->add<ColorRect>();
    y->sizeFlagsH = SizeFlags::Fill | SizeFlags::Expand;
    auto* corner = root->add<ColorRect>();
    corner->setAnchorPreset("bottom-right");
    corner->customMinSize = {30, 20};
    Ui ui(std::move(root), 400, 300);
    CHECK(x->rect().x == 10);
    CHECK(y->rect().right() == doctest::Approx(390));
    CHECK(corner->rect().x == doctest::Approx(370));
    CHECK(corner->rect().y == doctest::Approx(280));
}

TEST_CASE("ui: buttons run actions, shortcuts work, bindings update") {
    int saved = 0;
    bool canSave = true;
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    Ui ui(std::move(root));
    ui.ctx.actions().add({"file.save", "Save", "save", "Ctrl+S", "", [&] { ++saved; }, [&] { return canSave; }, nullptr});
    std::string status = "ready";
    ui.ctx.bindings().set("status", [&] { return status; });
    LayoutLoader loader(ui.ctx);
    auto layout = loader.loadText(R"(<VBox anchors="full">
        <Button id="save" action="file.save" text="Save"/>
        <Label id="status" text="{status}"/>
        <Button id="dis" text="x" disabled="{!file.save.enabled}"/>
    </VBox>)");
    REQUIRE(layout);
    CHECK(loader.errors().empty());
    ui.ctx.setRoot(std::move(layout));
    ui.frame();
    auto* btn = ui.ctx.root()->findAs<Button>("save");
    REQUIRE(btn);
    ui.click(btn->rect().center());
    CHECK(saved == 1);
    ui.key("s", Mod::Ctrl);
    CHECK(saved == 2);
    canSave = false;
    ui.frame();
    CHECK(btn->disabled);
    CHECK(ui.ctx.root()->findAs<Button>("dis")->disabled);
    ui.click(btn->rect().center());
    CHECK(saved == 2);
    status = "busy";
    ui.frame();
    CHECK(ui.ctx.root()->findAs<Label>("status")->text == "busy");
    CHECK(btn->tooltip == "Save (Ctrl+S)");
}

TEST_CASE("ui: line edit editing, selection and submit") {
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    auto* le = root->add<LineEdit>();
    Ui ui(std::move(root));
    std::string submitted;
    le->submitted.connect([&](const std::string& s) { submitted = s; });
    std::string clip;
    ui.ctx.setClipboard = [&](const std::string& s) { clip = s; };
    ui.ctx.getClipboard = [&] { return clip; };
    ui.click(le->rect().center());
    CHECK(le->hasFocus());
    ui.type("hello");
    ui.type(" world");
    CHECK(le->text() == "hello world");
    ui.key("backspace");
    ui.key("home");
    ui.key("delete");
    CHECK(le->text() == "ello worl");
    ui.key("a", Mod::Ctrl);
    ui.key("c", Mod::Ctrl);
    CHECK(clip == "ello worl");
    ui.key("end");
    ui.key("v", Mod::Ctrl);
    CHECK(le->text() == "ello worlello worl");
    ui.key("enter");
    CHECK(submitted == "ello worlello worl");
}

TEST_CASE("ui: spin box typing and scrubbing") {
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    auto* sb = root->add<SpinBox>();
    sb->step = 1;
    sb->decimals = 0;
    Ui ui(std::move(root));
    sb->setValue(10);
    double last = 0;
    sb->valueChanged.connect([&](double v) { last = v; });
    Vec2 c = sb->rect().center();
    ui.press(c);
    ui.move(c + Vec2{40, 0});
    ui.release(c + Vec2{40, 0});
    CHECK(sb->value() == doctest::Approx(20));
    CHECK(last == doctest::Approx(20));
    ui.click(c);  // a click without dragging edits
    CHECK(sb->hasFocus());
    ui.type("42");
    ui.key("enter");
    CHECK(sb->value() == doctest::Approx(42));
}

TEST_CASE("ui: menus open, run items and close") {
    int newCount = 0, other = 0;
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    Ui ui(std::move(root));
    ui.ctx.actions().add({"file.new", "New", "file", "Ctrl+N", "", [&] { ++newCount; }, nullptr, nullptr});
    LayoutLoader loader(ui.ctx);
    auto layout = loader.loadText(R"(<VBox anchors="full">
        <MenuBar id="bar"><Menu title="File"><Item action="file.new"/><Separator/>
            <Menu title="More"><Item label="Deep" action="x.deep"/></Menu></Menu></MenuBar>
        <Control expand="1"/>
    </VBox>)");
    REQUIRE(layout);
    ui.ctx.actions().add({"x.deep", "Deep", "", "", "", [&] { ++other; }, nullptr, nullptr});
    ui.ctx.setRoot(std::move(layout));
    ui.frame();
    auto* bar = ui.ctx.root()->find("bar");
    REQUIRE(bar);
    ui.click({bar->rect().x + 15, bar->rect().center().y});
    REQUIRE(ui.ctx.popupCount() == 1);
    // The first item is "New".
    Control* menu = nullptr;
    Rect r = bar->rect();
    (void)r;
    ui.frame();
    // Find the popup through the hovered control after moving into it.
    ui.move({bar->rect().x + 30, bar->rect().bottom() + 12});
    menu = ui.ctx.hovered();
    REQUIRE(menu);
    CHECK(std::string(menu->typeName()) == "PopupMenu");
    ui.click({bar->rect().x + 30, bar->rect().bottom() + 12});
    CHECK(newCount == 1);
    CHECK(ui.ctx.popupCount() == 0);
    // Submenu on hover, then run its item.
    ui.click({bar->rect().x + 15, bar->rect().center().y});
    ui.frame();
    Rect moreItem{bar->rect().x + 30, bar->rect().bottom() + 4 + 24 + 7 + 12, 1, 1};
    ui.move(moreItem.pos());
    ui.frame();
    CHECK(ui.ctx.popupCount() == 2);
    // Click outside closes everything.
    ui.click({700, 500});
    CHECK(ui.ctx.popupCount() == 0);
    CHECK(other == 0);
}

TEST_CASE("ui: option button selects from its list") {
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    auto* ob = root->add<OptionButton>();
    ob->setItems({"Wireframe", "Textured", "Engine"});
    Ui ui(std::move(root));
    int sel = -1;
    ob->itemSelected.connect([&](int i) { sel = i; });
    ui.click(ob->rect().center());
    REQUIRE(ui.ctx.popupCount() == 1);
    float ih = theme()->constant("PopupMenu", "item_height");
    ui.move({ob->rect().x + 20, ob->rect().bottom() + 4 + ih * 2 + ih * 0.5f});
    ui.click({ob->rect().x + 20, ob->rect().bottom() + 4 + ih * 2 + ih * 0.5f});
    CHECK(sel == 2);
    CHECK(ob->selectedText() == "Engine");
}

TEST_CASE("ui: tree selection, expansion and toggles") {
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    auto* tree = root->add<Tree>();
    tree->sizeFlagsV = SizeFlags::Fill | SizeFlags::Expand;
    auto* world = tree->root()->add("World", "globe");
    for (int i = 0; i < 5; ++i) {
        auto* b = world->add("Brush " + std::to_string(i), "cube");
        b->toggleIcon = "eye";
        b->toggleIconOff = "eye-slash";
        b->key = uint64_t(100 + i);
    }
    Ui ui(std::move(root));
    int changes = 0, toggles = 0;
    tree->selectionChanged.connect([&] { ++changes; });
    tree->toggleClicked.connect([&](TreeItem*) { ++toggles; });
    Rect r1 = tree->itemRect(tree->findByKey(101));
    ui.click({r1.x + 60, r1.center().y});
    REQUIRE(tree->selection().size() == 1);
    CHECK(tree->selection()[0]->key == 101);
    Rect r3 = tree->itemRect(tree->findByKey(103));
    InputEvent shiftClick;
    ui.move({r3.x + 60, r3.center().y});
    shiftClick.type = InputEvent::Type::MouseDown;
    shiftClick.button = MouseButton::Left;
    shiftClick.pos = {r3.x + 60, r3.center().y};
    shiftClick.mods = Mod::Shift;
    ui.ctx.dispatch(shiftClick);
    CHECK(tree->selection().size() == 3);
    ui.click({r3.right() - 10, r3.center().y});
    CHECK(toggles == 1);
    CHECK_FALSE(tree->findByKey(103)->toggleOn);
    Rect rw = tree->itemRect(world);
    ui.click({rw.x + 8, rw.center().y});  // the arrow folds it
    CHECK_FALSE(world->expanded);
    CHECK(tree->itemRect(tree->findByKey(101)).empty());
    CHECK(changes >= 2);
}

TEST_CASE("ui: drag and drop from a list onto a viewport") {
    auto root = std::make_unique<HBox>();
    root->setAnchorPreset("full");
    auto* list = root->add<ItemList>();
    list->customMinSize = {200, 0};
    list->items.push_back({"models/box.mdl", "cube", {}, {}, {}, std::string("models/box.mdl")});
    list->dragProvider = [&](int i, DragPayload& p) {
        p.type = "asset";
        p.data = std::any_cast<std::string>(list->items[size_t(i)].data);
        p.label = list->items[size_t(i)].text;
        return true;
    };
    auto* vp = root->add<ViewportControl>();
    vp->sizeFlagsH = SizeFlags::Fill | SizeFlags::Expand;
    std::string dropped;
    vp->onCanDrop = [](Vec2, const DragPayload& p) { return p.type == "asset"; };
    vp->onDrop = [&](Vec2, const DragPayload& p) { dropped = std::any_cast<std::string>(p.data); };
    Ui ui(std::move(root));
    Vec2 start = list->itemRect(0).center();
    ui.press(start);
    ui.move(start + Vec2{20, 0});
    CHECK(ui.ctx.dragging());
    Vec2 target = vp->rect().center();
    ui.move(target);
    ui.release(target);
    CHECK(dropped == "models/box.mdl");
    CHECK_FALSE(ui.ctx.dragging());
}

TEST_CASE("ui: dock layout from xml, tab drag between areas, saved layout") {
    auto root = std::make_unique<Control>();
    Ui ui(std::move(root), 1200, 800);
    LayoutLoader loader(ui.ctx);
    auto layout = loader.loadText(R"(<DockRoot anchors="full">
        <Dock id="tools" area="left" title="Tools"><Label text="t"/></Dock>
        <Dock id="view" area="center" title="3D"><Viewport/></Dock>
        <Dock id="props" area="right" title="Properties"><Label text="p"/></Dock>
        <Dock id="outliner" area="right" title="Outliner"><Label text="o"/></Dock>
        <Dock id="assets" area="bottom" title="Assets"><Label text="a"/></Dock>
    </DockRoot>)");
    REQUIRE(layout);
    CHECK(loader.errors().empty());
    auto* dock = dynamic_cast<DockRoot*>(layout.get());
    REQUIRE(dock);
    ui.ctx.setRoot(std::move(layout));
    ui.frame();
    CHECK(dock->area(DockRoot::Right)->childCount() == 2);
    CHECK(dock->panel("view")->rect().w > 400);
    // Drag the "Outliner" tab to the left area's tab bar.
    TabContainer* right = dock->area(DockRoot::Right);
    TabContainer* left = dock->area(DockRoot::Left);
    float tabY = right->rect().y + right->tabBarHeight() * 0.5f;
    float th = theme()->font->measure("Properties", 13) + 20;
    Vec2 tab{right->rect().x + 2 + th + 1 + 10, tabY};
    ui.press(tab);
    ui.move(tab + Vec2{0, 10});
    REQUIRE(ui.ctx.dragging());
    Vec2 dropAt{left->rect().x + 100, left->rect().y + left->tabBarHeight() * 0.5f};
    ui.move(dropAt);
    ui.release(dropAt);
    CHECK(left->childCount() == 2);
    CHECK(right->childCount() == 1);
    std::string saved = dock->saveLayout();
    CHECK(saved.find("left=tools,outliner") != std::string::npos);
    // Restore a layout: everything back to the right.
    dock->loadLayout("left=tools;right=props,outliner;bottom=assets;center=view;sizes=200,320,180");
    CHECK(right->childCount() == 2);
    CHECK(left->childCount() == 1);
}

TEST_CASE("ui: canvas produces batched geometry") {
    auto root = std::make_unique<VBox>();
    root->setAnchorPreset("full");
    root->add<Label>("Hello");
    root->add<Button>("Press");
    Ui ui(std::move(root));
    CHECK(ui.canvas.vertices().size() > 20);
    CHECK_FALSE(ui.canvas.commands().empty());
    for (const auto& c : ui.canvas.commands()) CHECK(c.firstIndex + c.indexCount <= ui.canvas.indices().size());
}
