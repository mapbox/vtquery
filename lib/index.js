"use strict";

/**
 * @name vtquery
 *
 * @param {Array<Object>} tiles an array of tile objects with `buffer`, `z`, `x`, and `y` values
 * @param {Array<Number>} LngLat a query point of longitude and latitude to query, `[lng, lat]`
 * @param {Object} [options]
 * @param {Number} [options.radius=0] the radius to query for features. If your radius is larger than
 * the extent of an individual tile, include multiple nearby buffers to collect a realstic list of features
 * @param {Number} [options.limit=5] limit the number of results/features returned from the query. Minimum is 1, maximum is 1000 (to avoid pre allocating large amounts of memory)
 * @param {Array<String>} [options.layers] an array of layer string names to query from. Default is all layers.
 * @param {String} [options.geometry] only return features of a particular geometry type. Can be `point`, `linestring`, or `polygon`.
 * Defaults to all geometry types.
 * @param {String} [options.dedup=true] perform deduplication of features based on shared layers, geometry, IDs and matching
 * properties.
 * @param {Array<String,Array>} [options.basic-filters] - and expression-like filter out Number or Boolean properties based on
 * the following condtions: `=, !=, <, <=, >, >=`. The first item must be the value "any" or "all" whether any or all filters
 * must evaluate to true.
 *
 * @example
 * const vtquery = require('@mapbox/vtquery');
 * const fs = require('fs');
 *
 * const tiles = [
 *   { buffer: fs.readFileSync('./path/to/tile.mvt'), z: 15, x: 5238, y: 12666 }
 * ];
 *
 * const options = {
 *   radius: 0,
 *   limit: 5,
 *   geometry: 'polygon',
 *   layers: ['building', 'parks'],
 *   dedupe: true,
 *   'basic-filters': ['all', [['population', '>', 10], ['population', '<', 1000]]]
 * };
 *
 * vtquery(tiles, [-122.4477, 37.7665], options, function(err, result) {
 *   if (err) throw err;
 *   console.log(result); // geojson FeatureCollection
 * });
 */
module.exports = require('./binding/vtquery.node').vtquery;
