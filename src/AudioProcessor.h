#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <mmdeviceapi.h>
#include <Audioclient.h>

#include "opus.h"

namespace godot {

class AudioProcessor : public Node
{
	GDCLASS(AudioProcessor, Node);

protected:
	static void _bind_methods();

public:
	AudioProcessor();
	~AudioProcessor();

	void process();

	size_t get_input_mix_rate() const;
	size_t get_output_mix_rate() const;

private:
	IMMDeviceEnumerator* m_deviceEnumerator = nullptr;
	IMMDevice* m_defaultOutputDevice = nullptr;
	IAudioClient* m_outputAudioClient = nullptr;
	WAVEFORMATEX* m_outputMixFormat = nullptr;

	IMMDevice* m_defaultInputDevice = nullptr;
	IAudioClient* m_inputAudioClient = nullptr;
	WAVEFORMATEX* m_inputMixFormat = nullptr;
};

}
