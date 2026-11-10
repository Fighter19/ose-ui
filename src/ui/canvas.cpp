#include "ui/canvas.h"

#include <cmath>


namespace ose::ui {

uint32_t packColor(Color c) {
    auto q = [](float v) { return uint32_t(clampv(int(v * 255.0f + 0.5f), 0, 255)); };
    return q(c.x) | (q(c.y) << 8) | (q(c.z) << 16) | (q(c.w) << 24);
}

void Canvas::clear(float width, float height) {
    width_ = width;
    height_ = height;
    clips_.clear();
    vertices_.clear();
    indices_.clear();
    commands_.clear();
}

void Canvas::pushClip(Rect r) { clips_.push_back(clip().intersect(r)); }
void Canvas::popClip() {
    if (!clips_.empty()) clips_.pop_back();
}

Canvas::Command& Canvas::command(TextureHandle tex) {
    Rect c = clip();
    if (commands_.empty() || !(commands_.back().texture == tex) || !(commands_.back().clip == c)) {
        Command cmd;
        cmd.texture = tex;
        cmd.clip = c;
        cmd.firstIndex = uint32_t(indices_.size());
        commands_.push_back(cmd);
    }
    return commands_.back();
}

uint32_t Canvas::vertex(float x, float y, float u, float v, uint32_t color) {
    vertices_.push_back({x, y, u, v, color});
    return uint32_t(vertices_.size() - 1);
}

void Canvas::quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color, TextureHandle tex) {
    Rect c = clip();
    if (x1 <= c.x || y1 <= c.y || x0 >= c.right() || y0 >= c.bottom() || c.empty()) return;
    Command& cmd = command(tex);
    uint32_t a = vertex(x0, y0, u0, v0, color), b = vertex(x1, y0, u1, v0, color), d = vertex(x1, y1, u1, v1, color),
             e = vertex(x0, y1, u0, v1, color);
    indices_.insert(indices_.end(), {a, b, d, a, d, e});
    cmd.indexCount += 6;
}

void Canvas::rect(const Rect& r, Color c) {
    if (c.w <= 0 || r.empty()) return;
    Vec2 w = atlas_.whiteUV();
    quad(r.x, r.y, r.right(), r.bottom(), w.x, w.y, w.x, w.y, packColor(c));
}

void Canvas::roundedPath(const Rect& r, float radius, std::vector<Vec2>& out) {
    out.clear();
    radius = std::min(radius, std::min(r.w, r.h) * 0.5f);
    if (radius <= 0.5f) {
        out = {{r.x, r.y}, {r.right(), r.y}, {r.right(), r.bottom()}, {r.x, r.bottom()}};
        return;
    }
    int seg = clampv(int(radius * 0.75f), 2, 8);
    struct Corner {
        float cx, cy, a0;
    } corners[4] = {{r.right() - radius, r.y + radius, -kPi * 0.5f},
                    {r.right() - radius, r.bottom() - radius, 0},
                    {r.x + radius, r.bottom() - radius, kPi * 0.5f},
                    {r.x + radius, r.y + radius, kPi}};
    for (auto& c : corners)
        for (int i = 0; i <= seg; ++i) {
            float a = c.a0 + (kPi * 0.5f) * float(i) / float(seg);
            out.push_back({c.cx + std::cos(a) * radius, c.cy + std::sin(a) * radius});
        }
}

void Canvas::polygon(const std::vector<Vec2>& pts, Color c) {
    if (pts.size() < 3 || c.w <= 0) return;
    Command& cmd = command({});
    Vec2 w = atlas_.whiteUV();
    uint32_t col = packColor(c);
    uint32_t first = uint32_t(vertices_.size());
    for (const auto& p : pts) vertex(p.x, p.y, w.x, w.y, col);
    for (uint32_t i = 1; i + 1 < pts.size(); ++i) indices_.insert(indices_.end(), {first, first + i, first + i + 1});
    cmd.indexCount += uint32_t(pts.size() - 2) * 3;
}

void Canvas::roundRect(const Rect& r, Color c, float radius) {
    if (c.w <= 0 || r.empty()) return;
    if (radius <= 0.5f) return rect(r, c);
    roundedPath(r, radius, scratch_);
    polygon(scratch_, c);
}

void Canvas::border(const Rect& r, Color c, float width, float radius) {
    if (c.w <= 0 || width <= 0 || r.empty()) return;
    if (radius <= 0.5f) {
        rect({r.x, r.y, r.w, width}, c);
        rect({r.x, r.bottom() - width, r.w, width}, c);
        rect({r.x, r.y + width, width, r.h - 2 * width}, c);
        rect({r.right() - width, r.y + width, width, r.h - 2 * width}, c);
        return;
    }
    roundedPath(r, radius, scratch_);
    roundedPath(r.shrink(width), std::max(0.0f, radius - width), scratch2_);
    if (scratch_.size() != scratch2_.size()) {
        // Inner radius collapsed: fall back to straight edges.
        return border(r, c, width, 0);
    }
    Command& cmd = command({});
    Vec2 w = atlas_.whiteUV();
    uint32_t col = packColor(c);
    uint32_t first = uint32_t(vertices_.size());
    for (size_t i = 0; i < scratch_.size(); ++i) {
        vertex(scratch_[i].x, scratch_[i].y, w.x, w.y, col);
        vertex(scratch2_[i].x, scratch2_[i].y, w.x, w.y, col);
    }
    uint32_t n = uint32_t(scratch_.size());
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t a = first + i * 2, b = a + 1, c2 = first + ((i + 1) % n) * 2, d = c2 + 1;
        indices_.insert(indices_.end(), {a, c2, b, b, c2, d});
    }
    cmd.indexCount += n * 6;
}

void Canvas::styleBox(const Rect& r, const StyleBox& s) {
    if (s.bg.w > 0) roundRect(r, s.bg, s.radius);
    if (s.bevel != StyleBox::Bevel::None) {
        bool raised = s.bevel == StyleBox::Bevel::Raised;
        Color outerTL = raised ? s.bevelHighlight : s.bevelShade, outerBR = raised ? s.bevelShadow : s.bevelHighlight;
        Color innerTL = raised ? s.bevelLight : s.bevelShadow, innerBR = raised ? s.bevelShade : s.bevelLight;
        auto ring = [&](Rect q, Color tl, Color br) {
            rect({q.x, q.y, q.w, 1}, tl);
            rect({q.x, q.y, 1, q.h}, tl);
            rect({q.x, q.bottom() - 1, q.w, 1}, br);
            rect({q.right() - 1, q.y, 1, q.h}, br);
        };
        ring(r, outerTL, outerBR);
        if (r.w > 4 && r.h > 4) ring({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, innerTL, innerBR);
        return;
    }
    float bw = s.borderWidth[0];
    bool uniform = s.borderWidth[1] == bw && s.borderWidth[2] == bw && s.borderWidth[3] == bw;
    if (s.border.w > 0) {
        if (uniform) {
            border(r, s.border, bw, s.radius);
        } else {
            rect({r.x, r.y, s.borderWidth[0], r.h}, s.border);
            rect({r.x, r.y, r.w, s.borderWidth[1]}, s.border);
            rect({r.right() - s.borderWidth[2], r.y, s.borderWidth[2], r.h}, s.border);
            rect({r.x, r.bottom() - s.borderWidth[3], r.w, s.borderWidth[3]}, s.border);
        }
    }
}

void Canvas::line(Vec2 a, Vec2 b, Color c, float width) {
    Vec2 d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-3f || c.w <= 0) return;
    Vec2 n{-d.y / len * width * 0.5f, d.x / len * width * 0.5f};
    polygon({a + n, b + n, b - n, a - n}, c);
}

void Canvas::triangle(Vec2 a, Vec2 b, Color col, Vec2 c) { polygon({a, b, c}, col); }

void Canvas::image(const Rect& r, TextureHandle tex, Rect uv, Color tint) {
    if (r.empty()) return;
    quad(r.x, r.y, r.right(), r.bottom(), uv.x, uv.y, uv.right(), uv.bottom(), packColor(tint), tex);
}

float Canvas::text(Vec2 pos, std::string_view s, const Font& font, float px, Color c) {
    uint32_t col = packColor(c);
    float baseline = std::round(pos.y + font.ascent(px));
    float pen = pos.x;
    uint32_t prev = 0;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = decodeUtf8(s, i);
        if (prev) pen += font.kerning(prev, cp, px);
        const GlyphAtlas::Glyph& g = atlas_.glyph(font, cp, px);
        if (!g.empty) {
            float x0 = std::round(pen) + g.x0, y0 = baseline + g.y0;
            quad(x0, y0, x0 + (g.x1 - g.x0), y0 + (g.y1 - g.y0), g.u0, g.v0, g.u1, g.v1, col);
        }
        pen += g.advance;
        prev = cp;
    }
    return pen;
}

void Canvas::textIn(const Rect& r, std::string_view s, const Font& font, float px, Color c, Align h, bool ellipsis) {
    float w = font.measure(s, px);
    std::string clipped;
    std::string_view shown = s;
    if (ellipsis && w > r.w && r.w > 0) {
        const char* dots = "...";
        float dw = font.measure(dots, px);
        size_t i = 0, keep = 0;
        float pen = 0;
        while (i < s.size()) {
            size_t start = i;
            uint32_t cp = decodeUtf8(s, i);
            float adv = font.advance(cp, px);
            if (pen + adv + dw > r.w) break;
            pen += adv;
            keep = i;
            (void)start;
        }
        clipped = std::string(s.substr(0, keep)) + dots;
        shown = clipped;
        w = font.measure(shown, px);
    }
    float x = h == Align::Begin ? r.x : (h == Align::Center ? r.x + (r.w - w) * 0.5f : r.right() - w);
    float lh = font.lineHeight(px);
    float y = r.y + (r.h - lh) * 0.5f;
    text({std::round(x), std::round(y)}, shown, font, px, c);
}

void Canvas::icon(const Rect& r, uint32_t cp, const Font& iconFont, float px, Color c) {
    if (!cp) return;
    const GlyphAtlas::Glyph& g = atlas_.glyph(iconFont, cp, px);
    if (g.empty) return;
    float gw = g.x1 - g.x0, gh = g.y1 - g.y0;
    float x0 = std::round(r.x + (r.w - gw) * 0.5f), y0 = std::round(r.y + (r.h - gh) * 0.5f);
    quad(x0, y0, x0 + gw, y0 + gh, g.u0, g.v0, g.u1, g.v1, packColor(c));
}

// ---------------------------------------------------------------------------------- renderer
}  // namespace ose::ui
