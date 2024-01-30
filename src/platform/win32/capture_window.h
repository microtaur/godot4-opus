#pragma once
#include <stdint.h>
#include <vector>
#include <memory>

namespace microtaur {

struct Frame
{
  size_t width;
  size_t height;
  std::vector<uint8_t> data;
};

class AcceleratedWindowCapturer;

class WindowCapturer
{
public:
  WindowCapturer();
  ~WindowCapturer();

  Frame capture(size_t id = 0);

private:
  std::unique_ptr<AcceleratedWindowCapturer> m_impl;

};

}
