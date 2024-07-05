#include "capture_window.h"
#include <godot_cpp/variant/utility_functions.hpp>

#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")


//#define PROFILER_ENABLED

using namespace godot;

namespace microtaur
{

class AcceleratedWindowCapturer
{
public:
  AcceleratedWindowCapturer() {
    init();
  }

  void init()
  {
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    auto hr = D3D11CreateDevice(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      0,
      &featureLevel,
      1,
      D3D11_SDK_VERSION,
      &m_device,
      nullptr,
      &m_context
    );
    if (FAILED(hr)) {
      reset();
      return;
    }

    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
      reset();
      return;
    }

    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
    if (FAILED(hr)) {
      reset();
      return;
    }
    dxgiDevice->Release();

    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(1, &dxgiOutput); // TODO: screen choose
    if (FAILED(hr)) {
      reset();
      return;
    }
    dxgiAdapter->Release();

    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    if (FAILED(hr)) {
      reset();
      return;
    }
    dxgiOutput->Release();

    // Get desktop duplication
    hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_duplication);
    if (FAILED(hr)) {
      reset();
      return;
    }
    dxgiOutput1->Release();

  }

  Frame nextFrame(size_t outputWidth, size_t outputHeight)
  {
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    if (m_frameAcquired) {
      m_duplication->ReleaseFrame();
      m_frameAcquired = false;
    }

    HRESULT hr = m_duplication->AcquireNextFrame(INFINITE, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
      if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // TODO: maybe adjust this value?
        //std::this_thread::sleep_for(std::chrono::milliseconds(33));
        return {};
      }

      reset();
      return { m_width, m_height, m_buffer };
    }
    m_frameAcquired = true;

    // Get the DXGI surface
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(m_desktopTexture.GetAddressOf()));
    if (FAILED(hr)) {
      desktopResource->Release();
      m_duplication->ReleaseFrame();
      return {};
    }

    // Create a staging texture if that's necessary
    D3D11_TEXTURE2D_DESC desc;
    m_desktopTexture->GetDesc(&desc);

    if (!m_stagingTexture || m_width != desc.Width || m_height == desc.Height) {
      m_width = desc.Width;
      m_height = desc.Height;

      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;
      m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);

      if (!m_stagingTexture) {
        desktopResource->Release();
        m_duplication->ReleaseFrame();
        return {};
      }
    }

    m_context->CopyResource(m_stagingTexture.Get(), m_desktopTexture.Get());
    desktopResource->Release();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
      m_stagingTexture->Release();
      return { m_width, m_height, m_buffer };
    }

    /*
    // Rescale if necessary
    if (m_width != outputWidth || m_height != outputHeight) {
      m_rescaleBuffer.resize(outputWidth * outputHeight * 4);

      rescale(mappedResource, m_width, m_height, outputWidth, outputHeight);

      // Convert BGRA to YUV420
      const auto yuvSize = outputWidth * outputHeight * 3 / 2;
      if (m_buffer.size() != yuvSize) {
        m_buffer.resize(yuvSize);
      }

      rgbToYuv2(outputWidth, outputHeight);

      // Unmap and release the staging texture
      m_context->Unmap(m_stagingTexture.Get(), 0);

      return { m_width, m_height, m_buffer };
    }
    */

    // Convert BGRA to YUV420
    const auto yuvSize = outputWidth * outputHeight * 3 / 2;
    if (m_buffer.size() != yuvSize) {
      m_buffer.resize(yuvSize);
    }

    if (m_width != outputWidth || m_height != outputHeight) {
      rgbToYuvRescale(mappedResource, m_width, m_height, outputWidth, outputHeight);
    }
    else {
      rgbToYuv(mappedResource, outputWidth, outputHeight);
    }

    // Unmap and release the staging texture
    m_context->Unmap(m_stagingTexture.Get(), 0);

    return {m_width, m_height, m_buffer};
  }

  struct Color {
    uint8_t r, g, b, a;
  };

  void rescale(D3D11_MAPPED_SUBRESOURCE mappedResource, size_t inputWidth, size_t inputHeight, size_t outputWidth, size_t outputHeight)
  {
    auto rgb = static_cast<uint8_t*>(mappedResource.pData);


    for (size_t y = 0; y < outputHeight; ++y) {
      for (size_t x = 0; x < outputWidth; ++x) {
        // Calculate corresponding position in the input image
        float srcX = static_cast<float>(x) / outputWidth * inputWidth;
        float srcY = static_cast<float>(y) / outputHeight * inputHeight;

        // Calculate the four surrounding pixels in the input image
        size_t x1 = static_cast<size_t>(std::floor(srcX));
        size_t x2 = std::min(x1 + 1, inputWidth - 1);
        size_t y1 = static_cast<size_t>(std::floor(srcY));
        size_t y2 = std::min(y1 + 1, inputHeight - 1);

        // Calculate interpolation weights
        float dx = srcX - x1;
        float dy = srcY - y1;

        // Interpolate RGBA values using bilinear interpolation
        Color c11 = { rgb[(y1 * inputWidth + x1) * 4], rgb[(y1 * inputWidth + x1) * 4 + 1], rgb[(y1 * inputWidth + x1) * 4 + 2], rgb[(y1 * inputWidth + x1) * 4 + 3] };
        Color c12 = { rgb[(y1 * inputWidth + x2) * 4], rgb[(y1 * inputWidth + x2) * 4 + 1], rgb[(y1 * inputWidth + x2) * 4 + 2], rgb[(y1 * inputWidth + x2) * 4 + 3] };
        Color c21 = { rgb[(y2 * inputWidth + x1) * 4], rgb[(y2 * inputWidth + x1) * 4 + 1], rgb[(y2 * inputWidth + x1) * 4 + 2], rgb[(y2 * inputWidth + x1) * 4 + 3] };
        Color c22 = { rgb[(y2 * inputWidth + x2) * 4], rgb[(y2 * inputWidth + x2) * 4 + 1], rgb[(y2 * inputWidth + x2) * 4 + 2], rgb[(y2 * inputWidth + x2) * 4 + 3] };

        Color result = {
            static_cast<uint8_t>((1.0f - dx) * (1.0f - dy) * c11.r + dx * (1.0f - dy) * c12.r + (1.0f - dx) * dy * c21.r + dx * dy * c22.r),
            static_cast<uint8_t>((1.0f - dx) * (1.0f - dy) * c11.g + dx * (1.0f - dy) * c12.g + (1.0f - dx) * dy * c21.g + dx * dy * c22.g),
            static_cast<uint8_t>((1.0f - dx) * (1.0f - dy) * c11.b + dx * (1.0f - dy) * c12.b + (1.0f - dx) * dy * c21.b + dx * dy * c22.b),
            static_cast<uint8_t>((1.0f - dx) * (1.0f - dy) * c11.a + dx * (1.0f - dy) * c12.a + (1.0f - dx) * dy * c21.a + dx * dy * c22.a)
        };

        // Set the result in the output buffer
        m_rescaleBuffer[(y * outputWidth + x) * 4] = result.r;
        m_rescaleBuffer[(y * outputWidth + x) * 4 + 1] = result.g;
        m_rescaleBuffer[(y * outputWidth + x) * 4 + 2] = result.b;
        m_rescaleBuffer[(y * outputWidth + x) * 4 + 3] = result.a;
      }
    }


  }

  void rgbToYuv(D3D11_MAPPED_SUBRESOURCE mappedResource, size_t width, size_t height)
  {
    auto rgb = static_cast<uint8_t*>(mappedResource.pData);

    size_t upos = width * height;
    size_t vpos = upos + upos / 4;
    size_t i = 0;

    for (size_t line = 0; line < height; ++line) {
      if (!(line % 2)) {
        for (size_t x = 0; x < width; x += 2) {
          uint8_t b = rgb[4 * i];
          uint8_t g = rgb[4 * i + 1];
          uint8_t r = rgb[4 * i + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;

          m_buffer[upos++] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
          m_buffer[vpos++] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;

          b = rgb[4 * i];
          g = rgb[4 * i + 1];
          r = rgb[4 * i + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        }
      }
      else {
        for (size_t x = 0; x < width; x += 1) {
          uint8_t b = rgb[4 * i];
          uint8_t g = rgb[4 * i + 1];
          uint8_t r = rgb[4 * i + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        }
      }
    }
  }

  void rgbToYuv2(size_t width, size_t height)
  {
    auto rgb = m_rescaleBuffer.data();

    size_t upos = width * height;
    size_t vpos = upos + upos / 4;
    size_t i = 0;

    for (size_t line = 0; line < height; ++line) {
      if (!(line % 2)) {
        for (size_t x = 0; x < width; x += 2) {
          uint8_t b = rgb[4 * i];
          uint8_t g = rgb[4 * i + 1];
          uint8_t r = rgb[4 * i + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;

          m_buffer[upos++] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
          m_buffer[vpos++] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;

          b = rgb[4 * i];
          g = rgb[4 * i + 1];
          r = rgb[4 * i + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        }
      }
      else {
        for (size_t x = 0; x < width; x += 1) {
          uint8_t b = rgb[4 * i];
          uint8_t g = rgb[4 * i + 1];
          uint8_t r = rgb[4 * i + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        }
      }
    }
  }

  void rgbToYuvRescale(D3D11_MAPPED_SUBRESOURCE mappedResource, size_t width, size_t height, size_t outputWidth, size_t outputHeight)
  {
    auto rgb = static_cast<uint8_t*>(mappedResource.pData);

    size_t upos = outputWidth * outputHeight;
    size_t vpos = upos + upos / 4;
    size_t i = 0;

    for (size_t line = 0; line < outputHeight; ++line) {
      size_t originalLine = static_cast<size_t>((static_cast<double>(line) / outputHeight) * height);

      if (!(line % 2)) {
        for (size_t x = 0; x < outputWidth; x += 2) {
          size_t originalX = static_cast<size_t>((static_cast<double>(x) / outputWidth) * width);
          uint8_t b = rgb[4 * (originalLine * width + originalX)];
          uint8_t g = rgb[4 * (originalLine * width + originalX) + 1];
          uint8_t r = rgb[4 * (originalLine * width + originalX) + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;

          m_buffer[upos++] = ((-38 * r + -74 * g + 112 * b) >> 8) + 128;
          m_buffer[vpos++] = ((112 * r + -94 * g + -18 * b) >> 8) + 128;

          originalX = static_cast<size_t>((static_cast<double>(x + 1) / outputWidth) * width);
          b = rgb[4 * (originalLine * width + originalX)];
          g = rgb[4 * (originalLine * width + originalX) + 1];
          r = rgb[4 * (originalLine * width + originalX) + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        }
      }
      else {
        for (size_t x = 0; x < outputWidth; x += 1) {
          size_t originalX = static_cast<size_t>((static_cast<double>(x) / outputWidth) * width);
          uint8_t b = rgb[4 * (originalLine * width + originalX)];
          uint8_t g = rgb[4 * (originalLine * width + originalX) + 1];
          uint8_t r = rgb[4 * (originalLine * width + originalX) + 2];

          m_buffer[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
        }
      }
    }
  }

  void reset()
  {
    m_stagingTexture.Reset();
    m_desktopTexture.Reset();
    m_duplication.Reset();
    m_context.Reset();
    m_device.Reset();

    m_width = m_height = 0;
    m_frameAcquired = false;

    init();
  }

private:
  Microsoft::WRL::ComPtr<ID3D11Device> m_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_duplication;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> m_desktopTexture;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;

  std::vector<uint8_t> m_buffer;
  std::vector<uint8_t> m_rescaleBuffer;
  bool m_frameAcquired{ false };

  size_t m_width{};
  size_t m_height{};
};


WindowCapturer::WindowCapturer()
  : m_impl(std::make_unique<AcceleratedWindowCapturer>())
{
}

WindowCapturer::~WindowCapturer()
{
}

Frame WindowCapturer::capture(size_t id, size_t width, size_t height)
{
  return m_impl->nextFrame(width, height);
}

}
