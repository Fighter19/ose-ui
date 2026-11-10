// String helpers, a bounds-checked byte reader and a minimal Result type.
#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/math.h"

namespace ose {

using Bytes = std::vector<uint8_t>;
using ByteSpan = std::span<const uint8_t>;

// ---------------------------------------------------------------------------------------
// Result<T>: a value or an error message. Loaders return this so callers can report *why*.
template <class T>
class Result {
public:
    Result(T v) : value_(std::move(v)) {}
    static Result fail(std::string msg) {
        Result r;
        r.error_ = std::move(msg);
        return r;
    }
    explicit operator bool() const { return value_.has_value(); }
    bool ok() const { return value_.has_value(); }
    T& operator*() { return *value_; }
    const T& operator*() const { return *value_; }
    T* operator->() { return &*value_; }
    const T* operator->() const { return &*value_; }
    T& value() { return *value_; }
    const std::string& error() const { return error_; }

private:
    Result() = default;
    std::optional<T> value_;
    std::string error_;
};

// ---------------------------------------------------------------------------------------
// Strings
std::string toLower(std::string_view s);
bool iequals(std::string_view a, std::string_view b);
bool istartsWith(std::string_view s, std::string_view prefix);
bool iendsWith(std::string_view s, std::string_view suffix);
std::string_view trim(std::string_view s);
std::vector<std::string_view> split(std::string_view s, char sep, bool skipEmpty = true);
std::vector<std::string_view> splitWhitespace(std::string_view s);
// Normalised asset path: lower case, '/' separators, no "./", no duplicate or leading '/'.
std::string normalizePath(std::string_view path);
std::string stripExtension(std::string_view path);
std::string_view extension(std::string_view path);  // without the dot, may be empty
std::string_view fileName(std::string_view path);
std::string_view dirName(std::string_view path);    // without trailing '/'
// Ensures `path` starts with `root/` (e.g. "materials") and ends with `.ext`.
std::string assetPath(std::string_view root, std::string_view path, std::string_view ext);

float parseFloat(std::string_view s, float def = 0.0f);
int parseInt(std::string_view s, int def = 0);
// "x y z" -> Vec3. Also accepts "[x y z]" and "{r g b}" (the latter scaled by 1/255).
Vec3 parseVec3(std::string_view s, Vec3 def = {});
Vec4 parseVec4(std::string_view s, Vec4 def = {});
QAngle parseAngles(std::string_view s);

struct CaseInsensitiveHash {
    size_t operator()(std::string_view s) const;
};
struct CaseInsensitiveEq {
    bool operator()(std::string_view a, std::string_view b) const { return iequals(a, b); }
};

uint32_t crc32(ByteSpan data);
uint64_t fnv1a64(std::string_view s);

// ---------------------------------------------------------------------------------------
// Bounds-checked little-endian reader over a byte span. All reads are memcpy based so
// unaligned and packed data is safe. Failed reads set `ok = false` and return zeros.
class Reader {
public:
    Reader() = default;
    explicit Reader(ByteSpan data) : data_(data) {}

    size_t size() const { return data_.size(); }
    ByteSpan data() const { return data_; }
    bool ok() const { return ok_; }

    bool inRange(size_t offset, size_t bytes) const {
        return offset <= data_.size() && bytes <= data_.size() - offset;
    }

    template <class T>
    T read(size_t offset) {
        static_assert(std::is_trivially_copyable_v<T>);
        T out{};
        if (!inRange(offset, sizeof(T))) {
            ok_ = false;
            return out;
        }
        std::memcpy(&out, data_.data() + offset, sizeof(T));
        return out;
    }

    template <class T>
    std::vector<T> readArray(size_t offset, size_t count) {
        static_assert(std::is_trivially_copyable_v<T>);
        std::vector<T> out;
        if (count > (1u << 28) || !inRange(offset, count * sizeof(T))) {
            ok_ = false;
            return out;
        }
        out.resize(count);
        if (count) std::memcpy(out.data(), data_.data() + offset, count * sizeof(T));
        return out;
    }

    ByteSpan sub(size_t offset, size_t bytes) {
        if (!inRange(offset, bytes)) {
            ok_ = false;
            return {};
        }
        return data_.subspan(offset, bytes);
    }

    // NUL-terminated string starting at offset (bounded by the buffer).
    std::string cstring(size_t offset) const {
        if (offset >= data_.size()) return {};
        const char* p = reinterpret_cast<const char*>(data_.data() + offset);
        size_t maxLen = data_.size() - offset;
        return std::string(p, strnlen(p, maxLen));
    }

private:
    ByteSpan data_;
    bool ok_ = true;
};

// Sequential writer used by tests and tools to build files.
class Writer {
public:
    template <class T>
    size_t put(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        size_t at = buf.size();
        buf.resize(at + sizeof(T));
        std::memcpy(buf.data() + at, &v, sizeof(T));
        return at;
    }
    size_t putBytes(const void* p, size_t n) {
        size_t at = buf.size();
        buf.resize(at + n);
        if (n) std::memcpy(buf.data() + at, p, n);
        return at;
    }
    size_t putString(std::string_view s, bool nul = true) {
        size_t at = putBytes(s.data(), s.size());
        if (nul) buf.push_back(0);
        return at;
    }
    void pad(size_t alignment) {
        while (buf.size() % alignment) buf.push_back(0);
    }
    template <class T>
    void patch(size_t at, const T& v) {
        std::memcpy(buf.data() + at, &v, sizeof(T));
    }
    size_t size() const { return buf.size(); }
    Bytes buf;
};

}  // namespace ose
