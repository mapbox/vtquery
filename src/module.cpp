#include "vtquery.hpp"
#include <nan.h>
// #include "your_code.hpp"

static void init(v8::Local<v8::Object> target) {
    // expose helloAsync method
    Nan::SetMethod(target, "vtquery", VectorTileQuery::vtquery);
}

NODE_MODULE(module, init) // NOLINT
