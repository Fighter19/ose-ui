// Canvas renderer for OpenGL ES 2.0 / WebGL 1. Also works on GLES 3 / WebGL 2 contexts.
// Needs a current GL context when created, used and destroyed.
#pragma once
#include <memory>

#include "ui/renderer.h"

namespace ose::ui {

struct Gles2Options {
    // Draw with 16-bit indices even if GL_OES_element_index_uint is available (WebGL 1
    // without the extension takes this path; tests use it to cover it).
    bool force16BitIndices = false;
};

std::unique_ptr<Renderer> createGles2Renderer(const Gles2Options& options = {});

}  // namespace ose::ui
