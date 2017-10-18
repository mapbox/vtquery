#include "vtquery.hpp"
#include "util.hpp"
#include "geometry_processors.hpp"
#include "reproject.hpp"

#include <deque>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <memory>
#include <mapbox/geometry/geometry.hpp>
#include <mapbox/geometry/algorithms/closest_point.hpp>
#include <mapbox/geometry/algorithms/closest_point_impl.hpp>
#include <vtzero/vector_tile.hpp>
#include <vtzero/types.hpp>

namespace VectorTileQuery {

struct ResultObject {
    ResultObject(
        std::uint32_t z0,
        std::uint32_t x0,
        std::uint32_t y0,
        mapbox::geometry::algorithms::closest_point_info<std::int64_t> cp_info0,
        vtzero::feature feature0)
      : z(z0),
        x(x0),
        y(y0),
        cp_info(cp_info0),
        feature(feature0) {}

    ~ResultObject() = default;

    std::uint32_t z;
    std::uint32_t x;
    std::uint32_t y;
    mapbox::geometry::algorithms::closest_point_info<std::int64_t> cp_info;
    vtzero::feature feature;
};

struct TileObject {
    TileObject(std::uint32_t z0,
        std::uint32_t x0,
        std::uint32_t y0,
        v8::Local<v8::Object> buffer)
      : z(z0),
        x(x0),
        y(y0),
        data(node::Buffer::Data(buffer),node::Buffer::Length(buffer)) {
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
    TileObject( TileObject const& ) = delete;
    TileObject& operator=(TileObject const& ) = delete;

    // non-movable
    TileObject( TileObject && ) = delete;
    TileObject& operator=(TileObject && ) = delete;

    std::uint32_t z;
    std::uint32_t x;
    std::uint32_t y;
    vtzero::data_view data;
    Nan::Persistent<v8::Object> buffer_ref;
};

struct QueryData {
    QueryData(std::uint32_t num_tiles) {
        tiles.reserve(num_tiles);
    }
    ~QueryData() = default;

    // guarantee that objects are not being copied by deleting the
    // copy and move definitions

    // non-copyable
    QueryData( QueryData const& ) = delete;
    QueryData& operator=(QueryData const& ) = delete;

    // non-movable
    QueryData( QueryData && ) = delete;
    QueryData& operator=(QueryData && ) = delete;

    // buffers object thing
    std::vector<std::unique_ptr<TileObject>> tiles;

    // lng/lat
    double latitude = 0.0;
    double longitude = 0.0;

    // zoom - determined by tiles array in validation
    std::int32_t zoom = 0;

    // options
    double radius = 0.0;
    std::uint32_t num_results = 5;
    std::vector<std::string> layers{};
    std::string geometry{};
};

struct Worker : Nan::AsyncWorker {
    using Base = Nan::AsyncWorker;

    Worker(std::unique_ptr<QueryData> query_data,
           Nan::Callback* callback)
        : Base(callback),
          query_data_(std::move(query_data)) {}

    // The Execute() function is getting called when the worker starts to run.
    // - You only have access to member variables stored in this worker.
    // - You do not have access to Javascript v8 objects here.
    void Execute() override {
        try {
            // Get the object from the unique_ptr
            QueryData const& data = *query_data_;

            /* QUERY POINT

               The query point will be in tile coordinates eventually. This means we need to convert
               lng/lat values into tile coordinates based on the z value of the current tile.

               If the xy of the current tile do not intersect the lng/lat, let's determine how far away
               the current buffer is in tile coordinates from the query point (now in tile coords). This
               means we'll need to store the origin x/y tile values to refer back to when looping through
               each tile.
            */

            // convert lng/lat to tile coordinates with z value (also get tile XY location)
            // std::int64_t origin_X = ...
            // std::int64_t origin_Y = ...
            // std::int32_t origin_tileX = ...
            // std::int32_t origin_tileY = ...
            // std::uint32_t extent = ...

            // storage mechanisms for features and layers
            std::deque<vtzero::layer> layers;
            std::vector<ResultObject> features;

            /* EACH TILE OBJECT

               At this point we've verified all tiles are of the same zoom level, so we work with that
               since it has been stored in the QueryData struct
            */
            for (auto const& tile_ptr : data.tiles) {

                // tile object
                TileObject const& tile_obj = *tile_ptr;

                // calculate a relative for the current tile based on origin x/y and origin tile x/y values
                // if the current tile x/y match the origin x/y, just create a query_point then
                // if (tile_obj.x == origin_tileX && tile_obj.y == origin_tileY) {
                //     mapbox::geometry::point<std::int64_t> query_point{origin_X,origin_Y};
                // } else {
                //     std::utin32_t diff_tileX = tile_obj.x - origin_tileX;
                //     std::utin32_t diff_tileY = tile_obj.y - origin_tileY;
                //     std::int64_t nX = -(diff_tileX * extent) + origin_X;
                //     std::int64_t nY = -(diff_tileY * extent) + origin_Y;
                //     mapbox::geometry::point<std::int64_t> query_point{nX,nY};
                // }
                mapbox::geometry::point<std::int64_t> query_point{10,15};

                // use vtzero to get geometry info
                vtzero::vector_tile tile{tile_obj.data};

                while (auto layer = tile.next_layer()) {
                    // storing layers to get properties afterwards
                    // this is probably not the most efficient, but we're getting it working
                    layers.emplace_back(layer);
                    // auto & layer_ref = layers.back();
                    while (auto feature = layer.next_feature()) {

                        // if we encounter an UNKNOWN geometry, skip the feature
                        bool skip_feature = false;

                        // create a dummy default geometry structure that will be updated in the switch statement below
                        mapbox::geometry::geometry<std::int64_t> query_geometry = mapbox::geometry::point<std::int64_t>();
                        // get the geometry type and decode the geometry into mapbox::geometry data structures
                        switch (feature.geometry_type()) {
                            case vtzero::GeomType::POINT: {
                                mapbox::geometry::multi_point<std::int64_t> mpoint;
                                point_processor proc_point(mpoint);
                                vtzero::decode_point_geometry(feature.geometry(), false, proc_point);
                                query_geometry = std::move(mpoint);
                                break;
                            }
                            case vtzero::GeomType::LINESTRING: {
                                mapbox::geometry::multi_line_string<std::int64_t> mline;
                                linestring_processor proc_line(mline);
                                vtzero::decode_linestring_geometry(feature.geometry(), false, proc_line);
                                query_geometry = std::move(mline);
                                break;
                            }
                            case vtzero::GeomType::POLYGON: {
                                mapbox::geometry::multi_polygon<std::int64_t> mpoly;
                                polygon_processor proc_poly(mpoly);
                                vtzero::decode_polygon_geometry(feature.geometry(), false, proc_poly);
                                query_geometry = std::move(mpoly);
                                break;
                            }
                            default: {
                                skip_feature = true;
                                break;
                            }
                        }

                        if (!skip_feature) {
                            // implement closest point algorithm on query geometry and the query point
                            const auto cp_info = mapbox::geometry::algorithms::closest_point(query_geometry, query_point);

                            // if the distance is within the threshold, save it
                            std::clog << "distance: " << cp_info.distance << std::endl;
                            if (cp_info.distance <= data.radius) {
                                ResultObject r(tile_obj.z, tile_obj.x, tile_obj.y, cp_info, feature);
                                features.push_back(r);
                                std::clog << "hit!" << std::endl;
                            }
                        }
                    } // end tile.layer.feature loop

                    // if we have hits
                    // decode properties for each hit, with a reference to the layer keys/values
                    // (included as a pointer in vtzero::feature)

                } // end tile.layer loop

            } // end tile loop

            /* GET 'n' results based on num_results */

            std::clog << "\n---\ntotal features: " << features.size() << "\n";
            for (auto const& feature : features) {
                std::clog << "\n" << (&feature - &features[0]) << ")\n";
                std::clog << "x: " << feature.x << ", y: " << feature.y << ", distance: " << feature.cp_info.distance << "\n";

                // lng lat
                // const auto ll = tile_to_long_lat(4096, feature.z, feature.x, feature.y, feature.cp_info.x, feature.cp_info.y);
                // std::clog << "lng: " << ll.first << ", lat: " << ll.second << "\n";

                while (auto property = feature.feature.next_property()) {
                // for (auto const prop : feature.feature) {
                    // get key as string

                    std::string key = std::string{prop.key()};
                    // get value as mapbox variant
                    auto v = vtzero::convert_property_value<variant_type>(prop.value());
                    // print them out
                    std::clog << " - " << key << ": ";
                    mapbox::util::apply_visitor(utils::print_variant(), v);
                    std::clog << "\n";

                    // lng lat
                    std::uint32_t extent = 4096; // TODO: pull from layer.extent()
                    const auto ll = tile_to_long_lat(extent, feature.z, feature.x, feature.y, feature.cp_info.x, feature.cp_info.y);
                    std::clog << "lng: " << ll.first << ", lat: " << ll.second << "\n";
                }
            }

        } catch (const std::exception& e) {
            SetErrorMessage(e.what());
        }
    }

    // The HandleOKCallback() is getting called when Execute() successfully
    // completed.
    // - In case Execute() invoked SetErrorMessage("") this function is not
    // getting called.
    // - You have access to Javascript v8 objects again
    // - You have to translate from C++ member variables to Javascript v8 objects
    // - Finally, you call the user's callback with your results
    void HandleOKCallback() override {
        Nan::HandleScope scope;

        // TODO(sam) turn results vector into JSON object
        QueryData const& data = *query_data_;
        std::string result = data.geometry;

        // std::string result("longitude: " + lng);
        const auto argc = 2u;
        v8::Local<v8::Value> argv[argc] = {
            Nan::Null(), Nan::New<v8::String>(result).ToLocalChecked()
        };

        // Static cast done here to avoid 'cppcoreguidelines-pro-bounds-array-to-pointer-decay' warning with clang-tidy
        callback->Call(argc, static_cast<v8::Local<v8::Value>*>(argv));
    }

    std::unique_ptr<QueryData> query_data_;
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

    std::unique_ptr<QueryData> query_data{new QueryData(num_tiles)};

    for (unsigned t=0; t < num_tiles; ++t) {
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
                for (unsigned j=0; j < num_layers; ++j) {
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

            if (geometry != "point" && geometry != "linestring" && geometry != "polygon") {
                return utils::CallbackError("'geometry' must be 'point', 'linestring', or 'polygon'", callback);
            }

            query_data->geometry = geometry;
        }
    }

    auto* worker = new Worker{std::move(query_data), new Nan::Callback{callback}};
    Nan::AsyncQueueWorker(worker);
}

} // namespace VectorTileQuery
