// https://gist.github.com/springmeyer/4b540e124b2fb900b0da56bb9c96cde8

#include <cmath>

/*
 input: z/x/y tile address, x,y within tile
 returns: std::pair<x, y> in wgs84
*/

std::pair<double, double> tile_to_long_lat(std::uint32_t z, std::uint32_t x, std::uint32_t y, std::int32_t px, std::int32_t py) {
  int extent = 4096;
  double size = extent * std::pow(2.0, z);
  int x0 = extent * x;
  int y0 = extent * y;
  double y2 = 180.0 - (py + y0) * 360.0 / size;
  double x1 = (px + x0) * 360.0 / size - 180.0;
  double y1 = 360.0 / M_PI * std::atan(std::exp(y2 * M_PI / 180.0)) - 90.0;
  std::pair<double, double> ll(x1, y1);
  return ll;
}
