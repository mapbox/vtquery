## 0.4.1

* Improved `basic-filters` performance
    * Changed map to unordered map
    * Replaced function parameter copies with const references
* Added additionaly tests

## 0.4.0

* Add `basic-filters` option for filtering on Number or Boolean properties. [#110](https://github.com/mapbox/vtquery/pull/110)
* Remove tests for node v4 + v6

## 0.3.0

* now adding feature.id to response JSON [#90](https://github.com/mapbox/vtquery/pull/90)
* added node v10 support [#99](https://github.com/mapbox/vtquery/pull/99)
* now no longer bundling `node-pre-gyp` as a `bundledDependency`
* now supporting on-the-fly decoding of tiles that are gzip-compressed [#95](https://github.com/mapbox/vtquery/pull/95)
* now sorting with `std::stable_sort` to ensure results are stable across platforms [#91](https://github.com/mapbox/vtquery/pull/91)
* set maximum results limit to 1000 to avoid pre-allocating large amounts of memory [#70](https://github.com/mapbox/vtquery/issues/70)
* use `int` for z/x/y values instead of `std::uint64_t` and casting [#87](https://github.com/mapbox/vtquery/issues/87)

Milestone link: https://github.com/mapbox/vtquery/milestone/3

## 0.2.1

* move `Nan::HandleScope` out of `try` statement, so it's within scope for `catch` statement [#84](https://github.com/mapbox/vtquery/pull/84)
* update protozero@1.6.2 [#85](https://github.com/mapbox/vtquery/pull/85)

## 0.2.0

* allow different zoom levels between tiles [#76](https://github.com/mapbox/vtquery/pull/76)
* add Node.js v8.x support, start building binaries for this version [#80](https://github.com/mapbox/vtquery/pull/80)
* remove .npmignore to reduce package size [#79](https://github.com/mapbox/vtquery/pull/79)
* update vtzero to 1.0.1 [#83](https://github.com/mapbox/vtquery/pull/83)
* add a try/catch to the HandleOKCallback method to avoid dangerous aborts [#69](https://github.com/mapbox/vtquery/issues/69)

## 0.1.0

* first official release! :tada: Take a look at the [0.1.0 milestone](https://github.com/mapbox/vtquery/milestone/1) to learn more about what went into this version.

## 0.1.0-alpha2

* update vtzero@088ec09

## 0.1.0-alpha1

* first release
