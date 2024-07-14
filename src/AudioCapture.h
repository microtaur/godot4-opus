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

	Array getFrame() const;

	void process();
  void start(Ref<AudioStreamGeneratorPlayback> playback);

private:
	IAudioClient* m_audioClient = nullptr;
	IAudioCaptureClient* m_captureClient = nullptr;

  Ref<AudioStreamGenerator> m_audioStream;
  Ref<AudioStreamGeneratorPlayback> m_audioPlayback;


};

}
