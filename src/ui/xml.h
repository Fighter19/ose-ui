// A small XML reader for UI layout and theme files: elements, attributes, text, comments,
// CDATA, the five standard entities and numeric character references. No namespaces, DTDs
// or processing instructions (an <?xml ...?> declaration is skipped).
#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/util.h"

namespace ose::ui {

struct XmlNode {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;  // in file order
    std::vector<XmlNode> children;
    std::string text;  // character data directly inside this element (trimmed)
    int line = 0;

    const std::string* attr(std::string_view key) const;
    std::string_view get(std::string_view key, std::string_view def = {}) const;
    bool has(std::string_view key) const { return attr(key) != nullptr; }
    const XmlNode* child(std::string_view name) const;
};

// Parses a document; returns its root element.
Result<XmlNode> parseXml(std::string_view text);
// Escapes text for attribute values and character data.
std::string xmlEscape(std::string_view s);

}  // namespace ose::ui
