// TrueType fonts (stb_truetype) and the glyph atlas shared by all text the UI draws.
#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/util.h"
#include "ui/core.h"

namespace ose::ui {

class Font {
public:
    static std::shared_ptr<Font> load(Bytes ttf, std::string name);
    static std::shared_ptr<Font> loadFile(const std::string& path);
    ~Font();
    const std::string& name() const { return name_; }
    int id() const { return id_; }
    // Metrics in pixels for a pixel size (the height of the em box).
    float ascent(float px) const;
    float descent(float px) const;  // negative
    float lineHeight(float px) const;
    float advance(uint32_t cp, float px) const;
    float kerning(uint32_t a, uint32_t b, float px) const;
    bool hasGlyph(uint32_t cp) const;
    // Width of a single line of text.
    float measure(std::string_view text, float px) const;
    // Byte offset of the character boundary closest to x (for text cursors).
    size_t hitTest(std::string_view text, float px, float x) const;
    void* info() const { return info_; }  // stbtt_fontinfo*

private:
    Font() = default;
    Bytes data_;
    void* info_ = nullptr;
    float unitsAscent_ = 0, unitsDescent_ = 0, unitsGap_ = 0;
    std::string name_;
    int id_ = 0;
    mutable std::unordered_map<uint64_t, float> advanceCache_;
};

// Glyphs rasterised on demand into one RGBA8 texture (white, coverage in alpha). Grows from
// 512 to 2048; when full it starts over (the next frame re-adds what is in use).
class GlyphAtlas {
public:
    explicit GlyphAtlas(int size = 512);
    struct Glyph {
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // texture coordinates
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // quad relative to the pen on the baseline
        float advance = 0;
        bool empty = true;                     // nothing to draw (spaces)
    };
    const Glyph& glyph(const Font& font, uint32_t cp, float px);
    // A white texel (solid fills sample it).
    Vec2 whiteUV() const { return {1.0f / float(size_), 1.0f / float(size_)}; }
    int size() const { return size_; }
    const Bytes& pixels() const { return pixels_; }
    uint32_t revision() const { return revision_; }  // changes when pixels change
    uint32_t generation() const { return generation_; }  // changes when the atlas restarts

private:
    void reset(int size);
    int size_;
    Bytes pixels_;
    int shelfX_ = 0, shelfY_ = 0, shelfH_ = 0;
    uint32_t revision_ = 1, generation_ = 1;
    std::unordered_map<uint64_t, Glyph> glyphs_;
};

}  // namespace ose::ui
