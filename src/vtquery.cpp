#include "vtquery.hpp"
#include "util.hpp"

#include <algorithm>
#include <exception>
#include <gzip/decompress.hpp>
#include <gzip/utils.hpp>
#include <iostream>
#include <map>
#include <mapbox/geometry/algorithms/closest_point.hpp>
#include <mapbox/geometry/algorithms/closest_point_impl.hpp>
#include <mapbox/geometry/geometry.hpp>
#include <mapbox/vector_tile.hpp>
#include <memory>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vtzero/types.hpp>
#include <vtzero/vector_tile.hpp>

namespace VectorTileQuery {

enum GeomType { point,
                linestring,
                polygon,
                all,
                unknown };
static const char* GeomTypeStrings[] = {"point", "linestring", "polygon", "unknown"};
const char* getGeomTypeString(int enumVal) {
    return GeomTypeStrings[enumVal]; // NOLINT to temporarily disable cppcoreguidelines-pro-bounds-constant-array-index, but this really should be fixed
}

using materialized_prop_type = std::pair<std::string, mapbox::feature::value>;

/// main storage item for returning to the user
struct ResultObject {
    std::vector<vtzero::property> properties_vector;
    std::vector<materialized_prop_type> properties_vector_materialized;
    std::string layer_name;
    mapbox::geometry::point<double> coordinates;
    double distance;
    GeomType original_geometry_type{GeomType::unknown};
    bool has_id{false};
    uint64_t id{0};

    ResultObject() : coordinates(0.0, 0.0),
                     distance(std::numeric_limits<double>::max()) {}

    ResultObject(ResultObject&&) = default;
    ResultObject& operator=(ResultObject&&) = default;
    ResultObject(ResultObject const&) = delete;
    ResultObject& operator=(ResultObject const&) = delete;
    ~ResultObject() = default;
};

/// an intermediate representation of a tile buffer and its necessary components
struct TileObject {
    TileObject(std::int32_t z0,
               std::int32_t x0,
               std::int32_t y0,
               Napi::Buffer<char> const& buffer)
        : z{z0},
          x{x0},
          y{y0},
          data{buffer.Data(), buffer.Length()},
          buffer_ref{Napi::Persistent(buffer)} {
    }

    ~TileObject() {
        buffer_ref.Reset();
    }

    // guarantee that objects are not being copied or moved
    // by deleting the copy and move definitions

    // non-copyable
    TileObject(TileObject const&) = delete;
    TileObject& operator=(TileObject const&) = delete;

    // non-movable
    TileObject(TileObject&&) = delete;
    TileObject& operator=(TileObject&&) = delete;

    std::int32_t z;
    std::int32_t x;
    std::int32_t y;
    vtzero::data_view data;
    Napi::Reference<Napi::Buffer<char>> buffer_ref;
};

using value_type = boost::variant<float, double, int64_t, uint64_t, bool, std::string>;
using map_type = std::unordered_map<std::string, value_type>;

enum BasicFilterType {
    ne,
    eq,
    lt,
    lte,
    gt,
    gte
};

struct basic_filter_struct {
    explicit basic_filter_struct()
        : key(""),
          value(false) {}

    std::string key;
    BasicFilterType type{eq};
    value_type value;
};

enum BasicMetaFilterType {
    filter_all,
    filter_any
};

struct meta_filter_struct {
    explicit meta_filter_struct() = default;

    BasicMetaFilterType type{filter_all};
    std::vector<basic_filter_struct> filters;
};

/// the baton of data to be passed from the v8 thread into the cpp threadpool
struct QueryData {
    explicit QueryData(std::uint32_t num_tiles)
        : latitude(0.0),
          longitude(0.0),
          radius(0.0),
          num_results(5),
          dedupe(true),
          direct_hit_polygon(false),
          geometry_filter_type(GeomType::all) {
        tiles.reserve(num_tiles);
    }

    ~QueryData() = default;

    // non-copyable
    QueryData(QueryData const&) = delete;
    QueryData& operator=(QueryData const&) = delete;

    // non-movable
    QueryData(QueryData&&) = delete;
    QueryData& operator=(QueryData&&) = delete;

    // buffers object thing
    std::vector<std::unique_ptr<TileObject>> tiles;
    std::vector<std::string> layers;
    double latitude;
    double longitude;
    double radius;
    std::uint32_t num_results;
    bool dedupe;
    bool direct_hit_polygon;
    GeomType geometry_filter_type;
    meta_filter_struct basic_filter;
};

/// convert properties to v8 types
struct property_value_visitor {
    Napi::Object& properties_obj;
    std::string const& key;
    Napi::Env& env;
    template <typename T>
    void operator()(T /*unused*/) {}

    void operator()(bool v) {
        properties_obj.Set(key, Napi::Boolean::New(env, v));
    }
    void operator()(uint64_t v) {
        properties_obj.Set(key, Napi::Number::New(env, v));
    }
    void operator()(int64_t v) {
        properties_obj.Set(key, Napi::Number::New(env, v));
    }
    void operator()(double v) {
        properties_obj.Set(key, Napi::Number::New(env, v));
    }
    void operator()(std::string const& v) {
        properties_obj.Set(key, Napi::String::New(env, v));
    }
};

/// used to create the final v8 (JSON) object to return to the user
void set_property(materialized_prop_type const& property,
                  Napi::Object& properties_obj, Napi::Env env) {
    mapbox::util::apply_visitor(property_value_visitor{properties_obj, property.first, env}, property.second);
}

GeomType get_geometry_type(vtzero::feature const& f) {
    GeomType gt = GeomType::unknown;
    switch (f.geometry_type()) {
    case vtzero::GeomType::POINT: {
        gt = GeomType::point;
        break;
    }
    case vtzero::GeomType::LINESTRING: {
        gt = GeomType::linestring;
        break;
    }
    case vtzero::GeomType::POLYGON: {
        gt = GeomType::polygon;
        break;
    }
    default: {
        break;
    }
    }

    return gt;
}

struct CompareDistance {
    bool operator()(ResultObject const& r1, ResultObject const& r2) {
        return r1.distance < r2.distance;
    }
};

/// replace already existing results with a better, duplicate result
void insert_result(ResultObject& old_result,
                   std::vector<vtzero::property>& props_vec,
                   std::string const& layer_name,
                   mapbox::geometry::point<double> const& pt,
                   double distance,
                   GeomType geom_type,
                   bool has_id,
                   uint64_t id) {

    std::swap(old_result.properties_vector, props_vec);
    old_result.layer_name = layer_name;
    old_result.coordinates = pt;
    old_result.distance = distance;
    old_result.original_geometry_type = geom_type;
    old_result.has_id = has_id;
    old_result.id = id;
}

/// generate a vector of vtzero::property objects
std::vector<vtzero::property> get_properties_vector(vtzero::feature& feat) {
    std::vector<vtzero::property> v;
    v.reserve(feat.num_properties());
    while (auto ii = feat.next_property()) {
        v.push_back(ii);
    }
    return v;
}

double convert_to_double(value_type const& value) {
    // float
    if (value.which() == 0) {
        return double(boost::get<float>(value));
    }
    // double
    if (value.which() == 1) {
        return boost::get<double>(value);
    }
    // int64_t
    if (value.which() == 2) {
        return double(boost::get<int64_t>(value));
    }
    // uint64_t
    if (value.which() == 3) {
        return double(boost::get<uint64_t>(value));
    }
    return 0.0f;
}

/// Evaluates a single filter on a feature - Returns true if it passes filter
bool single_filter_feature(basic_filter_struct const& filter, value_type const& feature_value) {
    double epsilon = 0.001;
    if (feature_value.which() <= 3 && filter.value.which() <= 3) { // Numeric Types
        double parameter_double = convert_to_double(feature_value);
        double filter_double = convert_to_double(filter.value);
        if ((filter.type == eq) && (std::abs(parameter_double - filter_double) < epsilon)) {
            return true;
        }
        if ((filter.type == ne) && (std::abs(parameter_double - filter_double) >= epsilon)) {
            return true;
        }
        if ((filter.type == gte) && (parameter_double >= filter_double)) {
            return true;
        }
        if ((filter.type == gt) && (parameter_double > filter_double)) {
            return true;
        }
        if ((filter.type == lte) && (parameter_double <= filter_double)) {
            return true;
        }
        if ((filter.type == lt) && (parameter_double < filter_double)) {
            return true;
        }
    } else if (feature_value.which() == 4 && filter.value.which() == 4) { // Boolean Types
        bool feature_bool = boost::get<bool>(feature_value);
        bool filter_bool = boost::get<bool>(filter.value);
        if ((filter.type == eq) && (feature_bool == filter_bool)) {
            return true;
        }
        if ((filter.type == ne) && (feature_bool != filter_bool)) {
            return true;
        }
    }
    return false;
}

/// apply filters to a feature - Returns true if feature matches all features
bool filter_feature_all(vtzero::feature& feature, std::vector<basic_filter_struct> const& filters) {
    auto features_property_map = vtzero::create_properties_map<map_type>(feature);
    for (auto const& filter : filters) {
        auto it = features_property_map.find(filter.key);
        if (it != features_property_map.end()) {
            value_type feature_value = it->second;
            if (!single_filter_feature(filter, feature_value)) {
                return false;
            }
        }
    }
    return true;
}

/// apply filters to a feature - Returns true if feature matches any features
bool filter_feature_any(vtzero::feature& feature, std::vector<basic_filter_struct> const& filters) {
    auto features_property_map = vtzero::create_properties_map<map_type>(feature);
    for (auto const& filter : filters) {
        auto it = features_property_map.find(filter.key);
        if (it != features_property_map.end()) {
            value_type feature_value = it->second;
            if (single_filter_feature(filter, feature_value)) {
                return true;
            }
        }
    }
    return false;
}

/// apply filters to a feature - Returns true if a feature matches the filters
bool filter_feature(vtzero::feature& feature, std::vector<basic_filter_struct> const& filters, BasicMetaFilterType filter_type) {
    if (filter_type == filter_all) {
        return filter_feature_all(feature, filters);
    }
    return filter_feature_any(feature, filters);
}

/// compare two features to determine if they are duplicates
bool value_is_duplicate(ResultObject const& r,
                        vtzero::feature const& candidate_feature,
                        std::string const& candidate_layer,
                        GeomType const candidate_geom,
                        std::vector<vtzero::property> const& candidate_props_vec) {

    // compare layer (if different layers, not duplicates)
    if (r.layer_name != candidate_layer) {
        return false;
    }

    // compare geometry (if different geometry types, not duplicates)
    if (r.original_geometry_type != candidate_geom) {
        return false;
    }

    // compare ids
    if (r.has_id && candidate_feature.has_id() && r.id != candidate_feature.id()) {
        return false;
    }

    // compare property tags
    return r.properties_vector == candidate_props_vec;
}

/// main worker used by N-API
struct Worker : Napi::AsyncWorker {
    using Base = Napi::AsyncWorker;

    /// set up major containers
    std::unique_ptr<QueryData> query_data_;
    std::vector<ResultObject> results_queue_;

    Worker(std::unique_ptr<QueryData> query_data,
           Napi::Function& cb)
        : Base(cb),
          query_data_(std::move(query_data)),
          // reserve the query results and fill with empty objects
          results_queue_{query_data_->num_results} {}

    void Execute() override {
        try {
            QueryData const& data = *query_data_;

            std::vector<basic_filter_struct> filters = data.basic_filter.filters;
            bool filter_enabled = !filters.empty();
            // query point lng/lat geometry.hpp point (used for distance calculation later on)
            mapbox::geometry::point<double> query_lnglat{data.longitude, data.latitude};

            gzip::Decompressor decompressor;
            std::string uncompressed;
            std::vector<std::string> buffers;
            std::vector<std::tuple<vtzero::vector_tile, std::int32_t, std::int32_t, std::int32_t>> tiles;
            tiles.reserve(data.tiles.size());
            for (auto const& tile_ptr : data.tiles) {
                TileObject const& tile_obj = *tile_ptr;
                if (gzip::is_compressed(tile_obj.data.data(), tile_obj.data.size())) {
                    decompressor.decompress(uncompressed, tile_obj.data.data(), tile_obj.data.size());
                    buffers.emplace_back(std::move(uncompressed));
                    tiles.emplace_back(vtzero::vector_tile(buffers.back()), tile_obj.z, tile_obj.x, tile_obj.y);
                } else {
                    tiles.emplace_back(vtzero::vector_tile(tile_obj.data), tile_obj.z, tile_obj.x, tile_obj.y);
                }
            }
            // for each tile
            for (auto& tile_obj : tiles) {
                vtzero::vector_tile& tile = std::get<0>(tile_obj);
                while (auto layer = tile.next_layer()) {

                    // check if this is a layer we should query
                    std::string layer_name = std::string(layer.name());
                    if (!data.layers.empty() && std::find(data.layers.begin(), data.layers.end(), layer_name) == data.layers.end()) {
                        continue;
                    }

                    std::uint32_t extent = layer.extent();
                    std::int32_t tile_obj_z = std::get<1>(tile_obj);
                    std::int32_t tile_obj_x = std::get<2>(tile_obj);
                    std::int32_t tile_obj_y = std::get<3>(tile_obj);
                    // query point in relation to the current tile the layer extent
                    mapbox::geometry::point<std::int64_t> query_point = utils::create_query_point(data.longitude, data.latitude, extent, tile_obj_z, tile_obj_x, tile_obj_y);

                    while (auto feature = layer.next_feature()) {
                        auto original_geometry_type = get_geometry_type(feature);

                        // check if this a geometry type we want to keep
                        if (data.geometry_filter_type != GeomType::all && data.geometry_filter_type != original_geometry_type) {
                            continue;
                        }

                        // implement closest point algorithm on query geometry and the query point
                        auto const cp_info = mapbox::geometry::algorithms::closest_point(mapbox::vector_tile::extract_geometry<int64_t>(feature), query_point);

                        // distance should never be less than zero, this is a safety check
                        if (cp_info.distance < 0.0) {
                            continue;
                        }

                        double meters = 0.0;
                        auto ll = mapbox::geometry::point<double>{data.longitude, data.latitude}; // default to original query lng/lat

                        // if distance from the query point is greater than 0.0 (not a direct hit) so recalculate the latlng
                        if (cp_info.distance > 0.0) {
                            ll = utils::convert_vt_to_ll(extent, tile_obj_z, tile_obj_x, tile_obj_y, cp_info);
                            meters = utils::distance_in_meters(query_lnglat, ll);
                        }

                        // if distance from the query point is greater than the radius, don't add it
                        if (meters > data.radius) {
                            continue;
                        }

                        // If direct_hit_polygon is enabled, disallow polygons that do not contain the point
                        if (meters > 0.0 && original_geometry_type == GeomType::polygon && data.direct_hit_polygon) {
                            continue;
                        }

                        // If we have filters and the feature doesn't pass the filters, skip this feature
                        if (filter_enabled && !filter_feature(feature, filters, data.basic_filter.type)) {
                            continue;
                        }

                        // check for duplicates
                        // if the candidate is a duplicate and smaller in distance, replace it
                        bool found_duplicate = false;
                        bool skip_duplicate = false;
                        auto properties_vec = get_properties_vector(feature);
                        if (data.dedupe) {
                            for (auto& result : results_queue_) {
                                if (value_is_duplicate(result, feature, layer_name, original_geometry_type, properties_vec)) {
                                    if (meters <= result.distance) {
                                        insert_result(result, properties_vec, layer_name, ll, meters, original_geometry_type, feature.has_id(), feature.id());
                                        found_duplicate = true;
                                        break;
                                        // if we have a duplicate but it's lesser than what we already have, just skip and don't add below
                                    }
                                    skip_duplicate = true;
                                    break;
                                }
                            }
                        }

                        if (skip_duplicate) {
                            continue;
                        }

                        if (found_duplicate) {
                            std::stable_sort(results_queue_.begin(), results_queue_.end(), CompareDistance());
                            continue;
                        }

                        if (meters < results_queue_.back().distance) {
                            insert_result(results_queue_.back(), properties_vec, layer_name, ll, meters, original_geometry_type, feature.has_id(), feature.id());
                            std::stable_sort(results_queue_.begin(), results_queue_.end(), CompareDistance());
                        }
                    } // end tile.layer.feature loop
                }     // end tile.layer loop
            }         // end tile loop
            // Here we create "materialized" properties. We do this here because, when reading from a compressed
            // buffer, it is unsafe to touch `feature.properties_vector` once we've left this loop.
            // That is because the buffer may represent uncompressed data that is not in scope outside of Execute()
            for (auto& feature : results_queue_) {
                feature.properties_vector_materialized.reserve(feature.properties_vector.size());
                for (auto const& property : feature.properties_vector) {
                    auto val = vtzero::convert_property_value<mapbox::feature::value, mapbox::vector_tile::detail::property_value_mapping>(property.value());
                    feature.properties_vector_materialized.emplace_back(std::string(property.key()), std::move(val));
                }
            }
        } catch (std::exception const& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        try {
            Napi::Object results_object = Napi::Object::New(Env());
            Napi::Array features_array = Napi::Array::New(Env());
            results_object.Set("type", "FeatureCollection");

            // for each result object
            while (!results_queue_.empty()) {
                auto const& feature = results_queue_.back(); // get reference to top item in results queue
                if (feature.distance < std::numeric_limits<double>::max()) {
                    // if this is a default value, don't use it
                    Napi::Object feature_obj = Napi::Object::New(Env());
                    feature_obj.Set("type", "Feature");
                    feature_obj.Set("id", feature.id);

                    // create geometry object
                    Napi::Object geometry_obj = Napi::Object::New(Env());
                    geometry_obj.Set("type", "Point");
                    Napi::Array coordinates_array = Napi::Array::New(Env(), 2);
                    coordinates_array.Set(0u, feature.coordinates.x); // latitude
                    coordinates_array.Set(1u, feature.coordinates.y); // longitude
                    geometry_obj.Set("coordinates", coordinates_array);
                    feature_obj.Set("geometry", geometry_obj);

                    // create properties object
                    Napi::Object properties_obj = Napi::Object::New(Env());
                    for (auto const& prop : feature.properties_vector_materialized) {
                        set_property(prop, properties_obj, Env());
                    }

                    // set properties.tilquery
                    Napi::Object tilequery_properties_obj = Napi::Object::New(Env());
                    tilequery_properties_obj.Set("distance", feature.distance);
                    std::string og_geom = getGeomTypeString(feature.original_geometry_type);
                    tilequery_properties_obj.Set("geometry", og_geom);
                    tilequery_properties_obj.Set("layer", feature.layer_name);
                    properties_obj.Set("tilequery", tilequery_properties_obj);

                    // add properties to feature
                    feature_obj.Set("properties", properties_obj);

                    // add feature to features array
                    features_array.Set(static_cast<uint32_t>(results_queue_.size() - 1), feature_obj);
                }

                results_queue_.pop_back();
            }
            results_object.Set("features", features_array);
            Callback().Call({Env().Null(), results_object});

        } catch (const std::exception& e) {
            // unable to create test to throw exception here, the try/catch is simply
            // for unexpected cases https://github.com/mapbox/vtquery/issues/69
            // LCOV_EXCL_START
            Callback().Call({Napi::String::New(Env(), e.what()), Env().Null()});
            // LCOV_EXCL_STOP
        }
    }
};

Napi::Value vtquery(Napi::CallbackInfo const& info) {
    // validate callback function
    // validate callback function
    std::size_t length = info.Length();
    if (length == 0) {
        Napi::Error::New(info.Env(), "last argument must be a callback function").ThrowAsJavaScriptException();
        return info.Env().Null();
    }
    Napi::Value callback_val = info[info.Length() - 1];
    if (!callback_val.IsFunction()) {
        Napi::Error::New(info.Env(), "last argument must be a callback function").ThrowAsJavaScriptException();
        return info.Env().Null();
    }
    Napi::Function callback = callback_val.As<Napi::Function>();

    // validate tiles
    if (!info[0].IsArray()) {
        return utils::CallbackError("first arg 'tiles' must be an array of tile objects", info);
    }

    Napi::Array tiles_arr_val = info[0].As<Napi::Array>();
    unsigned num_tiles = tiles_arr_val.Length();

    if (num_tiles <= 0) {
        return utils::CallbackError("'tiles' array must be of length greater than 0", info);
    }

    std::unique_ptr<QueryData> query_data = std::make_unique<QueryData>(num_tiles);

    for (unsigned t = 0; t < num_tiles; ++t) {
        Napi::Value tile_val = (tiles_arr_val).Get(t);
        if (!tile_val.IsObject()) {
            return utils::CallbackError("items in 'tiles' array must be objects", info);
        }

        Napi::Object tile_obj = tile_val.As<Napi::Object>();
        // check buffer value
        if (!tile_obj.Has("buffer")) {
            return utils::CallbackError("item in 'tiles' array does not include a buffer value", info);
        }
        Napi::Value buf_val = tile_obj.Get("buffer");
        if (buf_val.IsNull() || buf_val.IsUndefined()) {
            return utils::CallbackError("buffer value in 'tiles' array item is null or undefined", info);
        }

        Napi::Object buffer_obj = buf_val.As<Napi::Object>();
        if (!buffer_obj.IsBuffer()) {
            return utils::CallbackError("buffer value in 'tiles' array item is not a true buffer", info);
        }
        Napi::Buffer<char> buffer = buffer_obj.As<Napi::Buffer<char>>();

        // z value
        if (!tile_obj.Has("z")) {
            return utils::CallbackError("item in 'tiles' array does not include a 'z' value", info);
        }
        Napi::Value z_val = tile_obj.Get("z");
        if (!z_val.IsNumber()) {
            return utils::CallbackError("'z' value in 'tiles' array item is not an int32", info);
        }

        std::int32_t z = z_val.As<Napi::Number>().Int32Value();
        if (z < 0) {
            return utils::CallbackError("'z' value must not be less than zero", info);
        }

        // x value
        if (!tile_obj.Has("x")) {
            return utils::CallbackError("item in 'tiles' array does not include a 'x' value", info);
        }
        Napi::Value x_val = tile_obj.Get("x");
        if (!x_val.IsNumber()) {
            return utils::CallbackError("'x' value in 'tiles' array item is not an int32", info);
        }

        std::int32_t x = x_val.As<Napi::Number>().Int32Value();
        if (x < 0) {
            return utils::CallbackError("'x' value must not be less than zero", info);
        }

        // y value
        if (!(tile_obj).Has("y")) {
            return utils::CallbackError("item in 'tiles' array does not include a 'y' value", info);
        }
        Napi::Value y_val = tile_obj.Get("y");
        if (!y_val.IsNumber()) {
            return utils::CallbackError("'y' value in 'tiles' array item is not an int32", info);
        }

        std::int32_t y = y_val.As<Napi::Number>().Int32Value();
        if (y < 0) {
            return utils::CallbackError("'y' value must not be less than zero", info);
        }
        // in-place construction
        query_data->tiles.push_back(std::make_unique<TileObject>(z, x, y, buffer));
    }

    // validate lng/lat array
    if (!info[1].IsArray()) {
        return utils::CallbackError("second arg 'lnglat' must be an array with [longitude, latitude] values", info);
    }

    // Napi::Array lnglat_val(info[1]);
    Napi::Array lnglat_val = info[1].As<Napi::Array>();
    if (lnglat_val.Length() != 2) {
        return utils::CallbackError("'lnglat' must be an array of [longitude, latitude]", info);
    }

    Napi::Value lng_val = lnglat_val.Get(0u);
    Napi::Value lat_val = lnglat_val.Get(1u);
    if (!lng_val.IsNumber() || !lat_val.IsNumber()) {
        return utils::CallbackError("lnglat values must be numbers", info);
    }
    query_data->longitude = lng_val.As<Napi::Number>().DoubleValue();
    query_data->latitude = lat_val.As<Napi::Number>().DoubleValue();

    // validate options object if it exists
    // defaults are set in the QueryData struct.
    if (info.Length() > 3) {

        if (!info[2].IsObject()) {
            return utils::CallbackError("'options' arg must be an object", info);
        }

        Napi::Object options = info[2].As<Napi::Object>();

        if (options.Has("dedupe")) {
            Napi::Value dedupe_val = options.Get("dedupe");
            if (!dedupe_val.IsBoolean()) {
                return utils::CallbackError("'dedupe' must be a boolean", info);
            }

            bool dedupe = dedupe_val.As<Napi::Boolean>().Value();
            query_data->dedupe = dedupe;
        }

        if (options.Has("direct_hit_polygon")) {
            Napi::Value direct_hit_polygon_val = options.Get("direct_hit_polygon");
            if (!direct_hit_polygon_val.IsBoolean()) {
                return utils::CallbackError("'direct_hit_polygon' must be a boolean", info);
            }

            bool direct_hit_polygon = direct_hit_polygon_val.As<Napi::Boolean>().Value();
            query_data->direct_hit_polygon = direct_hit_polygon;
        }

        if (options.Has("radius")) {
            Napi::Value radius_val = options.Get("radius");
            if (!radius_val.IsNumber()) {
                return utils::CallbackError("'radius' must be a number", info);
            }

            double radius = radius_val.ToNumber();
            if (radius < 0.0) {
                return utils::CallbackError("'radius' must be a positive number", info);
            }

            query_data->radius = radius;
        }

        if (options.Has("limit")) {
            Napi::Value num_results_val = options.Get("limit");
            if (!num_results_val.IsNumber()) {
                return utils::CallbackError("'limit' must be a number", info);
            }

            std::int32_t num_results = num_results_val.As<Napi::Number>().Int32Value();
            if (num_results < 1) {
                return utils::CallbackError("'limit' must be 1 or greater", info);
            }
            if (num_results > 1000) {
                return utils::CallbackError("'limit' must be less than 1000", info);
            }

            query_data->num_results = static_cast<std::uint32_t>(num_results);
        }

        if (options.Has("layers")) {
            Napi::Value layers_val = options.Get("layers");
            if (!layers_val.IsArray()) {
                return utils::CallbackError("'layers' must be an array of strings", info);
            }

            Napi::Array layers_arr = layers_val.As<Napi::Array>();
            unsigned num_layers = layers_arr.Length();

            // only gather layers if there are some in the array
            if (num_layers > 0) {
                for (unsigned j = 0; j < num_layers; ++j) {
                    Napi::Value layer_val = layers_arr.Get(j);
                    if (!layer_val.IsString()) {
                        return utils::CallbackError("'layers' values must be strings", info);
                    }
                    std::string layer_name = layer_val.As<Napi::String>();
                    if (layer_name.empty()) {
                        return utils::CallbackError("'layers' values must be non-empty strings", info);
                    }
                    query_data->layers.emplace_back(layer_name);
                }
            }
        }

        if (options.Has("geometry")) {
            Napi::Value geometry_val = options.Get("geometry");
            if (!geometry_val.IsString()) {
                return utils::CallbackError("'geometry' option must be a string", info);
            }

            std::string geometry = geometry_val.As<Napi::String>();
            if (geometry.empty()) {
                return utils::CallbackError("'geometry' value must be a non-empty string", info);
            }
            if (geometry == "point") {
                query_data->geometry_filter_type = GeomType::point;
            } else if (geometry == "linestring") {
                query_data->geometry_filter_type = GeomType::linestring;
            } else if (geometry == "polygon") {
                query_data->geometry_filter_type = GeomType::polygon;
            } else {
                return utils::CallbackError("'geometry' must be 'point', 'linestring', or 'polygon'", info);
            }
        }

        if (options.Has("basic-filters")) {
            Napi::Value basic_filter_val = options.Get("basic-filters");
            if (basic_filter_val.IsArrayBuffer()) {
                return utils::CallbackError("'basic-filters' must be of the form [type, [filters]]", info);
            }

            Napi::Array basic_filter_array = basic_filter_val.As<Napi::Array>();
            unsigned basic_filter_length = basic_filter_array.Length();

            // gather filters from an array
            if (basic_filter_length == 2) {
                Napi::Value basic_filter_type = (basic_filter_array).Get(0u);
                if (!basic_filter_type.IsString()) {
                    return utils::CallbackError("'basic-filters' must be of the form [string, [filters]]", info);
                }
                std::string basic_filter_type_str = basic_filter_type.As<Napi::String>();
                if (basic_filter_type_str == "all") {
                    query_data->basic_filter.type = filter_all;
                } else if (basic_filter_type_str == "any") {
                    query_data->basic_filter.type = filter_any;
                } else {
                    return utils::CallbackError("'basic-filters[0] must be 'any' or 'all'", info);
                }

                Napi::Value filters_array_val = basic_filter_array.Get(1u);
                if (!filters_array_val.IsArray()) {
                    return utils::CallbackError("'basic-filters' must be of the form [type, [filters]]", info);
                }

                Napi::Array filters_array = filters_array_val.As<Napi::Array>();
                unsigned num_filters = filters_array.Length();
                for (unsigned j = 0; j < num_filters; ++j) {
                    basic_filter_struct filter;
                    Napi::Value filter_val = filters_array.Get(j);
                    if (!filter_val.IsArray()) {
                        return utils::CallbackError("filters must be of the form [parameter, condition, value]", info);
                    }
                    Napi::Array filter_array = filter_val.As<Napi::Array>();
                    unsigned filter_length = filter_array.Length();

                    if (filter_length != 3) {
                        return utils::CallbackError("filters must be of the form [parameter, condition, value]", info);
                    }

                    Napi::Value filter_parameter_val = filter_array.Get(0u);
                    if (!filter_parameter_val.IsString()) {
                        return utils::CallbackError("parameter filter option must be a string", info);
                    }

                    std::string filter_parameter = filter_parameter_val.As<Napi::String>();
                    if (filter_parameter.empty()) {
                        return utils::CallbackError("parameter filter value must be a non-empty string", info);
                    }
                    filter.key = filter_parameter;

                    Napi::Value filter_condition_val = filter_array.Get(1u);
                    if (!filter_condition_val.IsString()) {
                        return utils::CallbackError("condition filter option must be a string", info);
                    }

                    std::string filter_condition = filter_condition_val.As<Napi::String>();
                    if (filter_condition.empty()) {
                        return utils::CallbackError("condition filter value must be a non-empty string", info);
                    }
                    if (filter_condition == "=") {
                        filter.type = eq;
                    } else if (filter_condition == "!=") {
                        filter.type = ne;
                    } else if (filter_condition == "<") {
                        filter.type = lt;
                    } else if (filter_condition == "<=") {
                        filter.type = lte;
                    } else if (filter_condition == ">") {
                        filter.type = gt;
                    } else if (filter_condition == ">=") {
                        filter.type = gte;
                    } else {
                        return utils::CallbackError("condition filter value must be =, !=, <, <=, >, or >=", info);
                    }

                    Napi::Value filter_value_val = filter_array.Get(2u);
                    if (filter_value_val.IsNumber()) {
                        double filter_value_double = filter_value_val.As<Napi::Number>().DoubleValue();
                        filter.value = filter_value_double;
                    } else if (filter_value_val.IsBoolean()) {
                        filter.value = filter_value_val.As<Napi::Boolean>();
                    } else {
                        return utils::CallbackError("value filter value must be a number or boolean", info);
                    }
                    query_data->basic_filter.filters.push_back(filter);
                }
            } else {
                return utils::CallbackError("'basic-filters' must be of the form [type, [filters]]", info);
            }
        }
    }

    auto* worker = new Worker{std::move(query_data), callback};
    worker->Queue();
    return info.Env().Undefined();
}

} // namespace VectorTileQuery
