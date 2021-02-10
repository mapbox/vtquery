#include "vtquery.hpp"
#include <napi.h>

auto init(Napi::Env env, Napi::Object exports) -> Napi::Object {
    exports.Set(Napi::String::New(env, "vtquery"), Napi::Function::New(env, VectorTileQuery::vtquery));
    return exports;
}

NODE_API_MODULE(module, init) // NOLINT
