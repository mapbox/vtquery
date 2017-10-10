const test = require('tape');
const vtquery = require('../lib/index.js');

test('failure: fails without callback function', assert => {
  try {
    vtquery();
  } catch(err) {
    assert.ok(/last argument must be a callback function/.test(err.message), 'expected error message');
    assert.end();
  }
});

test('failure: buffers is not an object', assert => {
  vtquery('i am a buffer object', [47.6117, -122.3444], {}, function(err, result) {
    assert.ok(err);
    assert.equal(err.message, 'first arg \'buffers\' must be an array of objects');
    assert.end();
  });
});

// more buffer failure tests

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
