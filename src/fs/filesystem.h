// Reading files from the operating system's file system (the only file access the toolkit
// needs: fonts, layouts, themes).
#pragma once
#include <optional>
#include <string>

#include "core/util.h"

namespace ose::fs {

std::optional<Bytes> readOsFile(const std::string& osPath);

}  // namespace ose::fs
