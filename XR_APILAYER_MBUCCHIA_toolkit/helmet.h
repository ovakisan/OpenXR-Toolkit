// MIT License
//
// Copyright(c) 2024 OpenXR-Toolkit Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <memory>

// Forward declarations — full definitions are in interfaces.h (requires pch.h first).
namespace toolkit::config {
    struct IConfigManager;
}
namespace toolkit::graphics {
    struct IDevice;
    struct ITexture;
    struct IQuadShader;
    struct IShaderBuffer;
}

namespace toolkit::graphics {

    // Renders the helmet overlay PNG onto runtimeTexture in the bar regions (outside the
    // game content area defined by gameRect), using alpha blending so the game content
    // visible through the transparent center of the PNG is preserved.
    class HelmetOverlay {
      public:
        HelmetOverlay(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                      std::shared_ptr<IDevice> graphicsDevice);

        // Reload the shader (called on shader reload).
        void reload();

        // Returns true if the helmet PNG was found and loaded successfully.
        bool isTextureLoaded() const;

        // Apply the helmet overlay to runtimeTexture.
        // gameRect:  normalized {left, top, right, bottom} of game content in runtimeTexture [0..1].
        // visorRect: normalized {left, top, right, bottom} of visor opening in the PNG [0..1].
        void process(std::shared_ptr<ITexture> runtimeTexture,
                     const XrVector4f& gameRect,
                     const XrVector4f& visorRect,
                     float brightness);

      private:
        void createRenderResources();
        std::shared_ptr<ITexture> loadPngTexture();

        const std::shared_ptr<toolkit::config::IConfigManager> m_configManager;
        const std::shared_ptr<IDevice>        m_device;

        std::shared_ptr<IQuadShader>  m_shader;
        std::shared_ptr<IShaderBuffer> m_cbParams;
        std::shared_ptr<ITexture>     m_helmetTexture;
        bool                          m_textureLoaded{false};
    };

} // namespace toolkit::graphics
