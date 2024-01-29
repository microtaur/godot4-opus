#include "AudioProcessor.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "Mmdevapi.lib")
#pragma comment(lib, "Uuid.lib")

namespace godot {

void AudioProcessor::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("process"), &AudioProcessor::process);
	ClassDB::bind_method(D_METHOD("get_output_mix_rate"), &AudioProcessor::get_output_mix_rate);
	ClassDB::bind_method(D_METHOD("get_input_mix_rate"), &AudioProcessor::get_input_mix_rate);
}

AudioProcessor::AudioProcessor()
{
  HRESULT hr;

  CoInitialize(NULL);
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_deviceEnumerator);
  if (FAILED(hr)) {
    std::cerr << "Failed to create device enumerator" << std::endl;
    return;
  }

  // Initialize default output device
  hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_defaultOutputDevice);
  if (FAILED(hr)) {
    std::cerr << "Failed to get default output audio device" << std::endl;
    return;
  }

  hr = m_defaultOutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_outputAudioClient);
  if (FAILED(hr)) {
    std::cerr << "Failed to activate output audio client" << std::endl;
    return;
  }

  hr = m_outputAudioClient->GetMixFormat(&m_outputMixFormat);
  if (FAILED(hr)) {
    std::cerr << "Failed to get output mix format" << std::endl;
    return;
  }

  // Initialize default input device
  hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &m_defaultInputDevice);
  if (FAILED(hr)) {
    std::cerr << "Failed to get default input audio device" << std::endl;
    return;
  }

  hr = m_defaultInputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_inputAudioClient);
  if (FAILED(hr)) {
    std::cerr << "Failed to activate input audio client" << std::endl;
    return;
  }

  hr = m_inputAudioClient->GetMixFormat(&m_inputMixFormat);
  if (FAILED(hr)) {
    std::cerr << "Failed to get input mix format" << std::endl;
    return;
  }
}

AudioProcessor::~AudioProcessor() {
  if (m_outputMixFormat != nullptr) {
    CoTaskMemFree(m_outputMixFormat);
    m_outputMixFormat = nullptr;
  }

  if (m_outputAudioClient != nullptr) {
    m_outputAudioClient->Release();
    m_outputAudioClient = nullptr;
  }

  if (m_defaultOutputDevice != nullptr) {
    m_defaultOutputDevice->Release();
    m_defaultOutputDevice = nullptr;
  }

  if (m_inputMixFormat != nullptr) {
    CoTaskMemFree(m_inputMixFormat);
    m_inputMixFormat = nullptr;
  }

  if (m_inputAudioClient != nullptr) {
    m_inputAudioClient->Release();
    m_inputAudioClient = nullptr;
  }

  if (m_defaultInputDevice != nullptr) {
    m_defaultInputDevice->Release();
    m_defaultInputDevice = nullptr;
  }

  if (m_deviceEnumerator != nullptr) {
    m_deviceEnumerator->Release();
    m_deviceEnumerator = nullptr;
  }

  CoUninitialize();
}

void AudioProcessor::process()
{

}

size_t AudioProcessor::get_output_mix_rate() const
{
	return m_outputMixFormat->nSamplesPerSec;
}

size_t AudioProcessor::get_input_mix_rate() const {
  return m_inputMixFormat->nSamplesPerSec;
}

}
