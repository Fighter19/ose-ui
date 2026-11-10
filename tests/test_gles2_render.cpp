// Renders a layout with the GLES 2 renderer on a headless EGL context (Mesa llvmpipe) and
// checks pixels. The picture is written to test-output/ for looking at it.
#include "doctest.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "render/gles2/gles2_renderer.h"
#include "ui/context.h"
#include "ui/loader.h"

using namespace ose;
using namespace ose::ui;

namespace {

struct Headless {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    bool ok = false;

    Headless() {
        setenv("EGL_PLATFORM", "surfaceless", 0);
        setenv("GALLIUM_DRIVER", "llvmpipe", 0);
        auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (getPlatformDisplay) display = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        if (display == EGL_NO_DISPLAY) display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) return;
        eglBindAPI(EGL_OPENGL_ES_API);
        // Surfaceless displays may have no configs; then a context without config is used.
        const EGLint configAttribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
        EGLConfig config = nullptr;
        EGLint count = 0;
        if (!eglChooseConfig(display, configAttribs, &config, 1, &count) || count == 0) config = nullptr;
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        context = eglCreateContext(display, config ? config : EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, contextAttribs);
        if (context == EGL_NO_CONTEXT) return;
        ok = eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_TRUE;
    }
    ~Headless() {
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (display != EGL_NO_DISPLAY) eglTerminate(display);
    }
};

void writePpm(const std::string& path, int w, int h, const std::vector<uint8_t>& rgba) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)  // GL rows start at the bottom
        for (int x = 0; x < w; x++) std::fwrite(&rgba[(size_t(y) * w + x) * 4], 1, 3, f);
    std::fclose(f);
}

std::vector<uint8_t> renderLayout(const Gles2Options& options, const char* name, bool classic = false) {
    const int W = 640, H = 400;
    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    auto theme = classic ? Theme::makeClassic(OSE_UI_FONT_DIR) : Theme::makeDark(OSE_UI_FONT_DIR);
    Context ctx(theme);
    LayoutLoader loader(ctx);
    auto root = loader.loadText(R"(<VBox anchors="full" separation="0">
      <Panel theme-type="Background" anchors="full"/>
      <MenuBar><Menu title="File"/><Menu title="Edit"/><Menu title="View"/></MenuBar>
      <HBox expand="1">
        <VBox><Button text="Tiles"/><Button text="Palettes"/><CheckBox text="Grid" pressed="1"/></VBox>
        <Panel expand="1"/>
      </HBox>
    </VBox>)");
    CHECK(root);
    for (auto& e : loader.errors()) MESSAGE(e);
    ctx.setRoot(std::move(root));
    ctx.resize(W, H);
    Canvas canvas(ctx.atlas());
    ctx.process(0.016f);
    ctx.draw(canvas);

    // A texture of the application: a 2x2 checker drawn into the top right corner.
    auto renderer = createGles2Renderer(options);
    const uint8_t checker[16] = {255, 0, 0, 255, 0, 0, 255, 255, 0, 0, 255, 255, 255, 0, 0, 255};
    TextureHandle image = renderer->createTexture(2, 2, checker, true);
    canvas.image({W - 40.0f, 30, 32, 32}, image);

    glClearColor(1, 0, 1, 1);  // magenta: anything left is a gap in the UI
    glClear(GL_COLOR_BUFFER_BIT);
    renderer->render(canvas, W, H);
    std::vector<uint8_t> px(size_t(W) * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    CHECK(glGetError() == GL_NO_ERROR);

    std::filesystem::create_directories(OSE_UI_TEST_OUTPUT_DIR);
    writePpm(std::string(OSE_UI_TEST_OUTPUT_DIR) + "/" + name + ".ppm", W, H, px);

    auto at = [&](int x, int y) { return &px[(size_t(H - 1 - y) * W + x) * 4]; };
    if (classic) {
        // Classic theme: grey menu bar, a raised button with a white top edge.
        const uint8_t* m = at(3, 3);
        CHECK(m[0] == m[1]);
        CHECK(m[0] > 150);
    }
    // The menu bar is drawn at the top left (not magenta).
    const uint8_t* menu = at(3, 3);
    CHECK_FALSE((menu[0] == 255 && menu[1] == 0 && menu[2] == 255));
    // Text was drawn: some pixels in the menu bar differ from its background.
    int differing = 0;
    for (int x = 0; x < 120; x++)
        for (int y = 2; y < 20; y++) {
            const uint8_t* p = at(x, y);
            if (p[0] != menu[0] || p[1] != menu[1] || p[2] != menu[2]) differing++;
        }
    CHECK(differing > 20);
    // The application's texture shows up with its colours, sharp (pixelated).
    const uint8_t* img = at(W - 40 + 4, 30 + 4);
    CHECK(img[0] == 255);
    CHECK(img[2] == 0);

    renderer->destroyTexture(image);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return px;
}

}  // namespace

TEST_CASE("gles2: a layout renders into a framebuffer, with 32- and 16-bit indices alike") {
    Headless gl;
    if (!gl.ok) {
        MESSAGE("no headless EGL context; skipped");
        return;
    }
    auto a = renderLayout({}, "gles2_layout");
    auto b = renderLayout({.force16BitIndices = true}, "gles2_layout_16bit");
    CHECK(a == b);
    renderLayout({}, "gles2_layout_classic", true);
}
