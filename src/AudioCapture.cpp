#include "AudioCapture.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void AudioCapture::_bind_methods()
{
  ClassDB::bind_method(D_METHOD("get_frame"), &AudioCapture::getFrame);
  ClassDB::bind_method(D_METHOD("get_gain"), &AudioCapture::setGain);
}

AudioCapture::AudioCapture()
{
  HRESULT hr;

  hr = CoInitialize(nullptr);
  if (FAILED(hr)) {
    UtilityFunctions::printerr("CoInitialize failed:", hr);
    return;
  }

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_deviceEnumerator));
  if (FAILED(hr)) {
    UtilityFunctions::printerr("CoCreateInstance failed:", hr);
    return;
  }

  hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
  if (FAILED(hr)) {
    UtilityFunctions::printerr("GetDefaultAudioEndpoint failed:", hr);
    return;
  }

  hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
  if (FAILED(hr)) {
    UtilityFunctions::printerr("Device activate failed:", hr);
    return;
  }

  WAVEFORMATEX* pwfx{nullptr};
  hr = m_audioClient->GetMixFormat(&pwfx);
  if (FAILED(hr)) {
    UtilityFunctions::printerr("GetMixFormat failed:", hr);
    return;
  }

  hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, pwfx, nullptr);
  if (FAILED(hr)) {
    UtilityFunctions::printerr("Device initialize failed:", hr);
    return;
  };

  hr = m_audioClient->GetService(IID_PPV_ARGS(&m_captureClient));
  if (FAILED(hr)) {
    UtilityFunctions::printerr("Get service failed:", hr);
    return;
  }

  hr = m_audioClient->Start();
  if (FAILED(hr)) {
    UtilityFunctions::printerr("Start failed:", hr);
    return;
  };
}

AudioCapture::~AudioCapture()
{
  if (m_captureClient) {
    m_captureClient->Release();
  }

  if (m_audioClient) {
    m_audioClient->Release();
  }

  if (m_device) {
    m_device->Release();
  }

  if (m_deviceEnumerator) {
    m_deviceEnumerator->Release();
  }

  CoUninitialize();
}


PackedVector2Array AudioCapture::getFrame() const
{
  PackedVector2Array out{};

  UINT32 len = 0;
  UINT32 frames = 0;
  DWORD flags = 0;

  m_captureClient->GetNextPacketSize(&len);

  if (len > 0) {
    BYTE* pData = nullptr;
    m_captureClient->GetBuffer(&pData, &frames, &flags, NULL, NULL);

    if (pData && frames > 0) {
      float* pfData = (float*)pData;
      for (UINT32 i = 0; i < frames; ++i) {
        float leftChannel = pfData[i * 2];
        float rightChannel = pfData[i * 2 + 1];
        out.push_back(Vector2(leftChannel * m_gain, rightChannel * m_gain));
      }
    }

    m_captureClient->ReleaseBuffer(frames);
    m_captureClient->GetNextPacketSize(&len);
  }

  return out;
}

void AudioCapture::setGain(float gain)
{
  m_gain = gain;
}

}
