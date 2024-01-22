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
	size_t get_mix_rate() const;

private:
    IMMDeviceEnumerator *m_deviceEnumerator{};
    IMMDevice *m_defaultDevice{};
    IAudioClient *m_audioClient{};
    WAVEFORMATEX *m_pwfx{};
};

}
