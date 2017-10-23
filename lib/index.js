"use strict";

/**
 * @name vtquery
 *
 * @param {Array<Object>} tiles an array of tile objects with `buffer`, `z`, `x`, and `y` values
 * @param {Array<Number>} LngLat a query point of longitude and latitude to query, `[lng, lat]`
 * @param {Object} [options]
 * @param {Number} [options.radius=0] the radius to query for features. If your radius is larger than
 * the extent of an individual tile, include multiple nearby buffers to collect a realstic list of features
 * @param {Number} [options.results=5] the number of results/features returned from the query.
 * @param {Array<String>} [options.layers] an array of layer string names to query from. Default is all layers.
 * @param {String} [options.geometry] only return features of a particular geometry type. Can be `point`, `linestring`, or `polygon`.
 * Defaults to all geometry types.
 *
 * @example
 * const vtquery = require('@mapbox/vtquery');
 *
 * vtquery(tiles, lnglat, options, callback);
 */
module.exports = require('./binding/vtquery.node').vtquery;
