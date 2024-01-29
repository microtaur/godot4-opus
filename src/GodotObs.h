#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>

#include <vpx/vpx_encoder.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_codec.h>

namespace godot {

class Obs : public Node
{
	GDCLASS(Obs, Node);

protected:
	static void _bind_methods();

public:
	Obs();
	~Obs();

	PackedByteArray getEncodedScreenFrame(size_t id);
	void renderFrameToMesh(PackedByteArray frame, Ref<StandardMaterial3D> mat);

private:
	bool m_initialized{false};
	bool m_decInitialized{false};

	vpx_codec_enc_cfg_t m_cfg;
	vpx_codec_ctx_t m_encoder{};
	vpx_codec_ctx_t m_decoder{};

	vpx_codec_pts_t m_pts{1};
	vpx_codec_pts_t m_duration{1};

	Ref<Image> m_imageBuffer{};
	Ref<ImageTexture> m_imageTexture{};

	size_t m_avgEncodeTime{};
	size_t m_encodeCount{};
	size_t m_totalEncodeTime{};

	size_t m_avgDecodeTime{};
	size_t m_decodeCount{};
	size_t m_totalDecodeTime{};
};

}
