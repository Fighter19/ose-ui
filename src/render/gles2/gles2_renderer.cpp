#include "render/gles2/gles2_renderer.h"

#include <GLES2/gl2.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ose::ui {

namespace {

const char* kVS = R"GLSL(#version 100
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec4 aColor;
uniform vec2 uScreen;
varying vec2 vUV;
varying vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = vec4(aPos.x * uScreen.x - 1.0, 1.0 - aPos.y * uScreen.y, 0.0, 1.0);
}
)GLSL";

const char* kFS = R"GLSL(#version 100
precision mediump float;
uniform sampler2D sTex;
varying vec2 vUV;
varying vec4 vColor;
void main() { gl_FragColor = texture2D(sTex, vUV) * vColor; }
)GLSL";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "ose-ui: shader: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

class Gles2Renderer final : public Renderer {
public:
    explicit Gles2Renderer(const Gles2Options& options) {
        GLuint vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
        if (vs && fs) {
            program_ = glCreateProgram();
            glAttachShader(program_, vs);
            glAttachShader(program_, fs);
            glBindAttribLocation(program_, 0, "aPos");
            glBindAttribLocation(program_, 1, "aUV");
            glBindAttribLocation(program_, 2, "aColor");
            glLinkProgram(program_);
            GLint ok = 0;
            glGetProgramiv(program_, GL_LINK_STATUS, &ok);
            if (!ok) {
                std::fprintf(stderr, "ose-ui: program link failed\n");
                glDeleteProgram(program_);
                program_ = 0;
            }
        }
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        if (program_) {
            uScreen_ = glGetUniformLocation(program_, "uScreen");
            sTex_ = glGetUniformLocation(program_, "sTex");
        }
        glGenBuffers(1, &vb_);
        glGenBuffers(1, &ib_);
        const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        uint32Indices_ = !options.force16BitIndices && ext && std::strstr(ext, "GL_OES_element_index_uint") != nullptr;
    }

    ~Gles2Renderer() override {
        for (auto& [id, tex] : textures_) glDeleteTextures(1, &tex);
        if (atlas_) glDeleteTextures(1, &atlas_);
        glDeleteBuffers(1, &vb_);
        glDeleteBuffers(1, &ib_);
        if (program_) glDeleteProgram(program_);
    }

    TextureHandle createTexture(int width, int height, const void* rgba, bool pixelated) override {
        GLuint tex = makeTexture(width, height, rgba, pixelated);
        TextureHandle h{nextId_++};
        textures_[h.id] = tex;
        return h;
    }

    void updateTexture(TextureHandle texture, int x, int y, int width, int height, const void* rgba) override {
        auto it = textures_.find(texture.id);
        if (it == textures_.end()) return;
        glBindTexture(GL_TEXTURE_2D, it->second);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    }

    void destroyTexture(TextureHandle texture) override {
        auto it = textures_.find(texture.id);
        if (it == textures_.end()) return;
        glDeleteTextures(1, &it->second);
        textures_.erase(it);
    }

    void render(const Canvas& canvas, int width, int height) override {
        if (!program_ || canvas.commands().empty()) return;

        // Glyph atlas: (re)create when it grew, upload when it changed.
        const GlyphAtlas& atlas = const_cast<Canvas&>(canvas).atlas();
        if (!atlas_ || atlasSize_ != atlas.size()) {
            if (atlas_) glDeleteTextures(1, &atlas_);
            atlas_ = makeTexture(atlas.size(), atlas.size(), nullptr);
            atlasSize_ = atlas.size();
            atlasRevision_ = 0;
        }
        if (atlasRevision_ != atlas.revision()) {
            glBindTexture(GL_TEXTURE_2D, atlas_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, atlasSize_, atlasSize_, GL_RGBA, GL_UNSIGNED_BYTE, atlas.pixels().data());
            atlasRevision_ = atlas.revision();
        }

        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_SCISSOR_TEST);
        glUseProgram(program_);
        glUniform2f(uScreen_, 2.0f / float(std::max(1, width)), 2.0f / float(std::max(1, height)));
        glUniform1i(sTex_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_ARRAY_BUFFER, vb_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib_);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        const auto& vertices = canvas.vertices();
        const auto& indices = canvas.indices();
        bool direct = uint32Indices_;
        if (direct) {
            upload(vertices.data(), vertices.size());
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indices.size() * 4), indices.data(), GL_STREAM_DRAW);
        }

        for (const auto& c : canvas.commands()) {
            if (c.indexCount == 0 || c.clip.empty()) continue;
            int x = int(std::floor(c.clip.x)), y = int(std::floor(c.clip.y));
            int w = int(std::ceil(c.clip.right())) - x, h = int(std::ceil(c.clip.bottom())) - y;
            glScissor(x, height - (y + h), w, h);  // GL scissors count from the bottom
            GLuint tex = atlas_;
            if (c.texture) {
                auto it = textures_.find(c.texture.id);
                if (it != textures_.end()) tex = it->second;
            }
            glBindTexture(GL_TEXTURE_2D, tex);
            if (direct) {
                glDrawElements(GL_TRIANGLES, GLsizei(c.indexCount), GL_UNSIGNED_INT,
                               reinterpret_cast<const void*>(uintptr_t(c.firstIndex) * 4));
            } else {
                // Without 32-bit indices: copy the command's vertices and draw with 16-bit
                // indices, in pieces of at most 65535 vertices.
                drawWith16BitIndices(vertices, indices, c.firstIndex, c.indexCount);
            }
        }

        glDisable(GL_SCISSOR_TEST);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }

private:
    GLuint makeTexture(int width, int height, const void* rgba, bool pixelated = false) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        // No mipmaps and clamping: valid for any size on GLES 2 / WebGL 1.
        GLint filter = pixelated ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        return tex;
    }

    void upload(const Canvas::Vertex* v, size_t count) {
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(count * sizeof(Canvas::Vertex)), v, GL_STREAM_DRAW);
        const GLsizei stride = sizeof(Canvas::Vertex);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Canvas::Vertex, x)));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Canvas::Vertex, u)));
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<const void*>(offsetof(Canvas::Vertex, color)));
    }

    void drawWith16BitIndices(const std::vector<Canvas::Vertex>& vertices, const std::vector<uint32_t>& indices,
                              uint32_t first, uint32_t count) {
        scratchVertices_.clear();
        scratchIndices_.clear();
        remap_.clear();
        auto flush = [&] {
            if (scratchIndices_.empty()) return;
            upload(scratchVertices_.data(), scratchVertices_.size());
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(scratchIndices_.size() * 2), scratchIndices_.data(), GL_STREAM_DRAW);
            glDrawElements(GL_TRIANGLES, GLsizei(scratchIndices_.size()), GL_UNSIGNED_SHORT, nullptr);
            scratchVertices_.clear();
            scratchIndices_.clear();
            remap_.clear();
        };
        for (uint32_t i = 0; i + 2 < count; i += 3) {
            if (scratchVertices_.size() + 3 > 65535) flush();
            for (int k = 0; k < 3; k++) {
                uint32_t src = indices[first + i + uint32_t(k)];
                auto it = remap_.find(src);
                uint16_t dst;
                if (it == remap_.end()) {
                    dst = uint16_t(scratchVertices_.size());
                    scratchVertices_.push_back(vertices[src]);
                    remap_[src] = dst;
                } else {
                    dst = it->second;
                }
                scratchIndices_.push_back(dst);
            }
        }
        flush();
    }

    GLuint program_ = 0, vb_ = 0, ib_ = 0, atlas_ = 0;
    GLint uScreen_ = -1, sTex_ = -1;
    int atlasSize_ = 0;
    uint32_t atlasRevision_ = 0;
    bool uint32Indices_ = false;
    uint32_t nextId_ = 1;
    std::unordered_map<uint32_t, GLuint> textures_;
    std::vector<Canvas::Vertex> scratchVertices_;
    std::vector<uint16_t> scratchIndices_;
    std::unordered_map<uint32_t, uint16_t> remap_;
};

}  // namespace

std::unique_ptr<Renderer> createGles2Renderer(const Gles2Options& options) { return std::make_unique<Gles2Renderer>(options); }

}  // namespace ose::ui
