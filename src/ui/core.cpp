#include "ui/core.h"

#include <algorithm>

#include "core/util.h"

namespace ose::ui {

Rect Rect::intersect(const Rect& o) const {
    float x0 = std::max(x, o.x), y0 = std::max(y, o.y);
    float x1 = std::min(right(), o.right()), y1 = std::min(bottom(), o.bottom());
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

Color parseColor(std::string_view s, Color def) {
    s = trim(s);
    if (s.empty()) return def;
    if (iequals(s, "white")) return {1, 1, 1, 1};
    if (iequals(s, "black")) return {0, 0, 0, 1};
    if (iequals(s, "transparent")) return {0, 0, 0, 0};
    if (s[0] == '#') {
        auto hex = [&](size_t i) {
            int v = 0;
            for (size_t k = i; k < i + 2 && k < s.size(); ++k) {
                char c = s[k];
                v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0)));
            }
            return float(v) / 255.0f;
        };
        if (s.size() == 4) {  // #rgb
            auto h1 = [&](size_t i) {
                char c = s[i];
                int v = c >= '0' && c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10);
                return float(v * 17) / 255.0f;
            };
            return {h1(1), h1(2), h1(3), 1};
        }
        if (s.size() >= 7) return {hex(1), hex(3), hex(5), s.size() >= 9 ? hex(7) : 1.0f};
        return def;
    }
    auto parts = splitWhitespace(s);
    if (parts.size() >= 3)
        return {parseFloat(parts[0]), parseFloat(parts[1]), parseFloat(parts[2]), parts.size() > 3 ? parseFloat(parts[3]) : 1.0f};
    return def;
}

uint32_t decodeUtf8(std::string_view s, size_t& i) {
    if (i >= s.size()) return 0;
    uint8_t c = uint8_t(s[i++]);
    if (c < 0x80) return c;
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
    if (extra < 0) return 0xFFFD;
    uint32_t cp = c & (0x3F >> extra);
    for (int k = 0; k < extra; ++k) {
        if (i >= s.size() || (uint8_t(s[i]) & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (uint8_t(s[i++]) & 0x3F);
    }
    return cp;
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out.push_back(char(cp));
    else if (cp < 0x800) {
        out.push_back(char(0xC0 | (cp >> 6)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(char(0xE0 | (cp >> 12)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(char(0xF0 | (cp >> 18)));
        out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    }
}

size_t prevUtf8(std::string_view s, size_t i) {
    if (i == 0) return 0;
    --i;
    while (i > 0 && (uint8_t(s[i]) & 0xC0) == 0x80) --i;
    return i;
}

size_t nextUtf8(std::string_view s, size_t i) {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && (uint8_t(s[i]) & 0xC0) == 0x80) ++i;
    return i;
}

}  // namespace ose::ui
