#include <iostream>
#include <fstream>

#include <mapbox/geometry/geometry.hpp>
#include <mapbox/geometry/algorithms/closest_point.hpp>
#include <vtzero/vector_tile.hpp>
#include <deque>
#include <mapbox/variant.hpp>

#include "geometry_processors.hpp"
#include "reproject.hpp"

using variant_type = mapbox::util::variant<std::string, float, double, int64_t, uint64_t, bool>;

struct print_variant
{
    template <typename T>
    void operator()(T const& val) const
    {
        std::clog << val;
    }
};

// the main event
int main() {
  std::clog << "\n\nvtquery...\n";

  mapbox::geometry::point<std::int64_t> query_point{15,12};
  int distance_threshold = 5000;
  std::cout << "distance threshold: " << distance_threshold << "\n\n";
  std::vector<std::pair<vtzero::feature, double>> hits;
  std::vector<mapbox::geometry::point<std::int64_t>> results;

  // read buffer from filesystem
  std::string filename("../mvt-fixtures/fixtures/017/tile.mvt");
  std::ifstream stream(filename,std::ios_base::in|std::ios_base::binary);
  if (!stream.is_open())
  {
      throw std::runtime_error("could not open: '" + filename + "'");
  }
  std::string buffer((std::istreambuf_iterator<char>(stream.rdbuf())),
                      std::istreambuf_iterator<char>());
  stream.close();
  std::uint32_t tile_z = 0;
  std::uint32_t tile_x = 0;
  std::uint32_t tile_y = 0;

  // use vtzero to get geometry info
  vtzero::vector_tile tile{buffer};

  // storage mechanisms for features and layers
  std::deque<vtzero::layer> layers;
  std::vector<std::pair<vtzero::feature, mapbox::geometry::algorithms::closest_point_info<std::int64_t>>> features;

  // std::clog << "geometry type: " << static_cast<int> (feature.geometry_type()) << "\n";
  for (const auto layer : tile)
  {
    // storing layers to get properties afterwards
    // this is probably not the most efficient, but we're getting it working
    layers.emplace_back(layer);
    auto & layer_ref = layers.back();
    for (const auto feature : layer_ref)
    {

      // if we encounter an UNKNOWN geometry, skip the feature
      bool skip_feature = false;

      // create a dummy default geometry structure that will be updated in the switch statement below
      mapbox::geometry::geometry<std::int64_t> query_geometry = mapbox::geometry::point<std::int64_t>();
      // get the geometry type and decode the geometry into mapbox::geometry data structures
      switch (feature.geometry_type())
      {
        case vtzero::GeomType::POINT:
        {
          mapbox::geometry::multi_point<std::int64_t> mpoint;
          point_processor proc_point(mpoint);
          vtzero::decode_point_geometry(feature.geometry(), false, proc_point);
          query_geometry = std::move(mpoint);
          break;
        }
        case vtzero::GeomType::LINESTRING:
        {
          mapbox::geometry::multi_line_string<std::int64_t> mline;
          linestring_processor proc_line(mline);
          vtzero::decode_linestring_geometry(feature.geometry(), false, proc_line);
          query_geometry = std::move(mline);
          break;
        }
        case vtzero::GeomType::POLYGON:
        {
          mapbox::geometry::multi_polygon<std::int64_t> mpoly;
          polygon_processor proc_poly(mpoly);
          vtzero::decode_polygon_geometry(feature.geometry(), false, proc_poly);
          query_geometry = std::move(mpoly);
          break;
        }
        default:
        {
          skip_feature = true;
          break;
        }
      }

      if (!skip_feature)
      {
        // implement closest point algorithm on query geometry and the query point
        const auto cp_info = mapbox::geometry::algorithms::closest_point(query_geometry, query_point);

        // if the distance is within the threshold, save it
        if (cp_info.distance <= distance_threshold)
        {
          features.push_back(std::make_pair(feature, cp_info));
        }
      }
    }

    // if we have hits, decode properties for each hit, with a reference to the layer keys/values (included as a pointer in vtzero::feature)
  }

  // now decode properties
  std::clog << "\n---\ntotal features: " << features.size() << "\n";
  for (auto const& feature : features) {
    std::clog << "\n" << (&feature - &features[0]) << ")\n";
    std::clog << "x: " << feature.second.x << ", y: " << feature.second.y << ", distance: " << feature.second.distance << "\n";

    for (auto const prop : feature.first) {
      // get key as string

      std::string key = std::string{prop.key()};
      // get value as mapbox variant
      auto v = vtzero::convert_property_value<variant_type>(prop.value());
      // print them out
      std::clog << key << ": ";
      mapbox::util::apply_visitor(print_variant(),v);
      std::clog << "\n";

      // lng lat
      std::uint32_t extent = 4096; // TODO: pull from layer.extent()
      const auto ll = tile_to_long_lat(extent,tile_z, tile_x, tile_y, feature.second.x, feature.second.y);
      std::clog << "lng: " << ll.first << ", lat: " << ll.second << "\n";
    }
  }

  return 0;
}
