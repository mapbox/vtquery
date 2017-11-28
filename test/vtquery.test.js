'use strict';

const test = require('tape');
const path = require('path');
const vtquery = require('../lib/index.js');
const mvtf = require('@mapbox/mvt-fixtures');
const queue = require('d3-queue').queue;
const fs = require('fs');

const bufferSF = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/sanfrancisco/15-5238-12666.mvt'));

test('failure: fails without callback function', assert => {
  try {
    vtquery();
  } catch(err) {
    assert.ok(/last argument must be a callback function/.test(err.message), 'expected error message');
    assert.end();
  }
});

test('failure: buffers is not an array', assert => {
  vtquery('i am not an array', [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'first arg \'tiles\' must be an array of tile objects');
    assert.end();
  });
});

test('failure: buffers array is empty', assert => {
  const buffs = [];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'tiles\' array must be of length greater than 0');
    assert.end();
  });
});

test('failure: item in buffers array is not an object', assert => {
  const buffs = [
    'not an object'
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'items in \'tiles\' array must be objects');
    assert.end();
  });
});

test('failure: buffer value does not exist', assert => {
  const buffs = [
    {
      z: 0,
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'item in \'tiles\' array does not include a buffer value');
    assert.end();
  });
});

test('failure: buffer value is null', assert => {
  const buffs = [
    {
      buffer: null,
      z: 0,
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'buffer value in \'tiles\' array item is null or undefined');
    assert.end();
  });
});

test('failure: buffer value is not a buffer', assert => {
  const buffs = [
    {
      buffer: 'not a buffer',
      z: 0,
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'buffer value in \'tiles\' array item is not a true buffer');
    assert.end();
  });
});

test('failure: buffer object missing z value', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      // z: 0,
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'item in \'tiles\' array does not include a \'z\' value');
    assert.end();
  });
});

test('failure: buffer object missing x value', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      // x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'item in \'tiles\' array does not include a \'x\' value');
    assert.end();
  });
});

test('failure: buffer object missing y value', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      x: 0,
      // y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'item in \'tiles\' array does not include a \'y\' value');
    assert.end();
  });
});

test('failure: buffer object z value is not a number', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 'zero',
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'z\' value in \'tiles\' array item is not a number');
    assert.end();
  });
});

test('failure: buffer object x value is not a number', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      x: 'zero',
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'x\' value in \'tiles\' array item is not a number');
    assert.end();
  });
});

test('failure: buffer object y value is not a number', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      x: 0,
      y: 'zero'
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'y\' value in \'tiles\' array item is not a number');
    assert.end();
  });
});

test('failure: buffer object z value is negative', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: -10,
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'z\' value must not be less than zero');
    assert.end();
  });
});

test('failure: buffer object x value is negative', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      x: -5,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'x\' value must not be less than zero');
    assert.end();
  });
});

test('failure: buffer object y value is negative', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      x: 0,
      y: -4
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'y\' value must not be less than zero');
    assert.end();
  });
});

test('failure: buffer object z values don\'t match', assert => {
  const buffs = [
    {
      buffer: new Buffer('hey'),
      z: 0,
      x: 0,
      y: 0
    },
    {
      buffer: new Buffer('hi'),
      z: 1,
      x: 1,
      y: 1
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'z\' values do not match across all tiles in the \'tiles\' array');
    assert.end();
  });
});

test('failure: lnglat is not an array', assert => {
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], '[47.6117, -122.3444]', {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'second arg \'lnglat\' must be an array with [longitude, latitude] values');
    assert.end();
  });
});

test('failure: lnglat array is of length != 2', assert => {
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3, 'hi'], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'lnglat\' must be an array of [longitude, latitude]');
    assert.end();
  });
});

test('failure: longitude is not a number', assert => {
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], ['47.6', -122.3], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'lnglat values must be numbers');
    assert.end();
  });
});

test('failure: latitude is not a number', assert => {
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, '-122.3'], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'lnglat values must be numbers');
    assert.end();
  });
});

test('failure: options is not an object', assert => {
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], 'hi i am options', function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'options\' arg must be an object');
    assert.end();
  });
});

test('failure: options.radius is not a number', assert => {
  const opts = {
    radius: '4'
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'radius\' must be a number');
    assert.end();
  });
});

test('failure: options.radius is negative', assert => {
  const opts = {
    radius: -3
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'radius\' must be a positive number');
    assert.end();
  });
});

test('failure: options.results is not a number', assert => {
  const opts = {
    numResults: 'hi'
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'numResults\' must be a number');
    assert.end();
  });
});

test('failure: options.results is negative', assert => {
  const opts = {
    numResults: -10
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'numResults\' must be a positive number');
    assert.end();
  });
});

test('failure: options.layers is not an array', assert => {
  const opts = {
    layers: 'not array'
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'layers\' must be an array of strings');
    assert.end();
  });
});

test('failure: options.layers includes non string values', assert => {
  const opts = {
    layers: [8, 4]
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'layers\' values must be strings');
    assert.end();
  });
});

test('failure: options.layers includes empty strings', assert => {
  const opts = {
    layers: ['hello', '']
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'layers\' values must be non-empty strings');
    assert.end();
  });
});

test('failure: options.geometry is not a string', assert => {
  const opts = {
    geometry: 1234
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'geometry\' option must be a string');
    assert.end();
  });
});

test('failure: options.geometry does not equal an accepted value', assert => {
  const opts = {
    geometry: 'hexagon'
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'geometry\' must be \'point\', \'linestring\', or \'polygon\'');
    assert.end();
  });
});

test('failure: options.geometry must not be empty', assert => {
  const opts = {
    geometry: ''
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'geometry\' value must be a non-empty string');
    assert.end();
  });
});

test('options - defaults: success', assert => {
  // todo
  assert.end();
});

test('options - radius: all results within radius', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { numResults: 100, radius: 1000 }, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature) {
      assert.ok(feature.properties.tilequery.distance <= 1000, 'less than radius');
    });
    assert.end();
  });
});

test('options - radius: all results are in expected order', assert => {
  const expected = JSON.parse(fs.readFileSync(__dirname + '/fixtures/expected-order.json'));
  const buffer = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/chicago/13-2098-3045.mvt'));
  const ll = [-87.7987, 41.8451];
  vtquery([{buffer: buffer, z: 13, x: 2098, y: 3045}], ll, { radius: 50 }, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature, i) {
      let e = expected.features[i].properties;
      assert.equal(e.tilequery.distance, feature.properties.tilequery.distance, 'same distance');
      assert.equal(e.tilequery.layer, feature.properties.tilequery.layer, 'same layer');
      assert.equal(e.tilequery.geometry, feature.properties.tilequery.geometry, 'same geometry');
      if (feature.properties.type) {
        assert.equal(e.type, feature.properties.type, 'same type');
      }
    });
    assert.end();
  });
});

test('results with same exact distance return in expected order', assert => {
  // this query returns four results, three of which are the exact same distance and different types of features
  const buffer = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/chicago/13-2098-3045.mvt'));
  const ll = [-87.7964, 41.8675];
  vtquery([{buffer: buffer, z: 13, x: 2098, y: 3045}], ll, { radius: 10, layers: ['road'] }, function(err, result) {
    assert.equal(result.features[1].properties.type, 'pedestrian', 'is expected type');
    assert.equal(result.features[1].properties.tilequery.distance, 9.436356889343624, 'is the proper distance');
    assert.equal(result.features[2].properties.type, 'service:driveway', 'is expected type');
    assert.equal(result.features[2].properties.tilequery.distance, 9.436356889343624, 'is the proper distance');
    assert.equal(result.features[3].properties.type, 'turning_circle', 'is expected type');
    assert.equal(result.features[3].properties.tilequery.distance, 9.436356889343624, 'is the proper distance');
    assert.end();
  });
});

test('options - radius=0: only returns "point in polygon" results (on a building)', assert => {
  const buffer = bufferSF;
  const ll = [-122.4527, 37.7689]; // direct hit on a building
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { radius: 0, layers: ['building'] }, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'only one building returned');
    assert.deepEqual(result.features[0].properties.tilequery, { distance: 0.0, layer: 'building', geometry: 'polygon' }, 'expected tilequery info');
    assert.end();
  });
});

test('options - radius=0: returns only radius 0.0 results', assert => {
  const buffer = bufferSF;
  const ll = [-122.4527, 37.7689]; // direct hit on a building
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { radius: 0, numResults: 100 }, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature) {
      assert.ok(feature.properties.tilequery.distance == 0.0, 'radius 0.0');
    });
    assert.end();
  });
});

test('options - numResults: successfully limits results', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { numResults: 1, radius: 1000 }, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'expected length');
    assert.end();
  });
});

test('options - layers: successfully returns only requested layers', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, {radius: 2000, layers: ['poi_label']}, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature) {
      assert.equal(feature.properties.tilequery.layer, 'poi_label', 'proper layer');
    });
    assert.end();
  });
});

test('options - layers: returns zero results for a layer that does not exist - does not error', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, {radius: 2000, layers: ['i_am_not_real']}, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 0, 'no features');
    assert.end();
  });
});

test('options - radius: all results within radius', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { numResults: 100, radius: 1000 }, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature) {
      assert.ok(feature.properties.tilequery.distance <= 1000, 'less than radius');
    });
    assert.end();
  });
});

test('options - geometry: successfully returns only points', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, {radius: 2000, geometry: 'point'}, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 5, 'expected number of features');
    result.features.forEach(function(feature) {
      assert.equal(feature.properties.tilequery.geometry, 'point', 'expected original geometry');
    });
    assert.end();
  });
});

test('options - geometry: successfully returns only linestrings', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, {radius: 200, geometry: 'linestring'}, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 5, 'expected number of features');
    result.features.forEach(function(feature) {
      assert.equal(feature.properties.tilequery.geometry, 'linestring', 'expected original geometry');
    });
    assert.end();
  });
});


test('options - geometry: successfully returns only polygons', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, {radius: 200, geometry: 'polygon'}, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 5, 'expected number of features');
    result.features.forEach(function(feature) {
      assert.equal(feature.properties.tilequery.geometry, 'polygon', 'expected original geometry');
    });
    assert.end();
  });
});

test('success: returns all possible data value types', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800 // about the width of a z15 tile
  }
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    const p = result.features[0].properties;

    assert.equal(typeof p.string_value, 'string', 'expected value type');
    assert.equal(typeof p.bool_value, 'boolean', 'expected value type');
    assert.equal(typeof p.int_value, 'number', 'expected value type');
    assert.equal(typeof p.double_value, 'number', 'expected value type');
    assert.ok(p.double_value % 1 !== 0, 'expected decimal');
    assert.equal(typeof p.float_value, 'number', 'expected value type');
    assert.ok(p.float_value % 1 !== 0, 'expected decimal');
    assert.equal(typeof p.sint_value, 'number', 'expected value type');
    assert.ok(p.sint_value < 0, 'expected signedness');
    assert.equal(typeof p.uint_value, 'number', 'expected value type');
    assert.ifError(err);
    assert.end();
  });
});

test('success: does not throw on unknown geometry type', assert => {
  const tiles = [{buffer: mvtf.get('003').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800 // about the width of a z15 tile
  }
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.ifError(err);
    assert.notOk(result.features.length);
    assert.end();
  });
});

test('error: throws on invalid tile that is missing geometry type', assert => {
  const tiles = [{buffer: mvtf.get('004').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800 // about the width of a z15 tile
  }
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'Missing geometry field in feature (spec 4.2)', 'expected error message');
    assert.end();
  });
});
