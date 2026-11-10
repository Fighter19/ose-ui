// ose-ui demo: a small editor-like window in the classic or the dark theme.
//
//   ose_ui_demo [--dark] [--frames N --screenshot out.ppm]
//
// With --frames the demo renders N frames, optionally writes the last one, and exits (used to
// test the platform layer headless with SDL_VIDEO_DRIVER=offscreen).

#include <GLES2/gl2.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform/sdl3/ui_sdl3.h"
#include "ui/loader.h"
#include "ui/lists.h"
#include "ui/widgets.h"

using namespace ose::ui;

static const char* kLayout = R"(<Panel theme-type="Background" anchors="full"><VBox anchors="full" separation="0">
  <MenuBar>
    <Menu title="File"><Item action="file.quit"/></Menu>
    <Menu title="Edit"/><Menu title="View"/><Menu title="Help"/>
  </MenuBar>
  <PanelContainer theme-type="Toolbar"><HBox>
    <Button theme-type="ToolButton" icon="folder-open" tooltip="Open"/>
    <Button theme-type="ToolButton" icon="save" tooltip="Save"/>
    <Button theme-type="ToolButton" icon="pencil" toggle="1" pressed="1" tooltip="Draw"/>
    <Button theme-type="ToolButton" icon="eyedropper" toggle="1" tooltip="Pick"/>
    <Control expand="1"/>
    <OptionButton items="Mode 0;Mode 1;Mode 2" selected="0"/>
  </HBox></PanelContainer>
  <HBox expand="1">
    <PanelContainer><VBox>
      <Label text="Palette bank"/>
      <SpinBox value="0" decimals="0"/>
      <CheckBox text="Show grid" pressed="1"/>
      <CheckBox text="Show collision"/>
      <Button text="Import PNG..."/>
    </VBox></PanelContainer>
    <PanelContainer expand="1"><VBox>
      <Label text="Room: test_room" />
      <Tree id="rooms" expand="1"/>
    </VBox></PanelContainer>
  </HBox>
  <PanelContainer theme-type="StatusBar"><HBox><Label id="status" text="Ready"/></HBox></PanelContainer>
</VBox></Panel>)";

int main(int argc, char** argv) {
    bool dark = false;
    int frames = 0;
    const char* screenshot = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--dark") == 0) dark = true;
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) screenshot = argv[++i];
    }

    sdl3::App app;
    sdl3::AppDesc desc;
    desc.title = "ose-ui demo";
    desc.width = 800;
    desc.height = 500;
    desc.theme = dark ? Theme::makeDark(OSE_UI_FONT_DIR) : Theme::makeClassic(OSE_UI_FONT_DIR);
    std::string error;
    if (!app.init(desc, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    app.ui().actions().add({"file.quit", "Quit", "", "Ctrl+Q", "", [&] { app.quit(); }, nullptr, nullptr});
    LayoutLoader loader(app.ui());
    auto root = loader.loadText(kLayout);
    for (const auto& e : loader.errors()) std::fprintf(stderr, "layout: %s\n", e.c_str());
    if (!root) return 1;
    auto* tree = root->findAs<Tree>("rooms");
    auto* area = tree->root()->add("Area: castle", "map");
    area->add("Entrance", "home");
    area->add("Corridor", "home");
    area->add("Tower", "home");
    app.ui().setRoot(std::move(root));

    if (frames > 0) {
        for (int i = 0; i < frames; i++)
            if (!app.frame()) break;
        if (screenshot) {
            GLint vp[4] = {};
            glGetIntegerv(GL_VIEWPORT, vp);
            std::vector<unsigned char> px(size_t(vp[2]) * vp[3] * 4);
            glReadPixels(0, 0, vp[2], vp[3], GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            FILE* f = std::fopen(screenshot, "wb");
            if (!f) return 1;
            std::fprintf(f, "P6\n%d %d\n255\n", vp[2], vp[3]);
            for (int y = vp[3] - 1; y >= 0; y--)
                for (int x = 0; x < vp[2]; x++) std::fwrite(&px[(size_t(y) * vp[2] + x) * 4], 1, 3, f);
            std::fclose(f);
        }
        return 0;
    }
    app.run();
    return 0;
}
