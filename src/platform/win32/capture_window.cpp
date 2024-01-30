#include "capture_window.h"
#include <godot_cpp/variant/utility_functions.hpp>

#include <chrono>

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

  Frame nextFrame()
  {
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    if (m_frameAcquired) {
      m_duplication->ReleaseFrame();
      m_frameAcquired = false;
    }

    HRESULT hr = m_duplication->AcquireNextFrame(INFINITE, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
      if (hr == DXGI_ERROR_ACCESS_LOST) {
        // TODO
      }

      return {};
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

    // Convert BGRA to YUV420
    const auto yuvSize = m_width * m_height * 3 / 2;
    if (m_buffer.size() != yuvSize) {
      m_buffer.resize(yuvSize);
    }

    rgbToYuv(mappedResource, m_width, m_height);

    // Unmap and release the staging texture
    m_context->Unmap(m_stagingTexture.Get(), 0);

    return {m_width, m_height, m_buffer};
  }

  void rgbToYuv(D3D11_MAPPED_SUBRESOURCE mappedResource, size_t width, size_t height)
  {
    auto srcPtr = static_cast<uint8_t*>(mappedResource.pData);

    // Lambda functions for YUV conversion
    auto rgbToY = [](uint8_t r, uint8_t g, uint8_t b) -> uint8_t {
      return static_cast<uint8_t>((0.299 * r) + (0.587 * g) + (0.114 * b));
      };

    auto rgbToU = [](uint8_t r, uint8_t g, uint8_t b, int& sumU) -> uint8_t {
      sumU += (128 - (0.168736 * r) - (0.331264 * g) + (0.5 * b));
      return 0;  // Placeholder, real value computed in averaging step
      };

    auto rgbToV = [](uint8_t r, uint8_t g, uint8_t b, int& sumV) -> uint8_t {
      sumV += (128 + (0.5 * r) - (0.418688 * g) - (0.081312 * b));
      return 0;  // Placeholder, real value computed in averaging step
      };

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int srcIndex = (y * mappedResource.RowPitch) + (x * 4); // 4 bytes per pixel in source (BGRA)
        uint8_t b = srcPtr[srcIndex];
        uint8_t g = srcPtr[srcIndex + 1];
        uint8_t r = srcPtr[srcIndex + 2];

        // Set Y value
        m_buffer[y * width + x] = rgbToY(r, g, b);

        // Compute and average U and V values for 2x2 blocks
        if (x % 2 == 0 && y % 2 == 0) {
          int sumU = 0, sumV = 0;
          for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
              if ((y + dy) < height && (x + dx) < width) {
                int i = (y + dy) * width + (x + dx);
                rgbToU(r, g, b, sumU);
                rgbToV(r, g, b, sumV);
              }
            }
          }

          int uvIndex = width * height + (y / 2) * (width / 2) + (x / 2);
          m_buffer[uvIndex] = sumU / 4; // Average U
          m_buffer[uvIndex + width * height / 4] = sumV / 4; // Average V
        }
      }
    }
  }

  void reset()
  {
    // TODO: cleanup
  }

private:
  Microsoft::WRL::ComPtr<ID3D11Device> m_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_duplication;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> m_desktopTexture;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;

  std::vector<uint8_t> m_buffer;
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

Frame WindowCapturer::capture(size_t id)
{
  return m_impl->nextFrame();
}

}
