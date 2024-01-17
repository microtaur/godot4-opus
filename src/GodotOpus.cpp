#include "GodotOpus.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void Opus::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("encode"), &Opus::encode);
	ClassDB::bind_method(D_METHOD("decode"), &Opus::decode);
	ClassDB::bind_method(D_METHOD("decode_and_play"), &Opus::decode_and_play);
}

Opus::Opus()
{
	int err{};

	m_encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
	ERR_FAIL_COND(err != OPUS_OK);
	ERR_FAIL_COND(m_encoder == nullptr);

	m_decoder = opus_decoder_create(48000, 1, &err);
	ERR_FAIL_COND(err != OPUS_OK);
	ERR_FAIL_COND(m_decoder == nullptr);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
	ERR_FAIL_COND(err < 0);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
	ERR_FAIL_COND(err < 0);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(24000));
	ERR_FAIL_COND(err < 0);

	m_encodeInputBuffer.resize(SampleFrames);
	m_encodeOutputBuffer.resize(SampleFrames);
	m_decodeOutputBuffer.resize(SampleFrames);
}

Opus::~Opus()
{
	opus_encoder_destroy(m_encoder);
	opus_decoder_destroy(m_decoder);
}

PackedFloat32Array Opus::encode(PackedVector2Array input)
{
	if (input.size() < SampleFrames) {
		return {};
	}

	for (size_t i = 0; i < SampleFrames; i++) {
		m_encodeInputBuffer[i] = input[i].x;
	}

	const auto r = opus_encode_float(
		m_encoder,
		m_encodeInputBuffer.data(),
		SampleFrames,
		m_encodeOutputBuffer.data(),
		m_encodeOutputBuffer.size()
	);
	if (r == -1) {
		return {};
	}

	auto outputArray = PackedFloat32Array{};
	outputArray.resize(r);
	for (size_t i = 0; i < r; i++) {
		outputArray[i] = m_encodeOutputBuffer[i];
	}

	return outputArray;
}

PackedVector2Array Opus::decode(PackedFloat32Array input)
{
	std::vector<unsigned char> inputData(input.size());
	for (size_t i = 0; i < input.size(); i++) {
		inputData[i] = input[i];
	}

	const auto r = opus_decode_float(
		m_decoder,
		inputData.data(),
		input.size(),
		m_decodeOutputBuffer.data(),
		SampleFrames,
		0
	);
	if (r != SampleFrames) {
		return {};
	}

	auto packedOutput = PackedVector2Array{};
	packedOutput.resize(r);
	for (size_t i = 0; i < r; i++) {
		packedOutput[i] = Vector2{m_decodeOutputBuffer[i], m_decodeOutputBuffer[i]};
	}

	return packedOutput;
}

void Opus::decode_and_play(Ref<AudioStreamGeneratorPlayback> buffer, PackedFloat32Array input)
{
	const auto decoded = decode(input);
	buffer->push_buffer(decoded);
}

}
