#include "GodotOpus.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void Opus::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("update_mix_rate"), &Opus::update_mix_rate);
	ClassDB::bind_method(D_METHOD("encode"), &Opus::encode);
	ClassDB::bind_method(D_METHOD("decode"), &Opus::decode);
	ClassDB::bind_method(D_METHOD("decode_and_play"), &Opus::decode_and_play);
}

Opus::Opus()
{
	int err{};

	// Opus
	m_encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &err);
	ERR_FAIL_COND(err != OPUS_OK);
	ERR_FAIL_COND(m_encoder == nullptr);

	m_decoder = opus_decoder_create(48000, 2, &err);
	ERR_FAIL_COND(err != OPUS_OK);
	ERR_FAIL_COND(m_decoder == nullptr);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
	ERR_FAIL_COND(err < 0);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
	ERR_FAIL_COND(err < 0);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(28000));
	ERR_FAIL_COND(err < 0);

	// Speex
	m_encodeResampler = speex_resampler_init(2, m_inputMixRate, 48000, 10, &err);
	ERR_FAIL_COND(err != 0);

	m_decodeResampler = speex_resampler_init(2, 48000, m_outputMixRate, 10, &err);
	ERR_FAIL_COND(err != 0);

	// Setup buffers
	m_encodeSampleBuffer.resize(SampleFrames);
	m_decodeSampleBuffer.resize(SampleFrames);
}

Opus::~Opus()
{
	opus_encoder_destroy(m_encoder);
	opus_decoder_destroy(m_decoder);
	speex_resampler_destroy(m_encodeResampler);
	speex_resampler_destroy(m_decodeResampler);
}

void Opus::update_mix_rate(size_t input, size_t output)
{
	int err{};
	m_inputMixRate = input;
	m_outputMixRate = output;

	speex_resampler_destroy(m_encodeResampler);
	m_encodeResampler = speex_resampler_init(2, m_inputMixRate, 48000, 10, &err);
	ERR_FAIL_COND(err != 0);

	speex_resampler_destroy(m_decodeResampler);
	m_decodeResampler = speex_resampler_init(2, 48000, m_outputMixRate, 10, &err);
	ERR_FAIL_COND(err != 0);

	/*
	UtilityFunctions::print(
		(std::string("Input encoder mix rate set to: ") + std::to_string(m_inputMixRate)).c_str()
	);
	UtilityFunctions::print(
		(std::string("Output encoder mix rate set to: ") + std::to_string(m_outputMixRate)).c_str()
	);
	*/
}

PackedByteArray Opus::encode(PackedVector2Array samples)
{
	PackedByteArray encoded;
	encoded.resize(sizeof(float) * SampleFrames * 2);

	unsigned int inlen = samples.size();
	unsigned int outlen = SampleFrames;

	speex_resampler_process_interleaved_float(
		m_encodeResampler,
		(float*) samples.ptr(),
		&inlen,
		(float*) m_encodeSampleBuffer.ptrw(),
		&outlen
	);

	const auto encodedSize = opus_encode_float(
		m_encoder,
		(float*) m_encodeSampleBuffer.ptr(),
		SampleFrames,
		(unsigned char*) encoded.ptrw(),
		encoded.size()
	);
	encoded.resize(encodedSize);

	return encoded;
}

PackedVector2Array Opus::decode(PackedByteArray encoded)
{
	PackedVector2Array output;
	output.resize(SampleFrames * m_outputMixRate / 48000);

	opus_decode_float(
		m_decoder,
		encoded.ptr(),
		encoded.size(),
		(float*) m_decodeSampleBuffer.ptrw(),
		SampleFrames,
		0
	);

	unsigned int inlen = m_decodeSampleBuffer.size();
	unsigned int outlen = output.size();

	speex_resampler_process_interleaved_float(
		m_decodeResampler,
		(float*) m_decodeSampleBuffer.ptr(),
		&inlen,
		(float*) output.ptrw(),
		&outlen
	);
	output.resize(outlen);

	return output;
}

void Opus::decode_and_play(Ref<AudioStreamGeneratorPlayback> buffer, PackedByteArray input)
{
	const auto decoded = decode(input);
	buffer->push_buffer(decoded);
}

}
