#pragma once
#include <stdint.h>
#include <vector>

namespace microtaur {

struct Frame
{
  size_t width;
  size_t height;
  std::vector<uint8_t> data;
};

class WindowCapturer
{
public:
  Frame capture(size_t id = 0);
};

}
