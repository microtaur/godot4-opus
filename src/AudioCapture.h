#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>

namespace godot {

class AudioCapture : public Node
{
	GDCLASS(AudioCapture, Node);

protected:
	static void _bind_methods();

public:
	AudioCapture();
	~AudioCapture();

	PackedVector2Array getFrame() const;

	void setGain(float gain);

private:
	IMMDeviceEnumerator* m_deviceEnumerator{nullptr};
	IMMDevice* m_device{nullptr};
	IAudioClient* m_audioClient{nullptr};
	IAudioCaptureClient* m_captureClient{nullptr};

  Ref<AudioStreamGenerator> m_audioStream;
  Ref<AudioStreamGeneratorPlayback> m_audioPlayback;

	float m_gain{0.9};
};

}
