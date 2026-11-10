// Docking: panels in tabbed areas around a centre (left, right, bottom), separated by
// draggable splits. Tabs can be dragged between areas. The arrangement is saved and
// restored as text.
//
//   <DockRoot>
//     <Dock area="left" title="Tools" icon="wrench"> ... </Dock>
//     <Dock area="center" title="3D"> ... </Dock>
//   </DockRoot>
#pragma once
#include "ui/containers.h"

namespace ose::ui {

class DockRoot : public Control {
public:
    DockRoot();
    const char* typeName() const override { return "DockRoot"; }
    Vec2 computeMinSize() override;
    void layout() override;
    void process(float dt) override;
    bool setProperty(std::string_view key, std::string_view value) override;

    enum Area { Left, Center, Right, Bottom, AreaCount };
    static int areaFromName(std::string_view n);
    // Adds a panel; its `title` is the tab text and `id` names it in saved layouts.
    void addPanel(std::unique_ptr<Control> panel, int area);
    TabContainer* area(int a) { return areas_[a]; }
    Control* panel(std::string_view id);
    void showPanel(std::string_view id);  // selects its tab

    // "left=tools,props;right=...;sizes=260,300,220" (see saveLayout).
    std::string saveLayout() const;
    void loadLayout(std::string_view text);

private:
    HSplit* outer_;   // left | rest
    HSplit* inner_;   // middle | right
    VSplit* middle_;  // center | bottom
    TabContainer* areas_[AreaCount];
};

}  // namespace ose::ui
