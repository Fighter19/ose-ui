// Builds control trees from XML layout files (docs/editor/plan.md, section 5):
//
//   <ui>
//     <VBox anchors="full">
//       <MenuBar>
//         <Menu title="File"><Item action="file.save"/><Separator/>...</Menu>
//       </MenuBar>
//       <Button icon="play" action="play.from_camera" disabled="{!map.loaded}"/>
//       <include src="panels/assets.xml"/>
//     </VBox>
//   </ui>
//
// Elements are control types (registerControlType adds more), attributes are properties
// (Control::setProperty), on-<signal>="action.name" runs an action, and a value in braces
// is bound: it is read from the context's Bindings every frame.
#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "ui/control.h"
#include "ui/xml.h"

namespace ose::ui {

class Context;

using ControlFactory = std::function<std::unique_ptr<Control>()>;
void registerControlType(const std::string& name, ControlFactory fn);
std::unique_ptr<Control> createControl(const std::string& name);

class LayoutLoader {
public:
    explicit LayoutLoader(Context& ctx) : ctx_(ctx) {}
    // Reads included files (paths relative to the including file's directory).
    std::function<std::optional<std::string>(const std::string& path)> readFile;
    std::unique_ptr<Control> loadText(std::string_view xml, const std::string& dir = {});
    std::unique_ptr<Control> loadFile(const std::string& path);
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::unique_ptr<Control> build(const XmlNode& n, const std::string& dir);
    void applyAttributes(Control& c, const XmlNode& n);
    std::unique_ptr<Control> buildMenuBar(const XmlNode& n);
    Context& ctx_;
    std::vector<std::string> errors_;
    int depth_ = 0;
};

}  // namespace ose::ui
