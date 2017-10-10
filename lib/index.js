"use strict";

/**
 * Query an array of Mapbox Vector Tile buffers at a particular longitude and latitude and get back
 * features that either exist at the query point or features within a radius of the query point.
 * @name vtquery
 *
 * @param {Array<Object>} buffers - an array of buffer objects with `buffer`, `z`, `x`, and `y` values specified.
 * @param {Array<Number>} LngLat - a query point of longitude and latitude to query, `[lng, lat]`
 * @param {Object} [options]
 * @param {Number} [options.radius=0] radius - the radius to query for features. If your radius is larger than
 * the extent of an individual tile, include multiple nearby buffers to collect a realstic list of features
 * @param {Number} [options.results=5] results - the number of results/features returned from the query.
 * @param {Array<String>} [options.layers] layers - an array of layer string names to query from. Default is all layers.
 * @param {String} [options.  geometryType] - only return features of a particular geometry type. Can be `point`, `linestring`, or `polygon`.
 * Defaults to all geometry types.
 *
 */
module.exports = require('./binding/vtquery.node').vtquery;
