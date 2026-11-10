// Draws a Canvas. The toolkit only talks to the graphics API through this interface; the
// GLES 2 / WebGL 1 back end is in render/gles2, applications can add others.
#pragma once
#include "ui/canvas.h"

namespace ose::ui {

class Renderer {
public:
    virtual ~Renderer() = default;
    // RGBA8 textures for images in the UI (Canvas::image, TextureRect, item icons).
    // `pixelated`: nearest-neighbour filtering (pixel art, tiles shown zoomed in).
    virtual TextureHandle createTexture(int width, int height, const void* rgba, bool pixelated = false) = 0;
    virtual void updateTexture(TextureHandle texture, int x, int y, int width, int height, const void* rgba) = 0;
    virtual void destroyTexture(TextureHandle texture) = 0;
    // Draws the canvas into the current render target of `width` x `height` pixels. The glyph
    // atlas is uploaded when it changed.
    virtual void render(const Canvas& canvas, int width, int height) = 0;
};

}  // namespace ose::ui
