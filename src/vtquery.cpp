#include "vtquery.hpp"
#include "util.hpp"

#include <deque>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
// #include <mapbox/geometry/geometry.hpp>
// #include <mapbox/geometry/algorithms/closest_point.hpp>
#include <vtzero/vector_tile.hpp>
// #include <mapbox/variant.hpp>

namespace VectorTileQuery {

struct Worker : Nan::AsyncWorker
{
    using Base = Nan::AsyncWorker;

    Worker(Nan::Callback* callback)
        : Base(callback), result_{"hello"} {}

    // The Execute() function is getting called when the worker starts to run.
    // - You only have access to member variables stored in this worker.
    // - You do not have access to Javascript v8 objects here.
    void Execute() override
    {
        try
        {
            // do the stuff here
        }
        catch (const std::exception& e)
        {
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
    void HandleOKCallback() override
    {
        Nan::HandleScope scope;

        const auto argc = 2u;
        v8::Local<v8::Value> argv[argc] = {
            Nan::Null(), Nan::New<v8::String>(result_).ToLocalChecked()};

        // Static cast done here to avoid 'cppcoreguidelines-pro-bounds-array-to-pointer-decay' warning with clang-tidy
        callback->Call(argc, static_cast<v8::Local<v8::Value>*>(argv));
    }

    std::string result_;
};

NAN_METHOD(vtquery)
{
    // set defaults
    // float default_radius = 0;
    // std::uint32_t default_results = 5;
    // std::string default_geometryType = "all";
    // std::string default_layers = "all";

    // validate callback function
    v8::Local<v8::Value> callback_val = info[info.Length() - 1];
    if (!callback_val->IsFunction())
    {
        Nan::ThrowError("last argument must be a callback function");
        return;
    }
    v8::Local<v8::Function> callback = callback_val.As<v8::Function>();

    // validate buffers
      // must have `buffer` buffer
      // must have `z` int
      // must have `x` int
      // must have `y` int
    if (!info[0]->IsArray())
    {
        return utils::CallbackError("first arg 'buffers' must be an array of objects", callback);
    }

    // validate lng/lat array
    if (!info[1]->IsArray())
    {
        return utils::CallbackError("second arg 'lnglat' must be an array with [longitude, latitude] values", callback);
    }

    // v8::Local<v8::Array> lnglat_val(info[1]);
    v8::Local<v8::Array> lnglat_val = info[1].As<v8::Array>();
    if (lnglat_val->Length() != 2)
    {
        return utils::CallbackError("'lnglat' must be an array of [longitude, latitude]", callback);
    }

    v8::Local<v8::Value> lng_val = lnglat_val->Get(0);
    v8::Local<v8::Value> lat_val = lnglat_val->Get(1);
    if (!lng_val->IsNumber() || !lat_val->IsNumber())
    {
        return utils::CallbackError("lnglat values must be numbers", callback);
    }
    // double lng = lng_val->NumberValue();
    // double lat = lat_val->NumberValue();

    auto* worker = new Worker{new Nan::Callback{callback}};
    Nan::AsyncQueueWorker(worker);
}

} // namespace standalone_async
