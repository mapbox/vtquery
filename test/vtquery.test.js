const test = require('tape');
const vtquery = require('../lib/index.js');
// const mvtFixtures = require('@mapbox/mvt-fixtures');
const fs = require('fs');

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
    results: 'hi'
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'results\' must be a number');
    assert.end();
  });
});

test('failure: options.results is negative', assert => {
  const opts = {
    results: -10
  };
  vtquery([{buffer: new Buffer('hey'), z: 0, x: 0, y: 0}], [47.6, -122.3], opts, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, '\'results\' must be a positive number');
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

test('success: no options - all defaults', assert => {
  const buffer = fs.readFileSync(__dirname + '/../node_modules/@mapbox/mvt-fixtures/fixtures/043/tile.mvt');
  vtquery([{buffer: buffer, z: 0, x: 0, y: 0}], [47.6, -122.3], function(err, result) {
    assert.ifError(err);
    console.log(result);
    assert.end();
  });
});
