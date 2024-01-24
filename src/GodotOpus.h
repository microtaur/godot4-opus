#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>

#include "opus.h"
#include "speex/speex_resampler.h"

namespace godot {

constexpr size_t SampleFrames{480};

class Opus : public Node
{
	GDCLASS(Opus, Node);

protected:
	static void _bind_methods();

public:
	Opus();
	~Opus();

	void update_mix_rate(size_t input, size_t output);

	PackedByteArray encode(PackedVector2Array input);
	PackedVector2Array decode(PackedByteArray input);

	void decode_and_play(Ref<AudioStreamGeneratorPlayback> buffer, PackedByteArray input);

private:
	PackedVector2Array m_encodeSampleBuffer;
	PackedVector2Array m_decodeSampleBuffer;

	size_t m_outputMixRate{44100};
	size_t m_inputMixRate{44100};

	OpusEncoder* m_encoder;
	OpusDecoder* m_decoder;

	SpeexResamplerState* m_encodeResampler;
	SpeexResamplerState* m_decodeResampler;
};

}
