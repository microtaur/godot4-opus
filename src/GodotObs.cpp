#include "GodotObs.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <Windows.h>
#include <chrono>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

//#define PROFILER_ENABLED

namespace godot {

namespace {

inline void YUVToRGB(int Y, int U, int V, BYTE& R, BYTE& G, BYTE& B) {
  R = std::max(0, std::min(255, static_cast<int>(Y + 1.402 * (V - 128))));
  G = std::max(0, std::min(255, static_cast<int>(Y - 0.344136 * (U - 128) - 0.714136 * (V - 128))));
  B = std::max(0, std::min(255, static_cast<int>(Y + 1.772 * (U - 128))));
}

inline void ConvertToRGB420(unsigned char* planes[4], int stride[4], godot::Ref<godot::Image> m_imageBuffer, int width, int height) {
  constexpr float inv_255 = 1.0f / 255.0f;
  auto data = m_imageBuffer->get_data().ptr();

  for (int y = 0; y < height; ++y) {
    int uv_row = (y / 2) * stride[VPX_PLANE_U];
    for (int x = 0; x < width; ++x) {
      int Y = planes[VPX_PLANE_Y][y * stride[VPX_PLANE_Y] + x];
      int U = planes[VPX_PLANE_U][uv_row + (x / 2)];
      int V = planes[VPX_PLANE_V][uv_row + (x / 2)];

      BYTE R, G, B;
      YUVToRGB(Y, U, V, R, G, B);

      const auto index = (y * width + x) * 3;
      *(uint8_t*)&data[index] = R;
      *(uint8_t*)&data[index+1] = G;
      *(uint8_t*)&data[index+2] = B;
    }
  }
}

}


void Obs::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("get_screen_frame"), &Obs::getEncodedScreenFrame);
  ClassDB::bind_method(D_METHOD("render_frame"), &Obs::renderFrameToMesh);
}


Obs::Obs()
{
  vpx_codec_err_t res{};

  // encoder initialize
  res = vpx_codec_enc_config_default(vpx_codec_vp9_cx(), &m_cfg, 0);
  ERR_FAIL_COND_MSG(res != VPX_CODEC_OK, "VP9 cfg fail");

  m_cfg.g_w = 1920;
  m_cfg.g_h = 1080;
  m_cfg.rc_target_bitrate = 1500;
  m_cfg.g_timebase.num = 1000;
  m_cfg.g_timebase.den = 30001;
  m_cfg.g_lag_in_frames = 0;
  m_cfg.g_threads = 8;
  //m_cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT;
  m_cfg.g_pass = VPX_RC_ONE_PASS; // TODO: consider doing two-pass
  m_cfg.kf_max_dist = 1; // Currently necessary because of WebRTC buffer limitations
  m_cfg.kf_min_dist = 1; // Maybe we should split data that being sent through rpc?

  res = vpx_codec_enc_init(&m_encoder, vpx_codec_vp9_cx(), &m_cfg, 0);
  ERR_FAIL_COND_MSG(res != VPX_CODEC_OK, "VP9 could not be initialized");

  vpx_codec_control(&m_encoder, VP8E_SET_CPUUSED, 13);

  res = vpx_codec_dec_init(&m_decoder, vpx_codec_vp9_dx(), nullptr, 0);
  ERR_FAIL_COND_MSG(res != VPX_CODEC_OK, "Could not intitialize decoder");

  m_imageBuffer = Image::create(1920, 1080, false, godot::Image::FORMAT_RGB8); // TODO: adjust resolution
}

Obs::~Obs()
{
  // TODO: cleanup
}


PackedByteArray Obs::getEncodedScreenFrame(size_t id)
{
#ifdef PROFILER_ENABLED
  const auto start = std::chrono::high_resolution_clock::now();
#endif

  vpx_codec_err_t res{};
  ERR_FAIL_COND_V_MSG(!&m_encoder, {}, "VP9 encoder is not initialized");

  auto frame = m_capturer.capture(id);
  if (frame.data.empty()) {
    return {};
  }

  vpx_image_t img;
  vpx_img_wrap(&img, VPX_IMG_FMT_I420, 1920, 1080, 1, frame.data.data());

  res = vpx_codec_encode(&m_encoder, &img, 0, 1, 0, VPX_DL_REALTIME);
  ERR_FAIL_COND_V_MSG(res != VPX_CODEC_OK, {}, "Could not encode frame");

  vpx_codec_iter_t iter{nullptr};
  const vpx_codec_cx_pkt_t* pkt;

  PackedByteArray out;
  while ((pkt = vpx_codec_get_cx_data(&m_encoder, &iter))) {
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      out.resize(pkt->data.frame.sz);
      std::memcpy(&out[0], pkt->data.frame.buf, pkt->data.frame.sz);

      m_pts += pkt->data.frame.pts;
      m_duration += pkt->data.frame.duration;
    }
  }

#ifdef PROFILER_ENABLED
  auto end = std::chrono::high_resolution_clock::now();
  auto encodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  m_totalEncodeTime += encodeTime;
  m_encodeCount++;
  m_avgEncodeTime = static_cast<double>(m_totalEncodeTime) / m_encodeCount;
  if (m_encodeCount % 10 == 0) {
    UtilityFunctions::print("Average Encode time: ", m_avgEncodeTime);
  }
#endif

  return out;
}

void Obs::renderFrameToMesh(PackedByteArray frame, Ref<StandardMaterial3D> mat)
{
  const auto start = std::chrono::high_resolution_clock::now();

  vpx_codec_err_t res{};
  res = vpx_codec_decode(&m_decoder, (const uint8_t*)&frame[0], frame.size(), NULL, 0);
  if (res != VPX_CODEC_OK) {
    return;
  }

  vpx_codec_iter_t iter = NULL;
  vpx_image_t* img = NULL;
  while ((img = vpx_codec_get_frame(&m_decoder, &iter)) != NULL) {
    ConvertToRGB420(img->planes, img->stride, m_imageBuffer, 1920, 1080);

    if (m_imageTexture.is_null()) {
      m_imageTexture = ImageTexture::create_from_image(m_imageBuffer);
    }
    else {
      m_imageTexture->update(m_imageBuffer);
    }

    mat->set_texture(godot::StandardMaterial3D::TEXTURE_ALBEDO, m_imageTexture);
  }

#ifdef PROFILER_ENABLED
  auto end = std::chrono::high_resolution_clock::now();
  auto decodeTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  m_totalDecodeTime += decodeTime;
  m_decodeCount++;
  m_avgDecodeTime = static_cast<double>(m_totalDecodeTime) / m_decodeCount;
  if (m_decodeCount % 10 == 0) {
    UtilityFunctions::print("Average Decode time: ", m_avgDecodeTime);
  }
#endif
}

}
