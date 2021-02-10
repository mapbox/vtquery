#pragma once

#include <mapbox/feature.hpp>
#include <vtzero/types.hpp>
#include <vtzero/vector_tile.hpp>
// stl
#include <map>

namespace mapbox {
namespace vector_tile {
namespace detail {
struct property_value_mapping {

    /// mapping for string type
    using string_type = std::string;

    /// mapping for float type
    using float_type = double;

    /// mapping for double type
    using double_type = double;

    /// mapping for int type
    using int_type = int64_t;

    /// mapping for uint type
    using uint_type = uint64_t;

    /// mapping for bool type
    using bool_type = bool;

}; // struct property_value_mapping

template <typename CoordinateType>
struct point_geometry_handler {

    using geom_type = mapbox::geometry::multi_point<CoordinateType>;

    geom_type& geom_;

    point_geometry_handler(geom_type& geom) : geom_(geom) {
    }

    void points_begin(std::uint32_t count) {
        geom_.reserve(count);
    }

    void points_point(const vtzero::point pt) {
        geom_.emplace_back(pt.x, pt.y);
    }

    void points_end() {
    }
};

template <typename CoordinateType>
mapbox::geometry::geometry<CoordinateType> extract_geometry_point(vtzero::feature const& f) {

    mapbox::geometry::multi_point<CoordinateType> mp;
    vtzero::decode_point_geometry(f.geometry(), detail::point_geometry_handler<CoordinateType>(mp));
    if (mp.empty()) {
        return mapbox::geometry::geometry<CoordinateType>();
        // throw std::runtime_error("Point feature has no points in its geometry");
    } else if (mp.size() == 1) {
        return mapbox::geometry::geometry<CoordinateType>(mp.front());
    } else {
        return mapbox::geometry::geometry<CoordinateType>(std::move(mp));
    }
}

template <typename CoordinateType>
struct line_string_geometry_handler {

    using geom_type = mapbox::geometry::multi_line_string<CoordinateType>;

    geom_type& geom_;

    line_string_geometry_handler(geom_type& geom) : geom_(geom) {
    }

    void linestring_begin(std::uint32_t count) {
        geom_.emplace_back();
        geom_.back().reserve(count);
    }

    void linestring_point(const vtzero::point pt) {
        geom_.back().emplace_back(pt.x, pt.y);
    }

    void linestring_end() {
    }
};

template <typename CoordinateType>
struct polygon_ring {

    polygon_ring() : ring(), type(vtzero::ring_type::invalid) {
    }

    mapbox::geometry::linear_ring<CoordinateType> ring;
    vtzero::ring_type type;
};

template <typename CoordinateType>
struct polygon_geometry_handler {

    using geom_type = std::vector<polygon_ring<CoordinateType>>;

    geom_type& geom_;

    polygon_geometry_handler(geom_type& geom) : geom_(geom) {
    }

    void ring_begin(std::uint32_t count) {
        geom_.emplace_back();
        geom_.back().ring.reserve(count);
    }

    void ring_point(const vtzero::point pt) {
        geom_.back().ring.emplace_back(pt.x, pt.y);
    }

    void ring_end(vtzero::ring_type type) {
        geom_.back().type = type;
    }
};

template <typename CoordinateType>
mapbox::geometry::geometry<CoordinateType> extract_geometry_polygon(vtzero::feature const& f) {

    std::vector<polygon_ring<CoordinateType>> rings;
    vtzero::decode_polygon_geometry(f.geometry(), detail::polygon_geometry_handler<CoordinateType>(rings));
    if (rings.empty()) {
        return mapbox::geometry::geometry<CoordinateType>();
        // throw std::runtime_error("Polygon feature has no rings in its geometry");
    }
    mapbox::geometry::multi_polygon<CoordinateType> mp;
    mp.reserve(rings.size());
    for (auto&& r : rings) {
        if (r.type == vtzero::ring_type::outer) {
            mp.emplace_back();
            mp.back().push_back(std::move(r.ring));
        } else if (!mp.empty() && r.type == vtzero::ring_type::inner) {
            mp.back().push_back(std::move(r.ring));
        }
    }
    if (mp.empty()) {
        return mapbox::geometry::geometry<CoordinateType>();
        // throw std::runtime_error("Polygon feature has no rings in its geometry");
    } else if (mp.size() == 1) {
        return mapbox::geometry::geometry<CoordinateType>(std::move(mp.front()));
    } else {
        return mapbox::geometry::geometry<CoordinateType>(std::move(mp));
    }
}

template <typename CoordinateType>
mapbox::geometry::geometry<CoordinateType> extract_geometry_line_string(vtzero::feature const& f) {

    mapbox::geometry::multi_line_string<CoordinateType> mls;
    vtzero::decode_linestring_geometry(f.geometry(), detail::line_string_geometry_handler<CoordinateType>(mls));
    if (mls.empty()) {
        return mapbox::geometry::geometry<CoordinateType>();
        // throw std::runtime_error("Line string feature has no points in its geometry");
    } else if (mls.size() == 1) {
        return mapbox::geometry::geometry<CoordinateType>(std::move(mls.front()));
    } else {
        return mapbox::geometry::geometry<CoordinateType>(std::move(mls));
    }
}

} // namespace detail

template <typename CoordinateType>
mapbox::geometry::geometry<CoordinateType> extract_geometry(vtzero::feature const& f) {
    switch (f.geometry_type()) {
    case vtzero::GeomType::POINT:
        return detail::extract_geometry_point<CoordinateType>(f);
    case vtzero::GeomType::LINESTRING:
        return detail::extract_geometry_line_string<CoordinateType>(f);
    case vtzero::GeomType::POLYGON:
        return detail::extract_geometry_polygon<CoordinateType>(f);
    default:
        return mapbox::geometry::geometry<CoordinateType>();
    }
}

inline mapbox::feature::property_map extract_properties(vtzero::feature const& f) {
    mapbox::feature::property_map map;
    f.for_each_property([&](vtzero::property&& p) {
        map.emplace(std::string(p.key()), vtzero::convert_property_value<mapbox::feature::value, detail::property_value_mapping>(p.value()));
        return true;
    });
    return map;
}

inline mapbox::feature::identifier extract_id(vtzero::feature const& f) {
    if (f.has_id()) {
        return mapbox::feature::identifier(f.id());
    } else {
        return mapbox::feature::identifier();
    }
}

template <typename CoordinateType>
mapbox::feature::feature<CoordinateType> extract_feature(vtzero::feature const& f) {
    return mapbox::feature::feature<CoordinateType>(extract_geometry<CoordinateType>(f), extract_properties(f), extract_id(f));
}

template <typename CoordinateType>
using layer_map = std::map<std::string, mapbox::feature::feature_collection<CoordinateType>>;

template <typename CoordinateType>
layer_map<CoordinateType> decode_tile(std::string const& buffer) {
    layer_map<CoordinateType> m;
    vtzero::vector_tile tile(buffer);
    while (auto layer = tile.next_layer()) {
        mapbox::feature::feature_collection<CoordinateType> fc;
        while (auto feature = layer.next_feature()) {
            auto f = extract_feature<CoordinateType>(feature);
            if (!f.geometry.template is<mapbox::geometry::empty>()) {
                fc.push_back(std::move(f));
            }
        }

        if (!fc.empty()) {
            m.emplace(std::string(layer.name()), std::move(fc));
        }
    }
    return m;
}
} // namespace vector_tile
} // namespace mapbox
