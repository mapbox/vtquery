#include "vtquery.hpp"
// #include "geometry_processors.hpp"
#include "util.hpp"

#include <algorithm>
#include <exception>
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

struct ResultObject {
    mapbox::feature::property_map properties;
    std::string layer_name;
    mapbox::geometry::point<double> coordinates;
    double distance;
    GeomType original_geometry_type;

    // custom constructor
    ResultObject(
        mapbox::feature::property_map&& props_map, // specifies an r-value completely, whatever had the memory beforehand, this now controls it
        std::string const& name,
        mapbox::geometry::point<double>&& p,
        double distance0,
        GeomType geom_type)
        : properties(std::move(props_map)),
          layer_name(name),
          coordinates(std::move(p)),
          distance(distance0),
          original_geometry_type(geom_type) {}

    // default move constructor
    ResultObject(ResultObject&&) = default;

    // non-copyable object - there is no way the code will ever copy
    ResultObject(ResultObject const&) = delete;

    // use the default destructor
    ~ResultObject() = default;
};

struct TileObject {
    TileObject(std::uint32_t z0,
               std::uint32_t x0,
               std::uint32_t y0,
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

    std::uint32_t z;
    std::uint32_t x;
    std::uint32_t y;
    vtzero::data_view data;
    Nan::Persistent<v8::Object> buffer_ref;
};

struct QueryData {
    explicit QueryData(std::uint32_t num_tiles)
        : tiles(),
          layers(),
          latitude(0.0),
          longitude(0.0),
          radius(0.0),
          zoom(0),
          num_results(5),
          geometry_filter_type(GeomType::all) {
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
    std::int32_t zoom;
    std::uint32_t num_results;
    GeomType geometry_filter_type;
};

struct property_value_visitor {
    v8::Local<v8::Object>& properties_obj;
    std::string const& key;

    template <typename T>
    void operator()(T) {}

    void operator()(bool v) {
        properties_obj->Set(Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Boolean>(v));
    }
    void operator()(uint64_t v) {
        properties_obj->Set(Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>(v));
    }
    void operator()(int64_t v) {
        properties_obj->Set(Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>(v));
    }
    void operator()(double v) {
        properties_obj->Set(Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::Number>(v));
    }
    void operator()(std::string const& v) {
        properties_obj->Set(Nan::New<v8::String>(key).ToLocalChecked(), Nan::New<v8::String>(v).ToLocalChecked());
    }
};

void set_property(mapbox::feature::property_map::value_type const& property,
                  v8::Local<v8::Object>& properties_obj) {
    mapbox::util::apply_visitor(property_value_visitor{properties_obj, property.first}, property.second);
}

struct CompareDistance {
    bool operator()(const ResultObject* r1, const ResultObject* r2) const {
        return r1->distance < r2->distance;
    }
};

struct Worker : Nan::AsyncWorker {
    using Base = Nan::AsyncWorker;

    std::unique_ptr<QueryData> query_data_;
    std::deque<ResultObject> results_;
    std::priority_queue<ResultObject*, std::vector<ResultObject*>, CompareDistance> results_queue_;

    Worker(std::unique_ptr<QueryData> query_data,
           Nan::Callback* cb)
        : Base(cb),
          query_data_(std::move(query_data)),
          results_(),
          results_queue_() {}

    // The Execute() function is getting called when the worker starts to run.
    // - You only have access to member variables stored in this worker.
    // - You do not have access to Javascript v8 objects here.
    void Execute() override {
        try {
            // Get the object from the unique_ptr
            QueryData const& data = *query_data_;

            // query point lng/lat geometry.hpp point (used for distance calculation later on)
            mapbox::geometry::point<double> query_lnglat{data.longitude, data.latitude};

            /* EACH TILE OBJECT

               At this point we've verified all tiles are of the same zoom level, so we work with that
               since it has been stored in the QueryData struct
            */
            for (auto const& tile_ptr : data.tiles) {

                // tile object
                TileObject const& tile_obj = *tile_ptr;

                // use vtzero to get geometry info
                vtzero::vector_tile tile{tile_obj.data};

                while (auto layer = tile.next_layer()) {

                    // should we query this layer? based on user "layer" option
                    // if there are items in the data.layers vector AND
                    // the current layer name is not a part of that list, continue
                    std::string layer_name = std::string(layer.name());
                    if (!data.layers.empty() && std::find(data.layers.begin(), data.layers.end(), layer_name) == data.layers.end()) {
                        continue;
                    }

                    /* QUERY POINT

                       The query point will be in tile coordinates eventually. This means we need to convert
                       lng/lat values into tile coordinates based on the z value of the current tile.

                       If the xy of the current tile do not intersect the lng/lat, let's determine how far away
                       the current buffer is in tile coordinates from the query point (now in tile coords). This
                       means we'll need to store the origin x/y tile values to refer back to when looping through
                       each tile.

                       We need to calculate this for every layer because the "extent" can be different per layer.
                    */
                    std::uint32_t extent = layer.extent();
                    mapbox::geometry::point<std::int64_t> query_point = utils::create_query_point(data.longitude, data.latitude, data.zoom, extent, tile_obj.x, tile_obj.y);
                    GeomType original_geometry_type = GeomType::unknown; // set to unknown but this will get overwritten
                    while (auto feature = layer.next_feature()) {
                        switch (feature.geometry_type()) {
                        case vtzero::GeomType::POINT: {
                            original_geometry_type = GeomType::point;
                            break;
                        }
                        case vtzero::GeomType::LINESTRING: {
                            original_geometry_type = GeomType::linestring;
                            break;
                        }
                        case vtzero::GeomType::POLYGON: {
                            original_geometry_type = GeomType::polygon;
                            break;
                        }
                        default: {
                            continue;
                        }
                        }

                        if (data.geometry_filter_type != GeomType::all && data.geometry_filter_type != original_geometry_type) {
                            continue;
                        }

                        // implement closest point algorithm on query geometry and the query point
                        auto const cp_info = mapbox::geometry::algorithms::closest_point(mapbox::vector_tile::extract_geometry<int64_t>(feature), query_point);

                        // check if cp_info.distance isn't less than zero, if so, this is an error and we can move on
                        if (cp_info.distance < 0.0) {
                            continue;
                        }

                        // if the distance is greater than 0.0, get the distance from the query point
                        // otherwise if the distance is 0.0, add it to the queue
                        if (cp_info.distance > 0.0) {
                            // convert x/y into lng/lat point
                            auto feature_lnglat = utils::convert_vt_to_ll(extent, tile_obj.z, tile_obj.x, tile_obj.y, cp_info);
                            auto meters = utils::distance_in_meters(query_lnglat, feature_lnglat);

                            // if the distance is within the threshold, save it
                            if (meters <= data.radius) {

                                // if the queue size is still smaller than number of results, just add the hit
                                // if the queue size is at its max, compare the top() result and only add if this new
                                // hit is smaller than the top result
                                if (results_queue_.size() < data.num_results) {
                                    auto props = mapbox::vector_tile::extract_properties(feature);
                                    results_.emplace_back(std::move(props),
                                                          layer_name,
                                                          std::move(feature_lnglat),
                                                          meters,
                                                          original_geometry_type);
                                    results_queue_.emplace(&results_.back());
                                } else if (meters < results_queue_.top()->distance) {
                                    results_queue_.pop();
                                    auto props = mapbox::vector_tile::extract_properties(feature);
                                    results_.emplace_back(std::move(props),
                                                          layer_name,
                                                          std::move(feature_lnglat),
                                                          meters,
                                                          original_geometry_type);
                                    results_queue_.emplace(&results_.back());
                                }
                            }
                        } else {
                            auto qp = mapbox::geometry::point<double>{data.longitude, data.latitude}; // original query lng/lat
                            auto props = mapbox::vector_tile::extract_properties(feature);

                            // if our results list is already full, remove the top() element and add the new one - this way the
                            // we'll always be removing the largest element by distance and replacing with a 0.0 result
                            if (results_queue_.size() == data.num_results) {
                                results_queue_.pop();
                            }

                            results_.emplace_back(std::move(props),
                                                  layer_name,
                                                  std::move(qp),
                                                  0.0,
                                                  original_geometry_type);
                            results_queue_.emplace(&results_.back());
                        }
                    } // end tile.layer.feature loop
                }     // end tile.layer loop
            }         // end tile loop
        } catch (const std::exception& e) {
            SetErrorMessage(e.what());
        }
    }

    // The HandleOKCallback() is getting called when Execute() successfully
    // completed.
    void HandleOKCallback() override {
        Nan::HandleScope scope;

        // number of results to loop through
        v8::Local<v8::Object> results_object = Nan::New<v8::Object>();
        v8::Local<v8::Array> features_array = Nan::New<v8::Array>(results_queue_.size());
        results_object->Set(Nan::New("type").ToLocalChecked(), Nan::New<v8::String>("FeatureCollection").ToLocalChecked());

        // for each result object
        while (!results_queue_.empty()) {
            auto const& feature = results_queue_.top(); // get reference to top item in results queue
            v8::Local<v8::Object> feature_obj = Nan::New<v8::Object>();
            feature_obj->Set(Nan::New("type").ToLocalChecked(), Nan::New<v8::String>("Feature").ToLocalChecked());

            // create geometry object
            v8::Local<v8::Object> geometry_obj = Nan::New<v8::Object>();
            geometry_obj->Set(Nan::New("type").ToLocalChecked(), Nan::New<v8::String>("Point").ToLocalChecked());
            v8::Local<v8::Array> coordinates_array = Nan::New<v8::Array>(2);
            coordinates_array->Set(0, Nan::New<v8::Number>(feature->coordinates.x)); // latitude
            coordinates_array->Set(1, Nan::New<v8::Number>(feature->coordinates.y)); // longitude
            geometry_obj->Set(Nan::New("coordinates").ToLocalChecked(), coordinates_array);
            feature_obj->Set(Nan::New("geometry").ToLocalChecked(), geometry_obj);

            // create properties object
            v8::Local<v8::Object> properties_obj = Nan::New<v8::Object>();
            for (auto const& prop : feature->properties) {
                set_property(prop, properties_obj);
            }

            // set properties.tilquery
            v8::Local<v8::Object> tilequery_properties_obj = Nan::New<v8::Object>();
            tilequery_properties_obj->Set(Nan::New("distance").ToLocalChecked(), Nan::New<v8::Number>(feature->distance));
            std::string og_geom = getGeomTypeString(feature->original_geometry_type);
            tilequery_properties_obj->Set(Nan::New("geometry").ToLocalChecked(), Nan::New<v8::String>(og_geom).ToLocalChecked());
            tilequery_properties_obj->Set(Nan::New("layer").ToLocalChecked(), Nan::New<v8::String>(feature->layer_name).ToLocalChecked());
            properties_obj->Set(Nan::New("tilequery").ToLocalChecked(), tilequery_properties_obj);

            // add properties to feature
            feature_obj->Set(Nan::New("properties").ToLocalChecked(), properties_obj);

            // add feature to features array
            features_array->Set(static_cast<uint32_t>(results_queue_.size() - 1), feature_obj);
            results_queue_.pop(); // remove item from results queue
        }

        results_object->Set(Nan::New("features").ToLocalChecked(), features_array);

        auto const argc = 2u;
        v8::Local<v8::Value> argv[argc] = {
            Nan::Null(), results_object};

        // Static cast done here to avoid 'cppcoreguidelines-pro-bounds-array-to-pointer-decay' warning with clang-tidy
        callback->Call(argc, static_cast<v8::Local<v8::Value>*>(argv));
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

    // TODO(sam) - create this at the end and include defaults as standalone values here and override before constructing
    std::unique_ptr<QueryData> query_data = std::make_unique<QueryData>(num_tiles);

    for (unsigned t = 0; t < num_tiles; ++t) {
        v8::Local<v8::Value> tile_val = tiles_arr_val->Get(t);
        if (!tile_val->IsObject()) {
            return utils::CallbackError("items in 'tiles' array must be objects", callback);
        }
        v8::Local<v8::Object> tile_obj = tile_val->ToObject();

        // check buffer value
        if (!tile_obj->Has(Nan::New("buffer").ToLocalChecked())) {
            return utils::CallbackError("item in 'tiles' array does not include a buffer value", callback);
        }
        v8::Local<v8::Value> buf_val = tile_obj->Get(Nan::New("buffer").ToLocalChecked());
        if (buf_val->IsNull() || buf_val->IsUndefined()) {
            return utils::CallbackError("buffer value in 'tiles' array item is null or undefined", callback);
        }
        v8::Local<v8::Object> buffer = buf_val->ToObject();
        if (!node::Buffer::HasInstance(buffer)) {
            return utils::CallbackError("buffer value in 'tiles' array item is not a true buffer", callback);
        }

        // check z,x,y values
        if (!tile_obj->Has(Nan::New("z").ToLocalChecked())) {
            return utils::CallbackError("item in 'tiles' array does not include a 'z' value", callback);
        }
        v8::Local<v8::Value> z_val = tile_obj->Get(Nan::New("z").ToLocalChecked());
        if (!z_val->IsNumber()) {
            return utils::CallbackError("'z' value in 'tiles' array item is not a number", callback);
        }
        std::int32_t z = z_val->Int32Value();
        if (z < 0) {
            return utils::CallbackError("'z' value must not be less than zero", callback);
        }
        // set zoom level in QueryData struct if it's the first iteration, otherwise verify zooms match
        if (t == 0) {
            query_data->zoom = z;
        } else {
            if (z != query_data->zoom) {
                return utils::CallbackError("'z' values do not match across all tiles in the 'tiles' array", callback);
            }
        }

        if (!tile_obj->Has(Nan::New("x").ToLocalChecked())) {
            return utils::CallbackError("item in 'tiles' array does not include a 'x' value", callback);
        }
        v8::Local<v8::Value> x_val = tile_obj->Get(Nan::New("x").ToLocalChecked());
        if (!x_val->IsNumber()) {
            return utils::CallbackError("'x' value in 'tiles' array item is not a number", callback);
        }
        std::int64_t x = x_val->IntegerValue();
        if (x < 0) {
            return utils::CallbackError("'x' value must not be less than zero", callback);
        }

        if (!tile_obj->Has(Nan::New("y").ToLocalChecked())) {
            return utils::CallbackError("item in 'tiles' array does not include a 'y' value", callback);
        }
        v8::Local<v8::Value> y_val = tile_obj->Get(Nan::New("y").ToLocalChecked());
        if (!y_val->IsNumber()) {
            return utils::CallbackError("'y' value in 'tiles' array item is not a number", callback);
        }
        std::int64_t y = y_val->IntegerValue();
        if (y < 0) {
            return utils::CallbackError("'y' value must not be less than zero", callback);
        }

        // in-place construction
        std::unique_ptr<TileObject> tile{new TileObject{static_cast<std::uint32_t>(z),
                                                        static_cast<std::uint32_t>(x),
                                                        static_cast<std::uint32_t>(y),
                                                        buffer}};
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

    v8::Local<v8::Value> lng_val = lnglat_val->Get(0);
    v8::Local<v8::Value> lat_val = lnglat_val->Get(1);
    if (!lng_val->IsNumber() || !lat_val->IsNumber()) {
        return utils::CallbackError("lnglat values must be numbers", callback);
    }
    query_data->longitude = lng_val->NumberValue();
    query_data->latitude = lat_val->NumberValue();

    // validate options object if it exists
    // defaults are set in the QueryData struct.
    if (info.Length() > 3) {

        if (!info[2]->IsObject()) {
            return utils::CallbackError("'options' arg must be an object", callback);
        }

        v8::Local<v8::Object> options = info[2]->ToObject();

        if (options->Has(Nan::New("radius").ToLocalChecked())) {
            v8::Local<v8::Value> radius_val = options->Get(Nan::New("radius").ToLocalChecked());
            if (!radius_val->IsNumber()) {
                return utils::CallbackError("'radius' must be a number", callback);
            }

            double radius = radius_val->NumberValue();
            if (radius < 0.0) {
                return utils::CallbackError("'radius' must be a positive number", callback);
            }

            query_data->radius = radius;
        }

        if (options->Has(Nan::New("numResults").ToLocalChecked())) {
            v8::Local<v8::Value> num_results_val = options->Get(Nan::New("numResults").ToLocalChecked());
            if (!num_results_val->IsNumber()) {
                return utils::CallbackError("'numResults' must be a number", callback);
            }

            // TODO(sam) using std::uint32_t results in a "comparison of unsigned expression" error
            // what's the best way to check that a number isn't negative but also assigning it a proper value?
            std::int32_t num_results = num_results_val->Int32Value();
            if (num_results < 0) {
                return utils::CallbackError("'numResults' must be a positive number", callback);
            }

            // TODO(sam) do we need to cast here? Or can we safely use an Int32Value knowing that it isn't negative
            // thanks to the check above
            query_data->num_results = static_cast<std::uint32_t>(num_results);
        }

        if (options->Has(Nan::New("layers").ToLocalChecked())) {
            v8::Local<v8::Value> layers_val = options->Get(Nan::New("layers").ToLocalChecked());
            if (!layers_val->IsArray()) {
                return utils::CallbackError("'layers' must be an array of strings", callback);
            }

            v8::Local<v8::Array> layers_arr = layers_val.As<v8::Array>();
            unsigned num_layers = layers_arr->Length();

            // only gather layers if there are some in the array
            if (num_layers > 0) {
                for (unsigned j = 0; j < num_layers; ++j) {
                    v8::Local<v8::Value> layer_val = layers_arr->Get(j);
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

        if (options->Has(Nan::New("geometry").ToLocalChecked())) {
            v8::Local<v8::Value> geometry_val = options->Get(Nan::New("geometry").ToLocalChecked());
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
    }

    auto* worker = new Worker{std::move(query_data), new Nan::Callback{callback}};
    Nan::AsyncQueueWorker(worker);
}

} // namespace VectorTileQuery
