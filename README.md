# vtquery

query a vector tile (or multiple) with a lng/lat and radius and get an array of results

uses vtzero, geometry.hpp, and spatial-algorithms

## Things to remember

* dedupe based on ID in the feature if it exists
* save a vector of data_views from vtzero once geometry
* use vtzero to loop through and run geometry against closest_point - add data_view to vector for usage later, and keep moving
* convert "radius" to tile coordinates
* work within integers instead of doubles for geometry.hpp
* determine where to load tile buffers - probably in javascript and pass an array of buffers in?
* get feature properties from data_views once loop is finished
* not returning geometry
* how to handle "radius" across tiles?
  * what about keeping the origin relative to the current tile so if you have a tile that is outside of the bounds of the origin point, the origin value increases (or goes negative) this will give distances as real numbers, rather than interpolating based on a relative origin

## Loop architecture proposals

**1) double loop** - not the most efficient, but literal and easy to read

Collect all layers in the first loop and save to a `std::deque` to avoid memory reallocation. Loop through features in that new layer copy and save all features with "hits" to a `std::vector` which points to the layer in the deque. Once this loop is finished, use layer deque and features vector to get properties and return information.

**2) collect hit features, sort after**

Create a std::vector of features. Loop through layers, loop through their features, and run closeset point algorithm. If a hit, add to the vector of features with properties decoded. Once all features are collected, sort and return top `n` number of results specified by user.

**3) collect hit features, sort after**

Same loop as 2, but instead of adding to a vector only add to an array of a set length. Each time you get a feature hit, check against all other feature distances in vector and only add if the distance is smaller than the maximum distance already in the vector.
