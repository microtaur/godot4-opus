#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>

#include "opus.h"

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

	PackedFloat32Array encode(PackedVector2Array input);
	PackedVector2Array decode(PackedFloat32Array input);

	void decode_and_play(Ref<AudioStreamGeneratorPlayback> buffer, PackedFloat32Array input);

private:
	std::vector<float> m_encodeInputBuffer;
	std::vector<float> m_decodeOutputBuffer;
	std::vector<unsigned char> m_encodeOutputBuffer;

	OpusEncoder* m_encoder;
	OpusDecoder* m_decoder;

};

}
