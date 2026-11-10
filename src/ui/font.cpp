#include "ui/font.h"

#include <atomic>
#include <cmath>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "fs/filesystem.h"

namespace ose::ui {

namespace {
std::atomic<int> g_nextFontId{1};
stbtt_fontinfo* fi(void* p) { return static_cast<stbtt_fontinfo*>(p); }
}  // namespace

std::shared_ptr<Font> Font::load(Bytes ttf, std::string name) {
    std::shared_ptr<Font> f(new Font());
    f->data_ = std::move(ttf);
    f->name_ = std::move(name);
    auto* info = new stbtt_fontinfo;
    int offset = stbtt_GetFontOffsetForIndex(f->data_.data(), 0);
    if (offset < 0 || !stbtt_InitFont(info, f->data_.data(), offset)) {
        delete info;
        return nullptr;
    }
    f->info_ = info;
    int a, d, g;
    stbtt_GetFontVMetrics(info, &a, &d, &g);
    f->unitsAscent_ = float(a);
    f->unitsDescent_ = float(d);
    f->unitsGap_ = float(g);
    f->id_ = g_nextFontId++;
    return f;
}

std::shared_ptr<Font> Font::loadFile(const std::string& path) {
    auto data = fs::readOsFile(path);
    if (!data) return nullptr;
    return load(std::move(*data), path);
}

Font::~Font() { delete fi(info_); }

float Font::ascent(float px) const { return unitsAscent_ * stbtt_ScaleForMappingEmToPixels(fi(info_), px); }
float Font::descent(float px) const { return unitsDescent_ * stbtt_ScaleForMappingEmToPixels(fi(info_), px); }
float Font::lineHeight(float px) const {
    return (unitsAscent_ - unitsDescent_ + unitsGap_) * stbtt_ScaleForMappingEmToPixels(fi(info_), px);
}

float Font::advance(uint32_t cp, float px) const {
    uint64_t key = (uint64_t(cp) << 20) ^ uint64_t(px * 64);
    auto it = advanceCache_.find(key);
    if (it != advanceCache_.end()) return it->second;
    int adv, lsb;
    stbtt_GetCodepointHMetrics(fi(info_), int(cp), &adv, &lsb);
    float v = float(adv) * stbtt_ScaleForMappingEmToPixels(fi(info_), px);
    advanceCache_[key] = v;
    return v;
}

float Font::kerning(uint32_t a, uint32_t b, float px) const {
    return float(stbtt_GetCodepointKernAdvance(fi(info_), int(a), int(b))) * stbtt_ScaleForMappingEmToPixels(fi(info_), px);
}

bool Font::hasGlyph(uint32_t cp) const { return stbtt_FindGlyphIndex(fi(info_), int(cp)) != 0; }

float Font::measure(std::string_view text, float px) const {
    float w = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < text.size();) {
        uint32_t cp = decodeUtf8(text, i);
        if (prev) w += kerning(prev, cp, px);
        w += advance(cp, px);
        prev = cp;
    }
    return w;
}

size_t Font::hitTest(std::string_view text, float px, float x) const {
    float pen = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < text.size();) {
        size_t start = i;
        uint32_t cp = decodeUtf8(text, i);
        float adv = advance(cp, px) + (prev ? kerning(prev, cp, px) : 0);
        if (x < pen + adv * 0.5f) return start;
        pen += adv;
        prev = cp;
    }
    return text.size();
}

// ---------------------------------------------------------------------------------- atlas
GlyphAtlas::GlyphAtlas(int size) { reset(size); }

void GlyphAtlas::reset(int size) {
    size_ = size;
    pixels_.assign(size_t(size) * size * 4, 0);
    // White block in the corner for solid fills.
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) std::memset(&pixels_[(size_t(y) * size + x) * 4], 255, 4);
    shelfX_ = 4;
    shelfY_ = 0;
    shelfH_ = 4;
    glyphs_.clear();
    ++revision_;
    ++generation_;
}

const GlyphAtlas::Glyph& GlyphAtlas::glyph(const Font& font, uint32_t cp, float px) {
    uint64_t key = (uint64_t(font.id()) << 48) ^ (uint64_t(cp) << 16) ^ uint64_t(std::lround(px * 4));
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) return it->second;
    Glyph g;
    g.advance = font.advance(cp, px);
    stbtt_fontinfo* info = fi(font.info());
    float scale = stbtt_ScaleForMappingEmToPixels(info, px);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(info, int(cp), scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0, h = y1 - y0;
    if (w > 0 && h > 0) {
        if (shelfX_ + w + 1 > size_) {
            shelfX_ = 0;
            shelfY_ += shelfH_ + 1;
            shelfH_ = 0;
        }
        if (shelfY_ + h + 1 > size_) {
            // Full: grow, or start over.
            reset(size_ < 2048 ? size_ * 2 : size_);
            return glyph(font, cp, px);
        }
        std::vector<uint8_t> bmp(size_t(w) * h);
        stbtt_MakeCodepointBitmap(info, bmp.data(), w, h, w, scale, scale, int(cp));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint8_t* p = &pixels_[(size_t(shelfY_ + y) * size_ + size_t(shelfX_ + x)) * 4];
                p[0] = p[1] = p[2] = 255;
                p[3] = bmp[size_t(y) * w + x];
            }
        float inv = 1.0f / float(size_);
        g.u0 = float(shelfX_) * inv;
        g.v0 = float(shelfY_) * inv;
        g.u1 = float(shelfX_ + w) * inv;
        g.v1 = float(shelfY_ + h) * inv;
        g.x0 = float(x0);
        g.y0 = float(y0);
        g.x1 = float(x1);
        g.y1 = float(y1);
        g.empty = false;
        shelfX_ += w + 1;
        shelfH_ = std::max(shelfH_, h);
        ++revision_;
    }
    return glyphs_.emplace(key, g).first->second;
}

}  // namespace ose::ui
