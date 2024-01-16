#include "GodotOpus.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

Opus *Opus::singleton = nullptr;
constexpr auto sampleFrames = 480;

void Opus::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("encode"), &Opus::encode);
	ClassDB::bind_method(D_METHOD("decode"), &Opus::decode);
}

Opus *Opus::get_singleton()
{
	return singleton;
}

Opus::Opus()
{
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	int err{};

	m_encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
	ERR_FAIL_COND(err != OPUS_OK);
	ERR_FAIL_COND(m_encoder == nullptr);

	m_decoder = opus_decoder_create(48000, 1, &err);
	ERR_FAIL_COND(err != OPUS_OK);
	ERR_FAIL_COND(m_decoder == nullptr);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
	ERR_FAIL_COND(err < 0);

	err = opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(24000));
	ERR_FAIL_COND(err < 0);
}

Opus::~Opus()
{
	ERR_FAIL_COND(singleton != this);
	opus_encoder_destroy(m_encoder);
	opus_decoder_destroy(m_decoder);
	singleton = nullptr;
}

PackedFloat32Array Opus::encode(PackedVector2Array input)
{
	if (input.size() < sampleFrames) {
		return {};
	}

	std::vector<float> data(sampleFrames);
	for (size_t i = 0; i < sampleFrames; i++) {
		data[i] = input[i].x;
	}

	std::vector<unsigned char> output(sampleFrames * 2);
	const auto r = opus_encode_float(m_encoder, data.data(), sampleFrames, output.data(), output.size());
	if (r == -1) {
		return {};
	}

	auto outputArray = PackedFloat32Array{};
	outputArray.resize(r);
	for (size_t i = 0; i < r; i++) {
		outputArray[i] = output[i];
	}

	return outputArray;
}

PackedVector2Array Opus::decode(PackedFloat32Array input)
{
	std::vector<unsigned char> inputData(sampleFrames*2);
	for (size_t i = 0; i < input.size(); i++) {
		inputData[i] = input[i];
	}

	std::vector<float> output(sampleFrames*2);
	const auto r = opus_decode_float(m_decoder, inputData.data(), input.size(), output.data(), sampleFrames, 0);
	if (r != sampleFrames) {
		return {};
	}

	auto packedOutput = PackedVector2Array{};
	packedOutput.resize(r);
	for (size_t i = 0; i < r; i++) {
		packedOutput[i] = Vector2{output[i], output[i]};
	}

	return packedOutput;
}

}
