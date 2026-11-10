#include "ui/xml.h"

#include <cctype>

#include "ui/core.h"

namespace ose::ui {

const std::string* XmlNode::attr(std::string_view key) const {
    for (const auto& [k, v] : attributes)
        if (k == key) return &v;
    return nullptr;
}

std::string_view XmlNode::get(std::string_view key, std::string_view def) const {
    const std::string* v = attr(key);
    return v ? std::string_view(*v) : def;
}

const XmlNode* XmlNode::child(std::string_view n) const {
    for (const auto& c : children)
        if (c.name == n) return &c;
    return nullptr;
}

std::string xmlEscape(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

namespace {

struct Parser {
    std::string_view s;
    size_t p = 0;
    int line = 1;
    std::string error;

    bool fail(std::string msg) {
        if (error.empty()) error = "line " + std::to_string(line) + ": " + msg;
        return false;
    }
    void adv(size_t n = 1) {
        for (size_t i = 0; i < n && p < s.size(); ++i, ++p)
            if (s[p] == '\n') ++line;
    }
    bool starts(std::string_view t) const { return s.substr(p, t.size()) == t; }
    void skipSpace() {
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) adv();
    }
    // Comments, declarations and processing instructions between elements.
    bool skipMisc() {
        for (;;) {
            skipSpace();
            if (starts("<!--")) {
                size_t e = s.find("-->", p + 4);
                if (e == std::string_view::npos) return fail("unterminated comment");
                adv(e + 3 - p);
            } else if (starts("<?")) {
                size_t e = s.find("?>", p + 2);
                if (e == std::string_view::npos) return fail("unterminated declaration");
                adv(e + 2 - p);
            } else if (starts("<!DOCTYPE") || starts("<!doctype")) {
                size_t e = s.find('>', p);
                if (e == std::string_view::npos) return fail("unterminated doctype");
                adv(e + 1 - p);
            } else {
                return true;
            }
        }
    }
    std::string decode(std::string_view raw) {
        std::string out;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '&') {
                out.push_back(raw[i]);
                continue;
            }
            size_t e = raw.find(';', i);
            if (e == std::string_view::npos) {
                out.push_back('&');
                continue;
            }
            std::string_view ent = raw.substr(i + 1, e - i - 1);
            if (ent == "amp") out.push_back('&');
            else if (ent == "lt") out.push_back('<');
            else if (ent == "gt") out.push_back('>');
            else if (ent == "quot") out.push_back('"');
            else if (ent == "apos") out.push_back('\'');
            else if (!ent.empty() && ent[0] == '#') {
                uint32_t cp = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')
                                  ? uint32_t(std::strtoul(std::string(ent.substr(2)).c_str(), nullptr, 16))
                                  : uint32_t(std::strtoul(std::string(ent.substr(1)).c_str(), nullptr, 10));
                appendUtf8(out, cp);
            } else {
                out.append(raw.substr(i, e - i + 1));  // unknown entity: keep as is
            }
            i = e;
        }
        return out;
    }
    bool name(std::string& out) {
        size_t start = p;
        while (p < s.size() && (std::isalnum(static_cast<unsigned char>(s[p])) || s[p] == '_' || s[p] == '-' || s[p] == '.' || s[p] == ':')) adv();
        if (p == start) return fail("expected a name");
        out = std::string(s.substr(start, p - start));
        return true;
    }
    bool element(XmlNode& node) {
        if (!starts("<")) return fail("expected '<'");
        adv();
        node.line = line;
        if (!name(node.name)) return false;
        for (;;) {
            skipSpace();
            if (starts("/>")) {
                adv(2);
                return true;
            }
            if (starts(">")) {
                adv();
                break;
            }
            std::string key;
            if (!name(key)) return false;
            skipSpace();
            if (!starts("=")) return fail("expected '=' after attribute " + key);
            adv();
            skipSpace();
            if (p >= s.size() || (s[p] != '"' && s[p] != '\'')) return fail("attribute " + key + " needs a quoted value");
            char q = s[p];
            adv();
            size_t e = s.find(q, p);
            if (e == std::string_view::npos) return fail("unterminated attribute value");
            std::string value = decode(s.substr(p, e - p));
            adv(e + 1 - p);
            node.attributes.emplace_back(std::move(key), std::move(value));
        }
        // Content.
        std::string text;
        for (;;) {
            if (p >= s.size()) return fail("missing </" + node.name + ">");
            if (starts("</")) {
                adv(2);
                std::string close;
                if (!name(close)) return false;
                if (close != node.name) return fail("</" + close + "> closes <" + node.name + ">");
                skipSpace();
                if (!starts(">")) return fail("expected '>'");
                adv();
                node.text = std::string(trim(text));
                return true;
            }
            if (starts("<!--")) {
                size_t e = s.find("-->", p + 4);
                if (e == std::string_view::npos) return fail("unterminated comment");
                adv(e + 3 - p);
                continue;
            }
            if (starts("<![CDATA[")) {
                size_t e = s.find("]]>", p + 9);
                if (e == std::string_view::npos) return fail("unterminated CDATA");
                text.append(s.substr(p + 9, e - p - 9));
                adv(e + 3 - p);
                continue;
            }
            if (starts("<")) {
                node.children.emplace_back();
                if (!element(node.children.back())) return false;
                continue;
            }
            size_t e = s.find('<', p);
            if (e == std::string_view::npos) e = s.size();
            text += decode(s.substr(p, e - p));
            adv(e - p);
        }
    }
};

}  // namespace

Result<XmlNode> parseXml(std::string_view text) {
    Parser ps{text, 0, 1, {}};
    if (text.size() >= 3 && uint8_t(text[0]) == 0xEF && uint8_t(text[1]) == 0xBB && uint8_t(text[2]) == 0xBF) ps.p = 3;
    XmlNode root;
    if (!ps.skipMisc() || !ps.element(root) || !ps.skipMisc()) return Result<XmlNode>::fail("xml: " + ps.error);
    if (ps.p < text.size()) return Result<XmlNode>::fail("xml: line " + std::to_string(ps.line) + ": content after the root element");
    return root;
}

}  // namespace ose::ui
