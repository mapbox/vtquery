# vtquery

query a vector tile (or multiple) with a lng/lat and radius and get an array of results

uses vtzero, geometry.hpp, and spatial-algorithms

## Things to remember

-   dedupe based on ID in the feature if it exists
-   save a vector of data_views from vtzero once geometry
-   use vtzero to loop through and run geometry against closest_point - add data_view to vector for usage later, and keep moving
-   convert "radius" to tile coordinates
-   work within integers instead of doubles for geometry.hpp
-   determine where to load tile buffers - probably in javascript and pass an array of buffers in?
-   get feature properties from data_views once loop is finished
-   not returning geometry
-   how to handle "radius" across tiles?
    -   what about keeping the origin relative to the current tile so if you have a tile that is outside of the bounds of the origin point, the origin value increases (or goes negative) this will give distances as real numbers, rather than interpolating based on a relative origin

#### Z levels must be matching

## Loop architecture proposals

**1) double loop** - not the most efficient, but literal and easy to read

Collect all layers in the first loop and save to a `std::deque` to avoid memory reallocation. Loop through features in that new layer copy and save all features with "hits" to a `std::vector` which points to the layer in the deque. Once this loop is finished, use layer deque and features vector to get properties and return information.

**2) collect hit features, sort after**

Create a std::vector of features. Loop through layers, loop through their features, and run closeset point algorithm. If a hit, add to the vector of features with properties decoded. Once all features are collected, sort and return top `n` number of results specified by user.

**3) collect hit features, sort after**

Same loop as 2, but instead of adding to a vector only add to an array of a set length. Each time you get a feature hit, check against all other feature distances in vector and only add if the distance is smaller than the maximum distance already in the vector.

# Usage

## vtquery

Query an array of Mapbox Vector Tile buffers at a particular longitude and latitude and get back
features that either exist at the query point or features within a radius of the query point.

**Parameters**

-   `tiles` **[Array](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array).&lt;[Object](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)>** an array of tile objects with `buffer`, `z`, `x`, and `y` values
-   `LngLat` **[Array](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array).&lt;[Number](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number)>** a query point of longitude and latitude to query, `[lng, lat]`
-   `options` **[Object](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object)=**
    -   `options.radius` **[Number](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number)=** the radius to query for features. If your radius is larger than
        the extent of an individual tile, include multiple nearby buffers to collect a realstic list of features (optional, default `0`)
    -   `options.numResults` **[Number](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number)=** the number of results (features) returned from the query. (optional, default `5`)
    -   `options.layers` **[Array](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array).&lt;[String](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String)>=** an array of layer string names to query from. Default is all layers.
    -   `options.geometry` **[String](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String)=** only return features of a particular geometry type. Can be `point`, `linestring`, or `polygon`.
        Defaults to all geometry types.

# Develop

```bash
git clone git@github.com:mapbox/node-cpp-skel.git
cd node-cpp-skel

# Build binaries. This looks to see if there were changes in the C++ code. This does not reinstall deps.
make

# Run tests
make test

# Cleans your current builds and removes potential cache
make clean

# Cleans everything, including the things you download from the network in order to compile (ex: npm packages).
# This is useful if you want to nuke everything and start from scratch.
# For example, it's super useful for making sure everything works for Travis, production, someone else's machine, etc
make distclean

# This skel uses documentation.js to auto-generate API docs.
# If you'd like to generate docs for your code, you'll need to install documentation.js,
# and then add your subdirectory to the docs command in package.json
npm install -g documentation
npm run docs
```
