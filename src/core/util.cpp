#include "core/util.h"

#include <cctype>
#include <cstdlib>

namespace ose {

static inline char lowerChar(char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; }

std::string toLower(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = lowerChar(c);
    return r;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (lowerChar(a[i]) != lowerChar(b[i])) return false;
    return true;
}

bool istartsWith(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && iequals(s.substr(0, p.size()), p);
}

bool iendsWith(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && iequals(s.substr(s.size() - p.size()), p);
}

std::string_view trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string_view> split(std::string_view s, char sep, bool skipEmpty) {
    std::vector<std::string_view> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == sep) {
            auto part = s.substr(start, i - start);
            if (!skipEmpty || !part.empty()) out.push_back(part);
            start = i + 1;
        }
    }
    return out;
}

std::vector<std::string_view> splitWhitespace(std::string_view s) {
    std::vector<std::string_view> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        size_t b = i;
        while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i > b) out.push_back(s.substr(b, i - b));
    }
    return out;
}

std::string normalizePath(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
        c = (c == '\\') ? '/' : lowerChar(c);
        if (c == '/' && (out.empty() || out.back() == '/')) continue;
        out.push_back(c);
    }
    // Remove "./" segments.
    std::string clean;
    clean.reserve(out.size());
    for (auto seg : split(out, '/')) {
        if (seg == ".") continue;
        if (!clean.empty()) clean.push_back('/');
        clean.append(seg);
    }
    return clean;
}

std::string stripExtension(std::string_view path) {
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot != std::string_view::npos && (slash == std::string_view::npos || dot > slash))
        return std::string(path.substr(0, dot));
    return std::string(path);
}

std::string_view extension(std::string_view path) {
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot != std::string_view::npos && (slash == std::string_view::npos || dot > slash))
        return path.substr(dot + 1);
    return {};
}

std::string_view fileName(std::string_view path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view dirName(std::string_view path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash);
}

std::string assetPath(std::string_view root, std::string_view path, std::string_view ext) {
    std::string p = normalizePath(path);
    std::string r = normalizePath(root);
    if (!r.empty() && !(p.size() > r.size() && p.compare(0, r.size(), r) == 0 && p[r.size()] == '/'))
        p = r + "/" + p;
    if (!ext.empty() && !iequals(extension(p), ext)) {
        p += ".";
        p += ext;
    }
    return p;
}

float parseFloat(std::string_view s, float def) {
    s = trim(s);
    if (s.empty()) return def;
    std::string tmp(s);
    char* end = nullptr;
    float v = std::strtof(tmp.c_str(), &end);
    return end == tmp.c_str() ? def : v;
}

int parseInt(std::string_view s, int def) {
    s = trim(s);
    if (s.empty()) return def;
    std::string tmp(s);
    char* end = nullptr;
    long v = std::strtol(tmp.c_str(), &end, 10);
    if (end == tmp.c_str()) {
        // Accept "1.0" style ints.
        float f = std::strtof(tmp.c_str(), &end);
        return end == tmp.c_str() ? def : int(f);
    }
    return int(v);
}

static std::vector<float> parseFloats(std::string_view s, bool* braces) {
    s = trim(s);
    *braces = false;
    if (!s.empty() && (s.front() == '[' || s.front() == '{')) {
        *braces = s.front() == '{';
        s.remove_prefix(1);
        if (!s.empty() && (s.back() == ']' || s.back() == '}')) s.remove_suffix(1);
    }
    std::vector<float> out;
    for (auto part : splitWhitespace(s)) out.push_back(parseFloat(part));
    return out;
}

Vec3 parseVec3(std::string_view s, Vec3 def) {
    bool braces;
    auto f = parseFloats(s, &braces);
    if (f.empty()) return def;
    if (f.size() == 1) f = {f[0], f[0], f[0]};  // "$color 1" style scalars
    while (f.size() < 3) f.push_back(0);
    Vec3 v{f[0], f[1], f[2]};
    return braces ? v * (1.0f / 255.0f) : v;
}

Vec4 parseVec4(std::string_view s, Vec4 def) {
    bool braces;
    auto f = parseFloats(s, &braces);
    if (f.empty()) return def;
    while (f.size() < 4) f.push_back(f.size() == 3 ? (braces ? 255.0f : 1.0f) : 0.0f);
    Vec4 v{f[0], f[1], f[2], f[3]};
    if (braces) v = {v.x / 255, v.y / 255, v.z / 255, v.w / 255};
    return v;
}

QAngle parseAngles(std::string_view s) {
    Vec3 v = parseVec3(s);
    return {v.x, v.y, v.z};
}

size_t CaseInsensitiveHash::operator()(std::string_view s) const {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) {
        h ^= uint8_t(lowerChar(c));
        h *= 1099511628211ull;
    }
    return size_t(h);
}

uint32_t crc32(ByteSpan data) {
    static uint32_t table[256];
    static bool init = [] {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        return true;
    }();
    (void)init;
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : data) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

uint64_t fnv1a64(std::string_view s) {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) {
        h ^= uint8_t(c);
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace ose
