#include "fs/filesystem.h"

#include <cstdio>

namespace ose::fs {

std::optional<Bytes> readOsFile(const std::string& osPath) {
    FILE* f = std::fopen(osPath.c_str(), "rb");
    if (!f) return std::nullopt;
    Bytes data;
    unsigned char buf[16384];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    return data;
}

}  // namespace ose::fs
