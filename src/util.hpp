#pragma once
#include <cmath>
#include <mapbox/cheap_ruler.hpp>
#include <mapbox/geometry/algorithms/closest_point.hpp>
#include <mapbox/geometry/geometry.hpp>
#include <mapbox/variant.hpp>
#include <napi.h>
#include <vtzero/types.hpp>
#include <vtzero/vector_tile.hpp>

namespace utils {

inline Napi::Value CallbackError(std::string const& message, Napi::CallbackInfo const& info) {
    Napi::Object obj = Napi::Object::New(info.Env());
    obj.Set("message", message);
    auto func = info[info.Length() - 1].As<Napi::Function>();
    // ^^^ here we assume that info has a valid callback function
    // TODO: consider changing either method signature or adding internal checks
    return func.Call({obj});
}

/*
  Convert original lng/lat coordinates into a query point relative to the "active" tile in vector tile coordinates
  Returns a geometry.hpp point with std::int64_t values
*/
mapbox::geometry::point<std::int64_t> create_query_point(double lng,
                                                         double lat,
                                                         std::uint32_t extent,
                                                         std::int32_t active_tile_z,
                                                         std::int32_t active_tile_x,
                                                         std::int32_t active_tile_y) {

    lng = std::fmod((lng + 180.0), 360.0);
    if (lat > 89.9) {
        lat = 89.9;
    } else if (lat < -89.9) {
        lat = -89.9;
    }

    double z2 = static_cast<double>(1 << active_tile_z); // number of tiles 'across' a particular zoom level
    double lat_radian = (lat * M_PI) / 180.0;
    std::int64_t zl_x = static_cast<std::int64_t>(lng / (360.0 / (extent * z2)));
    std::int64_t zl_y = static_cast<std::int64_t>(((extent * z2) / 2.0) * (1.0 - (std::log(std::tan(lat_radian) + 1.0 / std::cos(lat_radian)) / M_PI)));
    std::int64_t origin_tile_x = zl_x / extent;
    std::int64_t origin_tile_y = zl_y / extent;
    std::int64_t origin_x = zl_x % extent;
    std::int64_t origin_y = zl_y % extent;
    std::int64_t diff_tile_x = active_tile_x - origin_tile_x;
    std::int64_t diff_tile_y = active_tile_y - origin_tile_y;
    std::int64_t query_x = origin_x - (diff_tile_x * extent);
    std::int64_t query_y = origin_y - (diff_tile_y * extent);
    return mapbox::geometry::point<std::int64_t>{query_x, query_y};
}

/*
  Create a geometry.hpp point from vector tile coordinates
*/
mapbox::geometry::point<double> convert_vt_to_ll(std::uint32_t extent,
                                                 std::int32_t z,
                                                 std::int32_t x,
                                                 std::int32_t y,
                                                 mapbox::geometry::algorithms::closest_point_info cp_info) {
    double z2 = static_cast<double>(static_cast<std::int64_t>(1) << z);
    double ex = static_cast<double>(extent);
    double size = ex * z2;
    double x0 = ex * x;
    double y0 = ex * y;
    double y2 = 180.0 - (static_cast<double>(cp_info.y) + y0) * 360.0 / size;
    double x1 = (static_cast<double>(cp_info.x) + x0) * 360.0 / size - 180.0;
    double y1 = 360.0 / M_PI * std::atan(std::exp(y2 * M_PI / 180.0)) - 90.0;
    return mapbox::geometry::point<double>{x1, y1};
}

/*
  Get the distance (in meters) between two geometry.hpp points using cheap-ruler
  https://github.com/mapbox/cheap-ruler-cpp

  The first point is considered the "origin" and its latitude is used to initialize
  the ruler. The second is considered the "feature" and is the distance to.
*/
double distance_in_meters(mapbox::geometry::point<double> const& origin_lnglat, mapbox::geometry::point<double> const& feature_lnglat) {
    // set up cheap ruler with query latitude
    mapbox::cheap_ruler::CheapRuler ruler(origin_lnglat.y, mapbox::cheap_ruler::CheapRuler::Meters);
    return ruler.distance(origin_lnglat, feature_lnglat);
}
} // namespace utils
