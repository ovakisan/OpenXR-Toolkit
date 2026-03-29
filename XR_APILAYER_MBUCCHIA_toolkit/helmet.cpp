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

#include "pch.h"

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"
#include "helmet.h"

#include <wincodec.h>

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::log;

    // Constant buffer layout must match helmet.hlsl
    struct alignas(16) HelmetConfig {
        XrVector4f GameRect;   // normalized {left, top, right, bottom} of game in display
        XrVector4f VisorRect;  // normalized {left, top, right, bottom} of visor in PNG
        float      Brightness; // 0..1 multiplier for helmet RGB
        float      _pad[3];
    };

} // anonymous namespace

namespace toolkit::graphics {

    HelmetOverlay::HelmetOverlay(std::shared_ptr<IConfigManager> configManager,
                                 std::shared_ptr<IDevice> graphicsDevice)
        : m_configManager(std::move(configManager)), m_device(std::move(graphicsDevice)) {
        createRenderResources();
    }

    void HelmetOverlay::reload() {
        createRenderResources();
    }

    bool HelmetOverlay::isTextureLoaded() const {
        return m_textureLoaded;
    }

    void HelmetOverlay::process(std::shared_ptr<ITexture> runtimeTexture,
                               const XrVector4f& gameRect,
                               const XrVector4f& visorRect,
                               float brightness) {
        if (!m_textureLoaded || !m_shader || !m_helmetTexture) {
            return;
        }

        HelmetConfig config{};
        config.GameRect   = gameRect;
        config.VisorRect  = visorRect;
        config.Brightness = brightness;
        m_cbParams->uploadData(&config, sizeof(config));

        // Render the helmet overlay onto runtimeTexture with alpha blending.
        // The shader outputs the helmet PNG color; hardware alpha blending composites it
        // so transparent PNG pixels leave the game content untouched.
        m_device->setShader(m_shader, SamplerType::LinearClamp);
        m_device->setShaderInput(0, m_cbParams);
        m_device->setShaderInput(0, m_helmetTexture);
        m_device->setShaderOutput(0, runtimeTexture);
        m_device->dispatchShader();
    }

    void HelmetOverlay::createRenderResources() {
        const auto shadersDir = dllHome / "shaders";
        const auto shaderFile = shadersDir / "helmet.hlsl";

        m_shader = nullptr;
        m_cbParams = nullptr;
        m_helmetTexture = nullptr;
        m_textureLoaded = false;

        try {
            // Create the alpha-blending quad shader.
            m_shader = m_device->createQuadShader(shaderFile, "mainHelmet", "Helmet PS",
                                                  nullptr, {}, /* alphaBlend= */ true);

            m_cbParams = m_device->createBuffer(sizeof(HelmetConfig), "Helmet CB");

            // Load the helmet PNG (once; reloaded if file changes on shader reload).
            m_helmetTexture = loadPngTexture();
            m_textureLoaded = (m_helmetTexture != nullptr);
        } catch (const std::exception& e) {
            Log("Helmet: failed to create render resources: %s\n", e.what());
            m_shader = nullptr;
            m_cbParams = nullptr;
            m_helmetTexture = nullptr;
            m_textureLoaded = false;
        }
    }

    std::shared_ptr<ITexture> HelmetOverlay::loadPngTexture() {
        // Search for helmet.png in localAppData first, then alongside the DLL.
        const std::vector<std::filesystem::path> searchPaths = {
            localAppData / "helmet.png",
            dllHome / "helmet.png",
        };

        std::filesystem::path pngPath;
        for (const auto& candidate : searchPaths) {
            if (std::filesystem::exists(candidate)) {
                pngPath = candidate;
                break;
            }
        }

        if (pngPath.empty()) {
            Log("Helmet: helmet.png not found. Place it at %S\\helmet.png\n", localAppData.c_str());
            return nullptr;
        }

        Log("Helmet: loading PNG from %S\n", pngPath.c_str());

        // Use WIC to decode the PNG into BGRA8 pixel data.
        // Ensure COM is initialized on this thread (may not be if called early in session setup).
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        ComPtr<IWICImagingFactory> wicFactory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,
                                      nullptr,
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(set(wicFactory)));
        if (FAILED(hr)) {
            Log("Helmet: failed to create WIC factory (hr=0x%x)\n", hr);
            return nullptr;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = wicFactory->CreateDecoderFromFilename(pngPath.c_str(),
                                                   nullptr,
                                                   GENERIC_READ,
                                                   WICDecodeMetadataCacheOnDemand,
                                                   set(decoder));
        if (FAILED(hr)) {
            Log("Helmet: failed to open PNG file (hr=0x%x)\n", hr);
            return nullptr;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, set(frame));
        if (FAILED(hr)) {
            Log("Helmet: failed to get PNG frame (hr=0x%x)\n", hr);
            return nullptr;
        }

        // Convert to 32-bit BGRA so it has an alpha channel.
        ComPtr<IWICFormatConverter> converter;
        hr = wicFactory->CreateFormatConverter(set(converter));
        if (FAILED(hr)) {
            Log("Helmet: failed to create WIC format converter (hr=0x%x)\n", hr);
            return nullptr;
        }

        hr = converter->Initialize(get(frame),
                                   GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0.0,
                                   WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr)) {
            Log("Helmet: failed to initialize WIC converter (hr=0x%x)\n", hr);
            return nullptr;
        }

        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);

        const uint32_t rowPitch = alignTo(width * 4u, m_device->getTextureAlignmentConstraint());
        const uint32_t imageSize = rowPitch * height;
        std::vector<uint8_t> pixels(imageSize, 0);

        // WIC CopyPixels stride must match our rowPitch.
        hr = converter->CopyPixels(nullptr, rowPitch, imageSize, pixels.data());
        if (FAILED(hr)) {
            Log("Helmet: failed to copy PNG pixels (hr=0x%x)\n", hr);
            return nullptr;
        }

        // Create the GPU texture.
        XrSwapchainCreateInfo texInfo{};
        texInfo.width       = width;
        texInfo.height      = height;
        texInfo.arraySize   = 1;
        texInfo.mipCount    = 1;
        texInfo.sampleCount = 1;
        texInfo.format      = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        texInfo.usageFlags  = XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        auto texture = m_device->createTexture(texInfo, "HelmetPNG", 0, rowPitch, imageSize, pixels.data());
        if (!texture) {
            Log("Helmet: failed to create GPU texture\n");
            return nullptr;
        }

        Log("Helmet: loaded %ux%u helmet PNG\n", width, height);
        return texture;
    }

} // namespace toolkit::graphics
