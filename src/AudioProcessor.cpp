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
	ClassDB::bind_method(D_METHOD("get_mix_rate"), &AudioProcessor::get_mix_rate);
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

    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_defaultDevice);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio device" << std::endl;
        m_deviceEnumerator->Release();
        return;
    }

    hr = m_defaultDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client" << std::endl;
        m_defaultDevice->Release();
        m_deviceEnumerator->Release();
        return;
    }

    hr = m_audioClient->GetMixFormat(&m_pwfx);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format" << std::endl;
        m_audioClient->Release();
        m_defaultDevice->Release();
        m_deviceEnumerator->Release();
        return;
    }
}

AudioProcessor::~AudioProcessor()
{
    CoTaskMemFree(m_pwfx);
    m_audioClient->Release();
    m_defaultDevice->Release();
    m_deviceEnumerator->Release();
    CoUninitialize();
}

void AudioProcessor::process()
{

}

size_t AudioProcessor::get_mix_rate() const
{
	return m_pwfx->nSamplesPerSec;
}

}
