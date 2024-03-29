'use strict';

const test = require('tape');
const path = require('path');
const vtquery = require('../lib/index.js');
const mvtf = require('@mapbox/mvt-fixtures');
const queue = require('d3-queue').queue;
const fs = require('fs');
const zlib = require('zlib')

const bufferSF = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/sanfrancisco/15-5238-12666.mvt'));

function checkClose(a, b, epsilon) {
  return 1*(a-b) < epsilon;
}

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
      buffer: Buffer.from('hey'),
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
      buffer: Buffer.from('hey'),
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
      buffer: Buffer.from('hey'),
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
      buffer: Buffer.from('hey'),
      z: 'zero',
      x: 0,
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'z\' value in \'tiles\' array item is not an int32');
    assert.end();
  });
});

test('failure: buffer object x value is not a number', assert => {
  const buffs = [
    {
      buffer: Buffer.from('hey'),
      z: 0,
      x: 'zero',
      y: 0
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'x\' value in \'tiles\' array item is not an int32');
    assert.end();
  });
});

test('failure: buffer object y value is not a number', assert => {
  const buffs = [
    {
      buffer: Buffer.from('hey'),
      z: 0,
      x: 0,
      y: 'zero'
    }
  ];
  vtquery(buffs, [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'y\' value in \'tiles\' array item is not an int32');
    assert.end();
  });
});

test('failure: buffer object z value is negative', assert => {
  const buffs = [
    {
      buffer: Buffer.from('hey'),
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
      buffer: Buffer.from('hey'),
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
      buffer: Buffer.from('hey'),
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

test('failure: lnglat is not an array', assert => {
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], '[47.6117, -122.3444]', {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'second arg \'lnglat\' must be an array with [longitude, latitude] values');
    assert.end();
  });
});

test('failure: lnglat array is of length != 2', assert => {
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3, 'hi'], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'lnglat\' must be an array of [longitude, latitude]');
    assert.end();
  });
});

test('failure: longitude is not a number', assert => {
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], ['47.6', -122.3], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'lnglat values must be numbers');
    assert.end();
  });
});

test('failure: latitude is not a number', assert => {
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, '-122.3'], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'lnglat values must be numbers');
    assert.end();
  });
});

test('failure: options is not an object', assert => {
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], 'hi i am options', function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'options\' arg must be an object');
    assert.end();
  });
});

test('failure: options.dedupe is not a boolean', assert => {
  const opts = {
    dedupe: 'yes please'
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'dedupe\' must be a boolean');
    assert.end();
  });
});

test('failure: options.radius is not a number', assert => {
  const opts = {
    radius: '4'
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'radius\' must be a number');
    assert.end();
  });
});

test('failure: options.radius is negative', assert => {
  const opts = {
    radius: -3
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'radius\' must be a positive number');
    assert.end();
  });
});

test('failure: options.results is not a number', assert => {
  const opts = {
    limit: 'hi'
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'limit\' must be a number');
    assert.end();
  });
});

test('failure: options.limit is negative', assert => {
  const opts = {
    limit: -10
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'limit\' must be 1 or greater');
    assert.end();
  });
});

test('failure: options.limit is 0', assert => {
  const opts = {
    limit: 0
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'limit\' must be 1 or greater');
    assert.end();
  });
});

test('failure: options.limit is greater than 1000', assert => {
  const opts = {
    limit: 2000
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'limit\' must be less than 1000');
    assert.end();
  });
});

test('failure: options.layers is not an array', assert => {
  const opts = {
    layers: 'not array'
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'layers\' must be an array of strings');
    assert.end();
  });
});

test('failure: options.layers includes non string values', assert => {
  const opts = {
    layers: [8, 4]
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'layers\' values must be strings');
    assert.end();
  });
});

test('failure: options.layers includes empty strings', assert => {
  const opts = {
    layers: ['hello', '']
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'layers\' values must be non-empty strings');
    assert.end();
  });
});

test('failure: options.geometry is not a string', assert => {
  const opts = {
    geometry: 1234
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'geometry\' option must be a string');
    assert.end();
  });
});

test('failure: options.geometry does not equal an accepted value', assert => {
  const opts = {
    geometry: 'hexagon'
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'geometry\' must be \'point\', \'linestring\', or \'polygon\'');
    assert.end();
  });
});

test('failure: options.geometry must not be empty', assert => {
  const opts = {
    geometry: ''
  };
  vtquery([{buffer: Buffer.from('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
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
  const expected_path = __dirname + '/fixtures/expected-sf.json';
  const expected = JSON.parse(fs.readFileSync(expected_path));
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { limit: 100, radius: 1000 }, function(err, result) {
    assert.ifError(err);
    if (process.env.UPDATE) {
      fs.writeFileSync(expected_path,JSON.stringify(result,null,2));
    }
    result.features.forEach(function(feature, i) {
      let e = expected.features[i];
      assert.equal(e.id, feature.id, 'same id');
      assert.ok(feature.properties.tilequery.distance <= 1000, 'less than radius');
      assert.ok(checkClose(e.properties.tilequery.distance, feature.properties.tilequery.distance, 1e-6), 'expected distance');
      assert.equal(e.properties.tilequery.layer, feature.properties.tilequery.layer, 'same layer');
      assert.equal(e.properties.tilequery.geometry, feature.properties.tilequery.geometry, 'same geometry');
      if (feature.properties.type) {
        assert.equal(e.properties.type, feature.properties.type, 'same type');
      }
    });
    assert.end();
  });
});

test('options - radius: all results are in expected order', assert => {
  const expected_path = __dirname + '/fixtures/expected-order.json';
  const expected = JSON.parse(fs.readFileSync(expected_path));
  const buffer = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/chicago/13-2098-3045.mvt'));
  const ll = [-87.7987, 41.8451];
  vtquery([{buffer: buffer, z: 13, x: 2098, y: 3045}], ll, { radius: 50 }, function(err, result) {
    assert.ifError(err);
    if (process.env.UPDATE) {
      fs.writeFileSync(expected_path,JSON.stringify(result,null,2));
    }
    result.features.forEach(function(feature, i) {
      let e = expected.features[i];
      assert.equal(e.id, feature.id, 'same id');
      assert.ok(checkClose(e.properties.tilequery.distance, feature.properties.tilequery.distance, 1e-6), 'expected distance');
      assert.equal(e.properties.tilequery.layer, feature.properties.tilequery.layer, 'same layer');
      assert.equal(e.properties.tilequery.geometry, feature.properties.tilequery.geometry, 'same geometry');
      if (feature.properties.type) {
        assert.equal(e.properties.type, feature.properties.type, 'same type');
      }
    });
    assert.end();
  });
});

test('options - radius=0: only returns "point in polygon" results (on a building)', assert => {
  const buffer = bufferSF;
  const ll = [-122.4527, 37.7689]; // direct hit on a building
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { radius: 0, layers: ['building'] }, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'only one building returned');
    assert.equal(result.features[0].properties.height, 7, 'expected property value');
    assert.deepEqual(result.features[0].properties.tilequery, { distance: 0.0, layer: 'building', geometry: 'polygon' }, 'expected tilequery info');
    assert.end();
  });
});

test('options - radius=0: returns only radius 0.0 results', assert => {
  const buffer = bufferSF;
  const ll = [-122.4527, 37.7689]; // direct hit on a building
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { radius: 0, limit: 100 }, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature) {
      assert.ok(feature.properties.tilequery.distance == 0.0, 'radius 0.0');
    });
    assert.end();
  });
});

test('options - radius=1000, direct_hit_polygon: returns points/lines within 1000.0 and polygons that are direct hits', assert => {
  const buffer = bufferSF;
  const ll = [-122.4527, 37.7689]; // direct hit on a building
  let polygon_count = 0;
  let non_polygon_count = 0;
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { radius: 1000, limit: 100 , direct_hit_polygon: true}, function(err, result) {
    assert.ifError(err);
    result.features.forEach(function(feature) {
      if (feature.properties.tilequery.geometry === 'polygon') {
        assert.ok(feature.properties.tilequery.distance == 0.0, 'radius 0.0');
        polygon_count += 1
      } else {
        assert.ok(feature.properties.tilequery.distance <= 1000.0, 'radius 1000.0');
        non_polygon_count += 1;
      }
    });
    assert.ok(polygon_count > 0, 'at least one polygon');
    assert.ok(non_polygon_count > 0, 'at least one non-polygon');
    assert.end();
  });
});

test('options - limit: successfully limits results', assert => {
  const buffer = bufferSF;
  const ll = [-122.4477, 37.7665]; // direct hit
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { limit: 1, radius: 1000 }, function(err, result) {
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
  vtquery([{buffer: buffer, z: 15, x: 5238, y: 12666}], ll, { limit: 100, radius: 1000 }, function(err, result) {
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

// two painted tiles one above the other (Y-axis) - confirming deduplication is preventing
// returning results that are actually tile borders
test('options - dedupe: returns only one result when dedupe is on', assert => {
  const buffer = fs.readFileSync(__dirname + '/fixtures/canada-covered-square.mvt');
  const tiles = [
    {buffer: buffer, z: 11, x: 449, y: 693}, // hit tile
    {buffer: buffer, z: 11, x: 449, y: 694},
    {buffer: buffer, z: 11, x: 448, y: 694},
    {buffer: buffer, z: 11, x: 448, y: 693}
  ];
  const opts = {
    radius: 10000 // about the width of a z15 tile
  }
  vtquery(tiles, [-100.9797421880223, 50.075683473759085], opts, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'only one feature');
    assert.equal(result.features[0].properties.tilequery.distance, 0, 'expected distance');
    assert.equal(result.features[0].properties.id, 'CA', 'expected id');
    assert.end();
  });
});

test('options - dedupe: removes results from deque when a closer result is found (reverses order of tiles to increase coverage)', assert => {
  const buffer = fs.readFileSync(__dirname + '/fixtures/canada-covered-square.mvt');
  const tiles = [
    {buffer: buffer, z: 11, x: 449, y: 694},
    {buffer: buffer, z: 11, x: 448, y: 694},
    {buffer: buffer, z: 11, x: 448, y: 693},
    {buffer: buffer, z: 11, x: 449, y: 693} // hit tile
  ];
  const opts = {
    radius: 10000 // about the width of a z15 tile
  }
  vtquery(tiles, [-100.9797421880223, 50.075683473759085], opts, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'only one feature');
    assert.equal(result.features[0].properties.tilequery.distance, 0, 'expected distance');
    assert.equal(result.features[0].properties.id, 'CA', 'expected id');
    assert.end();
  });
});

test('options - dedupe: returns duplicate results when dedupe is off', assert => {
  const buffer = fs.readFileSync(__dirname + '/fixtures/canada-covered-square.mvt');
  const tiles = [
    {buffer: buffer, z: 11, x: 449, y: 693},
    {buffer: buffer, z: 11, x: 449, y: 694}
  ];
  const opts = {
    radius: 10000, // about the width of a z15 tile
    dedupe: false
  }
  vtquery(tiles, [-100.9797421880223, 50.075683473759085], opts, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 2, 'two features');
    assert.equal(result.features[0].properties.tilequery.distance, 0, 'expected distance');
    assert.equal(result.features[0].properties.id, 'CA', 'expected id');
    assert.ok(result.features[1].properties.tilequery.distance > 0, 'expected distance greater than zero');
    assert.equal(result.features[1].properties.id, 'CA', 'expected id');
    assert.end();
  });
});

test('options - dedupe: compare fields for features that have no id (increases coverage)', assert => {
  const tiles = [
    {buffer: mvtf.get('002').buffer, z: 15, x: 5238, y: 12666},
    {buffer: mvtf.get('002').buffer, z: 15, x: 5237, y: 12666}
  ];
  const opts = {
    radius: 10000 // should encompass each point in each tile
  };
  vtquery(tiles, [-122.453, 37.767], opts, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'expected number of features');
    assert.end();
  });
});

test('options - dedupe: compare fields from real-world tiles (increases coverage)', assert => {
  const tiles = [
    {buffer: bufferSF, z: 15, x: 5238, y: 12666},
    {buffer: fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/sanfrancisco/15-5237-12666.mvt')), z: 15, x: 5237, y: 12666}
  ];
  const opts = {
    limit: 20,
    radius: 300 // should encompass each point in each tile
  };
  vtquery(tiles, [-122.453, 37.767], opts, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 20, 'expected number of features');
    assert.end();
  });
});

test('options - filter: Empty List', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', []]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 1, 'expected one feature');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Invalid Filter Condition', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['int_value', '==', 6]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'condition filter value must be =, !=, <, <=, >, or >=');
    assert.end();
  });
});

test('options - filter: Invalid Filter String', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['string_value', '=',"Strings_Not_Allowed"]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'value filter value must be a number or boolean');
    assert.end();
  });
});

test('options - filter: Invalid Filter Array', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', ['int_value', '=', 6]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'filters must be of the form [parameter, condition, value]');
    assert.end();
  });
});

test('options - filter: Test Int Equals', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['int_value', '=', 6]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.ifError(err);
    assert.equal(result.features.length, 1, 'expected one feature');
    const p = result.features[0].properties;
    assert.equal(p.int_value, 6, 'expected int value');
    assert.end();
  });
});

test('options - filter: Test Boolean Not Equals', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['bool_value', '!=', false]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 1, 'expected one feature');
    const p = result.features[0].properties;
    assert.equal(p.bool_value, true, 'expected value type');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test Float Approximate Equals', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['float_value', '=', 3.1]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 1, 'expected one feature');
    const p = result.features[0].properties;
    assert.equal(p.float_value, 3.0999999046325684, 'expected value type');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test No Results Less Than Equals', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['uint_value', '<', 0]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 0, 'expected one feature');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test No Results Greater Than Equals', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['sint_value', '>', 0]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 0, 'expected one feature');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test Double Int Greater Than Equal', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['double_value', '>',-2]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 1, 'expected one feature');
    const p = result.features[0].properties;
    assert.equal(p.double_value, 1.23, 'expected value type');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test filter with multiple features', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['population', '>', 10]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 3, 'expected one feature');
    let cities = [];
    for (let i = 0; i < 3; i++) {
      cities.push(result.features[i].properties.name);
    }
    assert.assert(cities.includes('RadEstablishment'), 'Missing RadEstablishment for population check');
    assert.assert(cities.includes('AwesomeCity'),  'Missing AwesomeCity for population check');
    assert.assert(cities.includes('TubularTown'), 'Missing TubularTown for population check');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test all filter with multiple features', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['population', '>', 10], ['population', '<', 1000]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 2, 'expected two features');
    let cities = [];
    for (let i = 0; i < 2; i++) {
      cities.push(result.features[i].properties.name);
    }
    assert.assert(cities.includes('RadEstablishment'), 'Missing RadEstablishment for population check');
    assert.assert(cities.includes('AwesomeCity'),  'Missing AwesomeCity for population check');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test any filter with multiple features', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['any', [['population', '<=', 10], ['population', '>', 1000]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 3, 'expected three features');
    let cities = [];
    for (let i = 0; i < 3; i++) {
      cities.push(result.features[i].properties.name);
    }
    assert.assert(cities.includes('Neatville'), 'Missing Neatville for population check');
    assert.assert(cities.includes('CoolVillage'),  'Missing CoolVillage for population check');
    assert.assert(cities.includes('TubularTown'),  'Missing TubularTown for population check');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test all with no filters', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', []]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 5, 'expected five features');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test any with no filters', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['any', []]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 5, 'expected five features');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test all with bad filters', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['all', [['junk', '=', 12]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 5, 'expected five features');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test with no filter', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 5, 'expected five features');
    assert.ifError(err);
    assert.end();
  });
});

test('options - filter: Test any with bad filters', assert => {
  const tiles = [{buffer: mvtf.get('062').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800, // about the width of a z15 tile
    'basic-filters': ['any', [['junk', '=', 12]]]
  };
  vtquery(tiles, [-122.3384, 47.6635], opts, function(err, result) {
    assert.equal(result.features.length, 0, 'expected zero features');
    assert.ifError(err);
    assert.end();
  });
});

test('success: returns all possible data value types', assert => {
  const tiles = [{buffer: mvtf.get('038').buffer, z: 15, x: 5248, y: 11436}];
  const opts = {
    radius: 800 // about the width of a z15 tile
  };
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

test('results with same exact distance return in expected order', assert => {
  // this query returns four results, three of which are the exact same distance and different types of features
  const buffer = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/chicago/13-2098-3045.mvt'));
  const ll = [-87.7964, 41.8675];
  vtquery([{buffer: buffer, z: 13, x: 2098, y: 3045}], ll, { radius: 10, layers: ['road'] }, function(err, result) {
    assert.equal(result.features[1].properties.type, 'turning_circle', 'is expected type');
    assert.ok(checkClose(result.features[1].properties.tilequery.distance, 9.436356889343624, 1e-6), 'is the proper distance');
    assert.equal(result.features[2].properties.type, 'service:driveway', 'is expected type');
    assert.ok(checkClose(result.features[2].properties.tilequery.distance, 9.436356889343624, 1e-6), 'is the proper distance');
    assert.equal(result.features[3].properties.type, 'pedestrian', 'is expected type');
    assert.ok(checkClose(result.features[3].properties.tilequery.distance, 9.436356889343624, 1e-6), 'is the proper distance');
    assert.end();
  });
});

test('success: handles multiple zoom levels', assert => {
  const buffer1 = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/chicago/13-2098-3045.mvt'));
  // spoofing a bangkok tile as somewhere over chicago
  const buffer2 = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/bangkok/12-3188-1888.mvt'));

  const tiles = [
    { buffer: buffer1, z: 13, x: 2098, y: 3045 },
    { buffer: buffer2, z: 12, x: 1049, y: 1522 }
  ];
  const opts = {
    radius: 100,
    layers: ['road_label']
  };
  vtquery(tiles, [-87.7718, 41.8464], opts, function(err, result) {
    assert.equal(result.features[0].properties.iso_3166_2, 'TH-73', 'expected road iso from bangkok');
    assert.equal(result.features[1].properties.iso_3166_2, 'US-IL', 'expected road iso from chicago');
    assert.equal(result.features[2].properties.iso_3166_2, 'US-IL', 'expected road iso from chicago');
    assert.end();
  });
});

test('success: handles multiple zoom levels (with compressed tiles)', assert => {
  const buffer1 = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/chicago/13-2098-3045.mvt'));
  // spoofing a bangkok tile as somewhere over chicago
  const buffer2 = fs.readFileSync(path.resolve(__dirname+'/../node_modules/@mapbox/mvt-fixtures/real-world/bangkok/12-3188-1888.mvt'));

  const tiles = [
    { buffer: zlib.gzipSync(buffer1), z: 13, x: 2098, y: 3045 },
    { buffer: zlib.gzipSync(buffer2), z: 12, x: 1049, y: 1522 }
  ];
  const opts = {
    radius: 100,
    layers: ['road_label']
  };
  vtquery(tiles, [-87.7718, 41.8464], opts, function(err, result) {
    assert.equal(result.features[0].properties.iso_3166_2, 'TH-73', 'expected road iso from bangkok');
    assert.equal(result.features[1].properties.iso_3166_2, 'US-IL', 'expected road iso from chicago');
    assert.equal(result.features[2].properties.iso_3166_2, 'US-IL', 'expected road iso from chicago');
    assert.end();
  });
});
