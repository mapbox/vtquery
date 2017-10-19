#include <mapbox/geometry/geometry.hpp>
#include <vtzero/types.hpp>
#include <vtzero/vector_tile.hpp>

// convert the point geometry from vtzero into mapbox geometry point structure
// https://gist.github.com/artemp/cfe855cbaf0cbc277ce80b5e768e7d0b#file-gistfile1-txt-L24
struct point_processor {
    point_processor(mapbox::geometry::multi_point<std::int64_t>& mpoint)
        : mpoint_(mpoint) {}

    void points_begin(uint32_t count) const {
        if (count > 1) mpoint_.reserve(count);
    }

    void points_point(vtzero::point const& point) const {
        mpoint_.emplace_back(point.x, 4096.0 - point.y);
    }

    void points_end() const {
        //
    }
    mapbox::geometry::multi_point<std::int64_t>& mpoint_;
};

struct linestring_processor {
    linestring_processor(mapbox::geometry::multi_line_string<std::int64_t>& mline)
        : mline_(mline) {}

    void linestring_begin(std::uint32_t count) {
        mline_.emplace_back();
        mline_.back().reserve(count);
    }

    void linestring_point(vtzero::point const& point) const {
        mline_.back().emplace_back(point.x, 4096.0 - point.y);
    }

    void linestring_end() const noexcept {
    }
    mapbox::geometry::multi_line_string<std::int64_t>& mline_;
};

struct polygon_processor {
    polygon_processor(mapbox::geometry::multi_polygon<std::int64_t>& mpoly)
        : mpoly_(mpoly) {}

    void ring_begin(std::uint32_t count) {
        ring_.reserve(count);
    }

    void ring_point(vtzero::point const& point) {
        ring_.emplace_back(point.x, 4096.0 - point.y);
    }

    void ring_end(bool is_outer) {
        if (is_outer) mpoly_.emplace_back();
        mpoly_.back().push_back(std::move(ring_));
        ring_.clear();
    }

    mapbox::geometry::multi_polygon<std::int64_t>& mpoly_;
    mapbox::geometry::linear_ring<std::int64_t> ring_;
};
