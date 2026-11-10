// 2D drawing for the UI: everything becomes textured triangles batched by texture and clip
// rectangle, drawn by a Renderer (ui/renderer.h; GLES 2 / WebGL 1 back end in render/gles2).
#pragma once
#include <string_view>
#include <vector>

#include "ui/core.h"
#include "ui/font.h"
#include "ui/theme.h"

namespace ose::ui {

enum class Align { Begin, Center, End };

class Canvas {
public:
    struct Vertex {
        float x, y, u, v;
        uint32_t color;  // RGBA8
    };
    struct Command {
        TextureHandle texture;  // invalid = the glyph atlas
        Rect clip;
        uint32_t firstIndex = 0, indexCount = 0;
    };

    explicit Canvas(GlyphAtlas& atlas) : atlas_(atlas) {}
    void clear(float width, float height);
    float width() const { return width_; }
    float height() const { return height_; }

    void pushClip(Rect r);  // intersected with the current clip
    void popClip();
    Rect clip() const { return clips_.empty() ? Rect{0, 0, width_, height_} : clips_.back(); }

    void rect(const Rect& r, Color c);
    void roundRect(const Rect& r, Color c, float radius);
    void border(const Rect& r, Color c, float width, float radius = 0);
    void styleBox(const Rect& r, const StyleBox& s);
    void line(Vec2 a, Vec2 b, Color c, float width = 1);
    void triangle(Vec2 a, Vec2 b, Color ca, Vec2 c);  // one colour
    void polygon(const std::vector<Vec2>& convex, Color c);
    void image(const Rect& r, TextureHandle tex, Rect uv = {0, 0, 1, 1}, Color tint = {1, 1, 1, 1});
    // Text on one line; `pos` is the top left of the line box. Returns the pen x after it.
    float text(Vec2 pos, std::string_view s, const Font& font, float px, Color c);
    // Text aligned inside a rectangle (vertically centred), clipped with "..." if too long.
    void textIn(const Rect& r, std::string_view s, const Font& font, float px, Color c, Align h = Align::Begin, bool ellipsis = true);
    // A Font Awesome icon centred in `r`.
    void icon(const Rect& r, uint32_t codepoint, const Font& iconFont, float px, Color c);

    const std::vector<Vertex>& vertices() const { return vertices_; }
    const std::vector<uint32_t>& indices() const { return indices_; }
    const std::vector<Command>& commands() const { return commands_; }
    GlyphAtlas& atlas() { return atlas_; }

private:
    Command& command(TextureHandle tex);
    uint32_t vertex(float x, float y, float u, float v, uint32_t color);
    void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color, TextureHandle tex = {});
    static void roundedPath(const Rect& r, float radius, std::vector<Vec2>& out);
    GlyphAtlas& atlas_;
    float width_ = 0, height_ = 0;
    std::vector<Rect> clips_;
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<Command> commands_;
    std::vector<Vec2> scratch_, scratch2_;
};

uint32_t packColor(Color c);


}  // namespace ose::ui
