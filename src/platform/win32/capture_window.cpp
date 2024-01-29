#include "capture_window.h"

#include "Windows.h"

#include <chrono>

namespace microtaur
{

namespace
{
struct Bitmap {
  HBITMAP bmp;
  int width;
  int height;
};

struct MonitorEnumData {
  int targetMonitorIndex;
  int currentMonitorIndex;
  HMONITOR hMonitor;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
  MonitorEnumData* pData = reinterpret_cast<MonitorEnumData*>(dwData);
  if (pData->currentMonitorIndex == pData->targetMonitorIndex) {
    pData->hMonitor = hMonitor;
    return FALSE;
  }
  pData->currentMonitorIndex++;
  return TRUE;
}

Bitmap CaptureScreen(int screenId) {
  MonitorEnumData med;
  med.targetMonitorIndex = screenId;
  med.currentMonitorIndex = 0;
  med.hMonitor = nullptr;

  EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&med));

  if (med.hMonitor == nullptr) {
    // No monitor found with the given ID
    return { NULL, 0, 0 };
  }

  MONITORINFOEX mi;
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(med.hMonitor, &mi);

  HDC hMonitorDC = CreateDC(TEXT("DISPLAY"), mi.szDevice, NULL, NULL);
  HDC hMemoryDC = CreateCompatibleDC(hMonitorDC);

  int width = mi.rcMonitor.right - mi.rcMonitor.left;
  int height = mi.rcMonitor.bottom - mi.rcMonitor.top;

  HBITMAP hBitmap = CreateCompatibleBitmap(hMonitorDC, width, height);
  HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hBitmap));
  BitBlt(hMemoryDC, 0, 0, width, height, hMonitorDC, 0, 0, SRCCOPY);
  SelectObject(hMemoryDC, hOldBitmap);

  DeleteDC(hMemoryDC);
  DeleteDC(hMonitorDC);

  return { hBitmap, width, height };
}


void RGBtoYUV(BYTE R, BYTE G, BYTE B, BYTE& Y, BYTE& U, BYTE& V) {
  int yTemp = 0.299 * R + 0.587 * G + 0.114 * B;
  int uTemp = -0.14713 * R - 0.28886 * G + 0.436 * B + 128;
  int vTemp = 0.615 * R - 0.51498 * G - 0.10001 * B + 128;

  Y = static_cast<BYTE>(std::max(0, std::min(255, yTemp)));
  U = static_cast<BYTE>(std::max(0, std::min(255, uTemp)));
  V = static_cast<BYTE>(std::max(0, std::min(255, vTemp)));
}


// Function to create YUV frame_data from HBITMAP
std::vector<uint8_t> CreateYUVFrameFromHBITMAP(HBITMAP hBitmap, int width, int height) {
  // Calculate the size for YUV 4:2:0 format
  std::vector<uint8_t> data(width * height + (width * height) / 4 + (width * height) / 4);

  HDC hdcScreen = GetDC(NULL);
  HDC hdcMem = CreateCompatibleDC(hdcScreen);

  BITMAPINFOHEADER bi;
  memset(&bi, 0, sizeof(bi));
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = width;
  bi.biHeight = -height; // Negative height for top-down bitmap
  bi.biPlanes = 1;
  bi.biBitCount = 24; // Assuming RGB 24-bit format
  bi.biCompression = BI_RGB;

  // First call to GetDIBits to populate biSizeImage
  GetDIBits(hdcMem, hBitmap, 0, height, NULL, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
  BYTE* rgbData = new BYTE[bi.biSizeImage];

  // Second call to GetDIBits to get the actual bitmap data
  GetDIBits(hdcMem, hBitmap, 0, height, rgbData, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

  // Calculate the stride for the bitmap
  int stride = ((width * bi.biBitCount + 31) / 32) * 4; // Bitmap scanline padding

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // Correct index with stride
      int i = (y * stride) + (x * 3);
      BYTE B = rgbData[i];
      BYTE G = rgbData[i + 1];
      BYTE R = rgbData[i + 2];

      BYTE Y, U, V;
      RGBtoYUV(R, G, B, Y, U, V);

      data.data()[y * width + x] = Y;

      // Correct subsampling for U and V components
      if (x % 2 == 0 && y % 2 == 0) {
        int uvIndex = (y / 2) * (width / 2) + (x / 2);
        data.data()[width * height + uvIndex] = U; // U plane
        data.data()[width * height + (width * height / 4) + uvIndex] = V; // V plane
      }
    }
  }

  // Clean up resources
  DeleteDC(hdcMem);
  ReleaseDC(NULL, hdcScreen);
  delete[] rgbData;

  return data;
}


}

Frame WindowCapturer::capture(size_t id)
{
  const auto start = std::chrono::high_resolution_clock::now();

  auto bitmap = CaptureScreen(id);
  auto out = Frame{
    static_cast<size_t>(bitmap.width),
    static_cast<size_t>(bitmap.height),
    CreateYUVFrameFromHBITMAP(bitmap.bmp, bitmap.width, bitmap.height)
  };

  DeleteObject(bitmap.bmp);

  return out;
}

}
