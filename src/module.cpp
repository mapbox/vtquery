#include "vtquery.hpp"
#include <napi.h>

Napi::Object init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "vtquery"), Napi::Function::New(env, VectorTileQuery::vtquery));
    return exports;
}

NODE_API_MODULE(module, init) // NOLINT
