// https://gist.github.com/springmeyer/4b540e124b2fb900b0da56bb9c96cde8

#include <cmath>

/*
 input: z/x/y tile address, x,y within tile
 returns: std::pair<x, y> in wgs84
*/

template<typename CoordinateType>
std::pair<double, double> tile_to_long_lat(std::uint32_t extent, std::uint32_t z, std::uint32_t x, std::uint32_t y, CoordinateType px, CoordinateType py) {
  double size = extent * std::pow(2.0, z);
  double x0 = static_cast<double>(extent) * x;
  double y0 = static_cast<double>(extent) * y;
  double y2 = 180.0 - (py + static_cast<double>(y0)) * 360.0 / size;
  double x1 = (static_cast<double>(px) + x0) * 360.0 / size - 180.0;
  double y1 = 360.0 / M_PI * std::atan(std::exp(y2 * M_PI / 180.0)) - 90.0;
  std::pair<double, double> ll(x1, y1);
  return ll;
}
