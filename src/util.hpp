#pragma once
#include <cmath>
#include <iostream>
#include <mapbox/cheap_ruler.hpp>
#include <mapbox/geometry/geometry.hpp>
#include <mapbox/variant.hpp>
#include <nan.h>

namespace utils {

/*
* This is an internal function used to return callback error messages instead of
* throwing errors.
* Usage:
*
* v8::Local<v8::Function> callback;
* return CallbackError("error message", callback);  // "return" is important to
* prevent duplicate callbacks from being fired!
*
*
* "inline" is important here as well. See for more contex:
* - https://github.com/mapbox/node-cpp-skel/pull/52#discussion_r126847394 for
* context
* - https://github.com/mapbox/cpp/pull/29 ...eventually point this to glossary
* when it merges
*
*/
inline void CallbackError(std::string message,
                          v8::Local<v8::Function> callback) {
    v8::Local<v8::Value> argv[1] = {Nan::Error(message.c_str())};
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), callback, 1, argv);
}

/*
* Print variant types
*/
using variant_type = mapbox::util::variant<std::string, float, double, int64_t, uint64_t, bool>;
struct print_variant {
    template <typename T>
    void operator()(T const& val) const {
        std::clog << val;
    }
};

/*
  Convert original lng/lat coordinates into a query point relative to the "active" tile in vector tile coordinates
  Returns a geometry.hpp point with std::int64_t values
*/
mapbox::geometry::point<std::int64_t> create_query_point(double lng,
                                                         double lat,
                                                         std::int32_t zoom,
                                                         std::uint32_t extent,
                                                         std::uint32_t active_tile_x,
                                                         std::uint32_t active_tile_y) {
    std::uint32_t z2 = 1 << zoom; // number of tiles 'across' a particular zoom level

    lng = fmod((lng + 180.0), 360.0);
    if (lat > 89.9) {
        lat = 89.9;
    } else if (lat < -89.9) {
        lat = -89.9;
    }
    double lat_radian = (lat * M_PI) / 180.0;
    double zl_x = lng / (360.0 / (extent * z2));
    double zl_y = ((extent * z2) / 2) * (1.0 - (log(tan(lat_radian) + 1.0 / cos(lat_radian)) / M_PI));
    std::clog << "zl_x: " << zl_x << ", zl_y: " << zl_y << std::endl;

    std::uint64_t origin_tile_x = static_cast<std::uint64_t>(floor(zl_x / extent));
    std::uint64_t origin_tile_y = static_cast<std::uint64_t>(floor(zl_y / extent));
    std::clog << "origin tile x: " << origin_tile_x << ", origin tile y: " << origin_tile_y << std::endl;
    std::clog << "active tile x: " << active_tile_x << ", active tile y: " << active_tile_y << std::endl;

    std::uint64_t origin_x = std::uint32_t(fmod(floor(zl_x), extent));
    std::uint64_t origin_y = std::uint32_t(fmod(floor(zl_y), extent));
    std::clog << "origin tile coordinate x: " << origin_x << ", origin tile coordinate y: " << origin_y << std::endl;

    auto diff_tile_x = active_tile_x - origin_tile_x;
    auto diff_tile_y = active_tile_y - origin_tile_y;
    std::clog << "tile difference x: " << diff_tile_x << ", y: " << diff_tile_y << std::endl;
    auto query_x = -(diff_tile_x * extent) + origin_x;
    auto query_y = -(diff_tile_y * extent) + origin_y;
    std::clog << "query point x: " << query_x << ", y: " << query_y << std::endl;
    mapbox::geometry::point<std::int64_t> query_point{static_cast<std::int64_t>(query_x), static_cast<std::int64_t>(query_y)};
    return query_point;
}

/*
 input: z/x/y tile address, x,y within tile
 returns: std::pair<x, y> in wgs84
*/
template <typename CoordinateType>
std::pair<double, double> convert_vt_to_ll(std::uint32_t extent,
                                           std::uint32_t z,
                                           std::uint32_t x,
                                           std::uint32_t y,
                                           CoordinateType px,
                                           CoordinateType py) {
    double size = extent * std::pow(2.0, z);
    double x0 = static_cast<double>(extent) * x;
    double y0 = static_cast<double>(extent) * y;
    double y2 = 180.0 - (py + static_cast<double>(y0)) * 360.0 / size;
    double x1 = (static_cast<double>(px) + x0) * 360.0 / size - 180.0;
    double y1 = 360.0 / M_PI * std::atan(std::exp(y2 * M_PI / 180.0)) - 90.0;
    std::pair<double, double> ll(x1, y1);
    // return a geometry.hpp point instead of a pair
    // mapbox::geometry::point<std::int64_t> query_point{10,15};
    return ll;
}

/*
  Get the distance (in meters) between two geometry.hpp points using cheap-ruler
  https://github.com/mapbox/cheap-ruler-cpp

  The first point is considered the "origin" and its latitude is used to initialize
  the ruler. The second is considered the "feature" and is the distance to.
*/
namespace cr = mapbox::cheap_ruler;
double distance_in_meters(cr::point origin_lnglat, cr::point feature_lnglat) {
    // set up cheap ruler with query latitude
    cr::CheapRuler ruler(origin_lnglat.y, cr::CheapRuler::Meters);
    auto d = ruler.distance(origin_lnglat, feature_lnglat);
    return d;
}
}
