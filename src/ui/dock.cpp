#include "ui/dock.h"

#include "core/util.h"

namespace ose::ui {

DockRoot::DockRoot() {
    outer_ = add<HSplit>();
    outer_->fixed = 240;
    areas_[Left] = outer_->add<TabContainer>();
    inner_ = outer_->add<HSplit>();
    inner_->fixed = 300;
    inner_->fixedEnd = true;
    middle_ = inner_->add<VSplit>();
    middle_->fixed = 240;
    middle_->fixedEnd = true;
    areas_[Center] = middle_->add<TabContainer>();
    areas_[Bottom] = middle_->add<TabContainer>();
    areas_[Right] = inner_->add<TabContainer>();
    for (auto* a : areas_) {
        a->dragTabs = true;
        a->sizeFlagsH = a->sizeFlagsV = SizeFlags::Fill | SizeFlags::Expand;
    }
    areas_[Left]->customMinSize = {120, 0};
    areas_[Right]->customMinSize = {160, 0};
    areas_[Bottom]->customMinSize = {0, 80};
    areas_[Center]->customMinSize = {200, 150};
}

int DockRoot::areaFromName(std::string_view n) {
    if (iequals(n, "left")) return Left;
    if (iequals(n, "right")) return Right;
    if (iequals(n, "bottom")) return Bottom;
    return Center;
}

void DockRoot::addPanel(std::unique_ptr<Control> panel, int area) {
    areas_[clampv(area, 0, AreaCount - 1)]->addChild(std::move(panel));
}

Control* DockRoot::panel(std::string_view id) {
    for (auto* a : areas_)
        for (size_t i = 0; i < a->childCount(); ++i)
            if (a->child(i)->id == id) return a->child(i);
    return nullptr;
}

void DockRoot::showPanel(std::string_view id) {
    for (auto* a : areas_)
        for (size_t i = 0; i < a->childCount(); ++i)
            if (a->child(i)->id == id) a->setCurrent(int(i));
}

Vec2 DockRoot::computeMinSize() { return outer_->minimumSize(); }

void DockRoot::process(float) {
    // Empty areas fold away; the splits then give their space to the other side.
    for (auto* a : areas_) a->setVisible(a->childCount() > 0 || a == areas_[Center]);
}

void DockRoot::layout() { outer_->setRect(rect_); }

bool DockRoot::setProperty(std::string_view key, std::string_view v) {
    if (key == "left-width") outer_->fixed = parseFloat(v, 240);
    else if (key == "right-width") inner_->fixed = parseFloat(v, 300);
    else if (key == "bottom-height") middle_->fixed = parseFloat(v, 240);
    else return Control::setProperty(key, v);
    return true;
}

std::string DockRoot::saveLayout() const {
    static const char* names[] = {"left", "center", "right", "bottom"};
    std::string out;
    for (int a = 0; a < AreaCount; ++a) {
        out += names[a];
        out += '=';
        for (size_t i = 0; i < areas_[a]->childCount(); ++i) out += (i ? "," : "") + areas_[a]->child(i)->id;
        out += ';';
    }
    out += "sizes=" + std::to_string(int(outer_->fixed)) + "," + std::to_string(int(inner_->fixed)) + "," + std::to_string(int(middle_->fixed));
    return out;
}

void DockRoot::loadLayout(std::string_view text) {
    for (auto part : split(text, ';')) {
        size_t eq = part.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view key = part.substr(0, eq), val = part.substr(eq + 1);
        if (key == "sizes") {
            auto s = split(val, ',');
            if (s.size() == 3) {
                outer_->fixed = parseFloat(s[0], 240);
                inner_->fixed = parseFloat(s[1], 300);
                middle_->fixed = parseFloat(s[2], 240);
            }
            continue;
        }
        int a = areaFromName(key);
        int at = 0;
        for (auto id : split(val, ',')) {
            // Move the panel with this id to area a, position at.
            for (auto* src : areas_) {
                for (size_t i = 0; i < src->childCount(); ++i) {
                    Control* p = src->child(i);
                    if (p->id != id) continue;
                    auto owned = src->removeChild(p);
                    owned->setVisible(true);
                    areas_[a]->addChild(std::move(owned), at);
                    goto moved;
                }
            }
            continue;
        moved:
            ++at;
        }
    }
    for (auto* a : areas_) a->setCurrent(0);
}

}  // namespace ose::ui
