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
    return GeomTypeStrings[enumVal];
}

using materialized_prop_type = std::pair<std::string, mapbox::feature::value>;

/// main storage item for returning to the user
struct ResultObject {
    std::vector<vtzero::property> properties_vector;
    std::vector<materialized_prop_type> properties_vector_materialized;
    std::string layer_name;
    mapbox::geometry::point<double> coordinates;
    double distance;
    GeomType original_geometry_type;
    bool has_id;
    uint64_t id;

    ResultObject() : properties_vector(),
                     properties_vector_materialized(),
                     layer_name(),
                     coordinates(0.0, 0.0),
                     distance(std::numeric_limits<double>::max()),
                     original_geometry_type(GeomType::unknown),
                     has_id(false),
                     id(0) {}

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
               v8::Local<v8::Object> buffer)
        : z(z0),
          x(x0),
          y(y0),
          data(node::Buffer::Data(buffer), node::Buffer::Length(buffer)),
          buffer_ref() {
        buffer_ref.Reset(buffer.As<v8::Object>());
    }

    // explicitly use the destructor to clean up
    // the persistent buffer ref by Reset()-ing
    ~TileObject() {
        buffer_ref.Reset();
    }

    // guarantee that objects are not being copied by deleting the
    // copy and move definitions

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
    Nan::Persistent<v8::Object> buffer_ref;
};

using value_type = boost::variant<float, double, int64_t, uint64_t, bool, std::string>;

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
          type(eq),
          value(false) {
    }

    std::string key;
    BasicFilterType type;
    value_type value;
};

enum BasicMetaFilterType {
    filter_all,
    filter_any
};

struct meta_filter_struct {
    explicit meta_filter_struct()
        : type(filter_all),
          filters() {
    }

    BasicMetaFilterType type;
    std::vector<basic_filter_struct> filters;
};

/// the baton of data to be passed from the v8 thread into the cpp threadpool
struct QueryData {
    explicit QueryData(std::uint32_t num_tiles)
        : tiles(),
          layers(),
          latitude(0.0),
          longitude(0.0),
          radius(0.0),
          num_results(5),
          dedupe(true),
          geometry_filter_type(GeomType::all),
          basic_filter() {
        tiles.reserve(num_tiles);
    }

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
    GeomType geometry_filter_type;
    meta_filter_struct basic_filter;
};

/// convert properties to v8 types
struct property_value_visitor {
    v8::Local<v8::Object>& properties_obj;
    std::string const& key;

    template <typename T>
    void operator()(T) {}

    void operator()(bool v) {
        Nan::Set(properties_obj, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Boolean>(v));
    }
    void operator()(uint64_t v) {
        Nan::Set(properties_obj, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>(v));
    }
    void operator()(int64_t v) {
        Nan::Set(properties_obj, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>(v));
    }
    void operator()(double v) {
        Nan::Set(properties_obj, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>(v));
    }
    void operator()(std::string const& v) {
        Nan::Set(properties_obj, Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::String>(v).ToLocalChecked());
    }
};

/// used to create the final v8 (JSON) object to return to the user
void set_property(materialized_prop_type const& property,
                  v8::Local<v8::Object>& properties_obj) {
    mapbox::util::apply_visitor(property_value_visitor{properties_obj, property.first}, property.second);
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
                   mapbox::geometry::point<double>& pt,
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
std::vector<vtzero::property> get_properties_vector(vtzero::feature& f) {
    std::vector<vtzero::property> v;
    v.reserve(f.num_properties());
    while (auto ii = f.next_property()) {
        v.push_back(std::move(ii));
    }
    return v;
}

double convert_to_double(value_type value) {
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
bool single_filter_feature(basic_filter_struct filter, value_type feature_value) {
    double epsilon = 0.001;
    if (feature_value.which() <= 3 && filter.value.which() <= 3) { // Numeric Types
        double parameter_double = convert_to_double(feature_value);
        double filter_double = convert_to_double(filter.value);
        if ((filter.type == eq) && (std::abs(parameter_double - filter_double) < epsilon)) {
            return true;
        } else if ((filter.type == ne) && (std::abs(parameter_double - filter_double) >= epsilon)) {
            return true;
        } else if ((filter.type == gte) && (parameter_double >= filter_double)) {
            return true;
        } else if ((filter.type == gt) && (parameter_double > filter_double)) {
            return true;
        } else if ((filter.type == lte) && (parameter_double <= filter_double)) {
            return true;
        } else if ((filter.type == lt) && (parameter_double < filter_double)) {
            return true;
        }
    } else if (feature_value.which() == 4 && filter.value.which() == 4) { // Boolean Types
        bool feature_bool = boost::get<bool>(feature_value);
        bool filter_bool = boost::get<bool>(filter.value);
        if ((filter.type == eq) && (feature_bool == filter_bool)) {
            return true;
        } else if ((filter.type == ne) && (feature_bool != filter_bool)) {
            return true;
        }
    }
    return false;
}

/// apply filters to a feature - Returns true if feature matches all features
bool filter_feature_all(vtzero::feature& feature, std::vector<basic_filter_struct> filters) {
    using key_type = std::string;
    using map_type = std::map<key_type, value_type>;
    auto features_property_map = vtzero::create_properties_map<map_type>(feature);
    for (auto filter : filters) {
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
bool filter_feature_any(vtzero::feature& feature, std::vector<basic_filter_struct> filters) {
    using key_type = std::string;
    using map_type = std::map<key_type, value_type>;
    auto features_property_map = vtzero::create_properties_map<map_type>(feature);
    for (auto filter : filters) {
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
bool filter_feature(vtzero::feature& feature, std::vector<basic_filter_struct> filters, BasicMetaFilterType filter_type) {
    if (filter_type == filter_all) {
        return filter_feature_all(feature, filters);
    } else {
        return filter_feature_any(feature, filters);
    }
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

/// main worker used by NAN
struct Worker : Nan::AsyncWorker {
    using Base = Nan::AsyncWorker;

    /// set up major containers
    std::unique_ptr<QueryData> query_data_;
    std::vector<ResultObject> results_queue_;

    Worker(std::unique_ptr<QueryData> query_data,
           Nan::Callback* cb)
        : Base(cb, "vtquery:worker"),
          query_data_(std::move(query_data)),
          results_queue_() {}

    void Execute() override {
        try {
            QueryData const& data = *query_data_;

            std::vector<basic_filter_struct> filters = data.basic_filter.filters;
            bool filter_enabled = filters.size() > 0;

            // reserve the query results and fill with empty objects
            results_queue_.reserve(data.num_results);
            for (std::size_t i = 0; i < data.num_results; ++i) {
                results_queue_.emplace_back();
            }

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

                        if (filter_enabled && !filter_feature(feature, filters, data.basic_filter.type)) { // If we have filters and the feature doesn't pass the filters, skip this feature
                            continue;
                        }

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
                                    } else {
                                        skip_duplicate = true;
                                        break;
                                    }
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
            SetErrorMessage(e.what());
        }
    }

    void HandleOKCallback() override {
        Nan::HandleScope scope;
        try {
            v8::Local<v8::Object> results_object = Nan::New<v8::Object>();
            v8::Local<v8::Array> features_array = Nan::New<v8::Array>();
            Nan::Set(results_object, Nan::New("type").ToLocalChecked(), Nan::New<v8::String>("FeatureCollection").ToLocalChecked());

            // for each result object
            while (!results_queue_.empty()) {
                auto const& feature = results_queue_.back(); // get reference to top item in results queue
                if (feature.distance < std::numeric_limits<double>::max()) {
                    // if this is a default value, don't use it
                    v8::Local<v8::Object> feature_obj = Nan::New<v8::Object>();
                    Nan::Set(feature_obj, Nan::New("type").ToLocalChecked(), Nan::New<v8::String>("Feature").ToLocalChecked());
                    Nan::Set(feature_obj, Nan::New("id").ToLocalChecked(), Nan::New<v8::Number>(feature.id));

                    // create geometry object
                    v8::Local<v8::Object> geometry_obj = Nan::New<v8::Object>();
                    Nan::Set(geometry_obj, Nan::New("type").ToLocalChecked(), Nan::New<v8::String>("Point").ToLocalChecked());
                    v8::Local<v8::Array> coordinates_array = Nan::New<v8::Array>(2);
                    Nan::Set(coordinates_array, 0, Nan::New<v8::Number>(feature.coordinates.x)); // latitude
                    Nan::Set(coordinates_array, 1, Nan::New<v8::Number>(feature.coordinates.y)); // longitude
                    Nan::Set(geometry_obj, Nan::New("coordinates").ToLocalChecked(), coordinates_array);
                    Nan::Set(feature_obj, Nan::New("geometry").ToLocalChecked(), geometry_obj);

                    // create properties object
                    v8::Local<v8::Object> properties_obj = Nan::New<v8::Object>();
                    for (auto const& prop : feature.properties_vector_materialized) {
                        set_property(prop, properties_obj);
                    }

                    // set properties.tilquery
                    v8::Local<v8::Object> tilequery_properties_obj = Nan::New<v8::Object>();
                    Nan::Set(tilequery_properties_obj, Nan::New("distance").ToLocalChecked(), Nan::New<v8::Number>(feature.distance));
                    std::string og_geom = getGeomTypeString(feature.original_geometry_type);
                    Nan::Set(tilequery_properties_obj, Nan::New("geometry").ToLocalChecked(), Nan::New<v8::String>(og_geom).ToLocalChecked());
                    Nan::Set(tilequery_properties_obj, Nan::New("layer").ToLocalChecked(), Nan::New<v8::String>(feature.layer_name).ToLocalChecked());
                    Nan::Set(properties_obj, Nan::New("tilequery").ToLocalChecked(), tilequery_properties_obj);

                    // add properties to feature
                    Nan::Set(feature_obj, Nan::New("properties").ToLocalChecked(), properties_obj);

                    // add feature to features array
                    Nan::Set(features_array, static_cast<uint32_t>(results_queue_.size() - 1), feature_obj);
                }

                results_queue_.pop_back();
            }

            Nan::Set(results_object, Nan::New("features").ToLocalChecked(), features_array);

            auto const argc = 2u;
            v8::Local<v8::Value> argv[argc] = {
                Nan::Null(), results_object};

            callback->Call(argc, static_cast<v8::Local<v8::Value>*>(argv), async_resource);

        } catch (const std::exception& e) {
            // unable to create test to throw exception here, the try/catch is simply
            // for unexpected cases https://github.com/mapbox/vtquery/issues/69
            // LCOV_EXCL_START
            auto const argc = 1u;
            v8::Local<v8::Value> argv[argc] = {Nan::Error(e.what())};
            callback->Call(argc, static_cast<v8::Local<v8::Value>*>(argv), async_resource);
            // LCOV_EXCL_STOP
        }
    }
};

NAN_METHOD(vtquery) {
    // validate callback function
    v8::Local<v8::Value> callback_val = info[info.Length() - 1];
    if (!callback_val->IsFunction()) {
        Nan::ThrowError("last argument must be a callback function");
        return;
    }
    v8::Local<v8::Function> callback = callback_val.As<v8::Function>();

    // validate tiles
    if (!info[0]->IsArray()) {
        return utils::CallbackError("first arg 'tiles' must be an array of tile objects", callback);
    }

    v8::Local<v8::Array> tiles_arr_val = info[0].As<v8::Array>();
    unsigned num_tiles = tiles_arr_val->Length();

    if (num_tiles <= 0) {
        return utils::CallbackError("'tiles' array must be of length greater than 0", callback);
    }

    std::unique_ptr<QueryData> query_data = std::make_unique<QueryData>(num_tiles);

    for (unsigned t = 0; t < num_tiles; ++t) {
        v8::Local<v8::Value> tile_val = Nan::Get(tiles_arr_val,t).ToLocalChecked();
        if (!tile_val->IsObject()) {
            return utils::CallbackError("items in 'tiles' array must be objects", callback);
        }
        v8::Local<v8::Object> tile_obj = tile_val->ToObject(Nan::GetCurrentContext()).ToLocalChecked();

        // check buffer value
        if (!Nan::Has(tile_obj, Nan::New("buffer").ToLocalChecked()).FromMaybe(false)) {
            return utils::CallbackError("item in 'tiles' array does not include a buffer value", callback);
        }
        v8::Local<v8::Value> buf_val = Nan::Get(tile_obj,Nan::New("buffer").ToLocalChecked()).ToLocalChecked();
        if (buf_val->IsNull() || buf_val->IsUndefined()) {
            return utils::CallbackError("buffer value in 'tiles' array item is null or undefined", callback);
        }
        v8::Local<v8::Object> buffer = buf_val->ToObject(Nan::GetCurrentContext()).ToLocalChecked();
        if (!node::Buffer::HasInstance(buffer)) {
            return utils::CallbackError("buffer value in 'tiles' array item is not a true buffer", callback);
        }

        // z value
        if (!Nan::Has(tile_obj, Nan::New("z").ToLocalChecked()).FromMaybe(false)) {
            return utils::CallbackError("item in 'tiles' array does not include a 'z' value", callback);
        }
        v8::Local<v8::Value> z_val = Nan::Get(tile_obj,Nan::New("z").ToLocalChecked()).ToLocalChecked();
        if (!z_val->IsInt32()) {
            return utils::CallbackError("'z' value in 'tiles' array item is not an int32", callback);
        }
        std::int32_t z = Nan::To<std::int32_t>(z_val).FromJust();
        if (z < 0) {
            return utils::CallbackError("'z' value must not be less than zero", callback);
        }

        // x value
        if (!Nan::Has(tile_obj, Nan::New("x").ToLocalChecked()).FromMaybe(false)) {
            return utils::CallbackError("item in 'tiles' array does not include a 'x' value", callback);
        }
        v8::Local<v8::Value> x_val = Nan::Get(tile_obj,Nan::New("x").ToLocalChecked()).ToLocalChecked();
        if (!x_val->IsInt32()) {
            return utils::CallbackError("'x' value in 'tiles' array item is not an int32", callback);
        }
        std::int32_t x = Nan::To<std::int32_t>(x_val).FromJust();
        if (x < 0) {
            return utils::CallbackError("'x' value must not be less than zero", callback);
        }

        // y value
        if (!Nan::Has(tile_obj, Nan::New("y").ToLocalChecked()).FromMaybe(false)) {
            return utils::CallbackError("item in 'tiles' array does not include a 'y' value", callback);
        }
        v8::Local<v8::Value> y_val = Nan::Get(tile_obj,Nan::New("y").ToLocalChecked()).ToLocalChecked();
        if (!y_val->IsInt32()) {
            return utils::CallbackError("'y' value in 'tiles' array item is not an int32", callback);
        }
        std::int32_t y = Nan::To<std::int32_t>(y_val).FromJust();
        if (y < 0) {
            return utils::CallbackError("'y' value must not be less than zero", callback);
        }

        // in-place construction
        std::unique_ptr<TileObject> tile{new TileObject{z, x, y, buffer}};
        query_data->tiles.push_back(std::move(tile));
    }

    // validate lng/lat array
    if (!info[1]->IsArray()) {
        return utils::CallbackError("second arg 'lnglat' must be an array with [longitude, latitude] values", callback);
    }

    // v8::Local<v8::Array> lnglat_val(info[1]);
    v8::Local<v8::Array> lnglat_val = info[1].As<v8::Array>();
    if (lnglat_val->Length() != 2) {
        return utils::CallbackError("'lnglat' must be an array of [longitude, latitude]", callback);
    }

    v8::Local<v8::Value> lng_val = Nan::Get(lnglat_val, 0).ToLocalChecked();
    v8::Local<v8::Value> lat_val = Nan::Get(lnglat_val, 1).ToLocalChecked();
    if (!lng_val->IsNumber() || !lat_val->IsNumber()) {
        return utils::CallbackError("lnglat values must be numbers", callback);
    }
    query_data->longitude = Nan::To<double>(lng_val).FromJust();
    query_data->latitude = Nan::To<double>(lat_val).FromJust();

    // validate options object if it exists
    // defaults are set in the QueryData struct.
    if (info.Length() > 3) {

        if (!info[2]->IsObject()) {
            return utils::CallbackError("'options' arg must be an object", callback);
        }

        v8::Local<v8::Object> options = info[2]->ToObject(Nan::GetCurrentContext()).ToLocalChecked();

        if (Nan::Has(options,Nan::New("dedupe").ToLocalChecked()).FromMaybe(false)) {
            v8::Local<v8::Value> dedupe_val = Nan::Get(options, Nan::New("dedupe").ToLocalChecked()).ToLocalChecked();
            if (!dedupe_val->IsBoolean()) {
                return utils::CallbackError("'dedupe' must be a boolean", callback);
            }

            bool dedupe = Nan::To<bool>(dedupe_val).FromJust();
            query_data->dedupe = dedupe;
        }

        if (Nan::Has(options,Nan::New("radius").ToLocalChecked()).FromMaybe(false)) {
            v8::Local<v8::Value> radius_val = Nan::Get(options, Nan::New("radius").ToLocalChecked()).ToLocalChecked();
            if (!radius_val->IsNumber()) {
                return utils::CallbackError("'radius' must be a number", callback);
            }

            double radius = Nan::To<double>(radius_val).FromJust();
            if (radius < 0.0) {
                return utils::CallbackError("'radius' must be a positive number", callback);
            }

            query_data->radius = radius;
        }

        if (Nan::Has(options,Nan::New("limit").ToLocalChecked()).FromMaybe(false)) {
            v8::Local<v8::Value> num_results_val = Nan::Get(options, Nan::New("limit").ToLocalChecked()).ToLocalChecked();
            if (!num_results_val->IsNumber()) {
                return utils::CallbackError("'limit' must be a number", callback);
            }

            std::int32_t num_results = Nan::To<std::int32_t>(num_results_val).FromJust();
            if (num_results < 1) {
                return utils::CallbackError("'limit' must be 1 or greater", callback);
            }
            if (num_results > 1000) {
                return utils::CallbackError("'limit' must be less than 1000", callback);
            }

            query_data->num_results = static_cast<std::uint32_t>(num_results);
        }

        if (Nan::Has(options,Nan::New("layers").ToLocalChecked()).FromMaybe(false)) {
            v8::Local<v8::Value> layers_val = Nan::Get(options, Nan::New("layers").ToLocalChecked()).ToLocalChecked();
            if (!layers_val->IsArray()) {
                return utils::CallbackError("'layers' must be an array of strings", callback);
            }

            v8::Local<v8::Array> layers_arr = layers_val.As<v8::Array>();
            unsigned num_layers = layers_arr->Length();

            // only gather layers if there are some in the array
            if (num_layers > 0) {
                for (unsigned j = 0; j < num_layers; ++j) {
                    v8::Local<v8::Value> layer_val = Nan::Get(layers_arr,j).ToLocalChecked();
                    if (!layer_val->IsString()) {
                        return utils::CallbackError("'layers' values must be strings", callback);
                    }

                    Nan::Utf8String layer_utf8_value(layer_val);
                    int layer_str_len = layer_utf8_value.length();
                    if (layer_str_len <= 0) {
                        return utils::CallbackError("'layers' values must be non-empty strings", callback);
                    }

                    query_data->layers.emplace_back(*layer_utf8_value, static_cast<std::size_t>(layer_str_len));
                }
            }
        }

        if (Nan::Has(options,Nan::New("geometry").ToLocalChecked()).FromMaybe(false)) {
            v8::Local<v8::Value> geometry_val = Nan::Get(options, Nan::New("geometry").ToLocalChecked()).ToLocalChecked();
            if (!geometry_val->IsString()) {
                return utils::CallbackError("'geometry' option must be a string", callback);
            }

            Nan::Utf8String geometry_utf8_value(geometry_val);
            std::int32_t geometry_str_len = geometry_utf8_value.length();
            if (geometry_str_len <= 0) {
                return utils::CallbackError("'geometry' value must be a non-empty string", callback);
            }

            std::string geometry(*geometry_utf8_value, static_cast<std::size_t>(geometry_str_len));
            if (geometry == "point") {
                query_data->geometry_filter_type = GeomType::point;
            } else if (geometry == "linestring") {
                query_data->geometry_filter_type = GeomType::linestring;
            } else if (geometry == "polygon") {
                query_data->geometry_filter_type = GeomType::polygon;
            } else {
                return utils::CallbackError("'geometry' must be 'point', 'linestring', or 'polygon'", callback);
            }
        }

        if (Nan::Has(options,Nan::New("basic-filters").ToLocalChecked()).FromMaybe(false)) {
            v8::Local<v8::Value> basic_filter_val = Nan::Get(options, Nan::New("basic-filters").ToLocalChecked()).ToLocalChecked();
            if (!basic_filter_val->IsArray()) {
                return utils::CallbackError("'basic-filters' must be of the form [type, [filters]]", callback);
            }

            v8::Local<v8::Array> basic_filter_array = basic_filter_val.As<v8::Array>();
            unsigned basic_filter_length = basic_filter_array->Length();

            // gather filters from an array
            if (basic_filter_length == 2) {
                v8::Local<v8::Value> basic_filter_type = Nan::Get(basic_filter_array, 0).ToLocalChecked();
                if (!basic_filter_type->IsString()) {
                    return utils::CallbackError("'basic-filters' must be of the form [string, [filters]]", callback);
                }
                Nan::Utf8String basic_filter_type_utf8_value(basic_filter_type);
                std::int32_t basic_filter_type_str_len = basic_filter_type_utf8_value.length();
                std::string basic_filter_type_str(*basic_filter_type_utf8_value, static_cast<std::size_t>(basic_filter_type_str_len));
                if (basic_filter_type_str == "all") {
                    query_data->basic_filter.type = filter_all;
                } else if (basic_filter_type_str == "any") {
                    query_data->basic_filter.type = filter_any;
                } else {
                    return utils::CallbackError("'basic-filters[0] must be 'any' or 'all'", callback);
                }

                v8::Local<v8::Value> filters_array_val = Nan::Get(basic_filter_array,1).ToLocalChecked();
                if (!filters_array_val->IsArray()) {
                    return utils::CallbackError("'basic-filters' must be of the form [type, [filters]]", callback);
                }

                v8::Local<v8::Array> filters_array = filters_array_val.As<v8::Array>();
                unsigned num_filters = filters_array->Length();
                for (unsigned j = 0; j < num_filters; ++j) {
                    basic_filter_struct filter;
                    v8::Local<v8::Value> filter_val = Nan::Get(filters_array,j).ToLocalChecked();
                    if (!filter_val->IsArray()) {
                        return utils::CallbackError("filters must be of the form [parameter, condition, value]", callback);
                    }
                    v8::Local<v8::Array> filter_array = filter_val.As<v8::Array>();
                    unsigned filter_length = filter_array->Length();

                    if (filter_length != 3) {
                        return utils::CallbackError("filters must be of the form [parameter, condition, value]", callback);
                    }

                    v8::Local<v8::Value> filter_parameter_val = Nan::Get(filter_array, 0).ToLocalChecked();
                    if (!filter_parameter_val->IsString()) {
                        return utils::CallbackError("parameter filter option must be a string", callback);
                    }

                    Nan::Utf8String filter_parameter_utf8_value(filter_parameter_val);
                    std::int32_t filter_parameter_len = filter_parameter_utf8_value.length();
                    if (filter_parameter_len <= 0) {
                        return utils::CallbackError("parameter filter value must be a non-empty string", callback);
                    }

                    std::string filter_parameter(*filter_parameter_utf8_value, static_cast<std::size_t>(filter_parameter_len));
                    filter.key.assign(filter_parameter);

                    v8::Local<v8::Value> filter_condition_val = Nan::Get(filter_array,1).ToLocalChecked();
                    if (!filter_condition_val->IsString()) {
                        return utils::CallbackError("condition filter option must be a string", callback);
                    }

                    Nan::Utf8String filter_condition_utf8_value(filter_condition_val);
                    std::int32_t filter_condition_len = filter_condition_utf8_value.length();
                    if (filter_condition_len <= 0) {
                        return utils::CallbackError("condition filter value must be a non-empty string", callback);
                    }

                    std::string filter_condition(*filter_condition_utf8_value, static_cast<std::size_t>(filter_condition_len));

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
                        return utils::CallbackError("condition filter value must be =, !=, <, <=, >, or >=", callback);
                    }

                    v8::Local<v8::Value> filter_value_val = Nan::Get(filter_array,2).ToLocalChecked();
                    if (filter_value_val->IsNumber()) {
                        double filter_value_double = Nan::To<double>(filter_value_val).FromJust();
                        filter.value = filter_value_double;
                    } else if (filter_value_val->IsBoolean()) {
                        filter.value = Nan::To<bool>(filter_value_val).FromJust();
                    } else {
                        return utils::CallbackError("value filter value must be a number or boolean", callback);
                    }
                    query_data->basic_filter.filters.push_back(filter);
                }
            } else {
                return utils::CallbackError("'basic-filters' must be of the form [type, [filters]]", callback);
            }
        }
    }

    auto* worker = new Worker{std::move(query_data), new Nan::Callback{callback}};
    Nan::AsyncQueueWorker(worker);
}

} // namespace VectorTileQuery
