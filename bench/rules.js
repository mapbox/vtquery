'use strict';

const fs = require('fs');
const path = require('path');
const mvtf = require('@mapbox/mvt-fixtures');

module.exports = [
  {
    description: '9 tiles, chicago',
    queryPoint: [-87.7164, 41.8705],
    options: { radius: 1000 },
    tiles: [
      { z: 13, x: 2098, y: 3043, buffer: getTile('chicago', '13-2098-3043.mvt') },
      { z: 13, x: 2099, y: 3043, buffer: getTile('chicago', '13-2099-3043.mvt') },
      { z: 13, x: 2100, y: 3043, buffer: getTile('chicago', '13-2100-3043.mvt') },
      { z: 13, x: 2098, y: 3044, buffer: getTile('chicago', '13-2098-3044.mvt') },
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') },
      { z: 13, x: 2100, y: 3044, buffer: getTile('chicago', '13-2100-3044.mvt') },
      { z: 13, x: 2098, y: 3045, buffer: getTile('chicago', '13-2098-3045.mvt') },
      { z: 13, x: 2099, y: 3045, buffer: getTile('chicago', '13-2099-3045.mvt') },
      { z: 13, x: 2100, y: 3045, buffer: getTile('chicago', '13-2100-3045.mvt') }
    ]
  },
  {
    description: '9 tiles, chicago, super large radius',
    queryPoint: [-87.7164, 41.8705],
    options: { radius: 10000 },
    tiles: [
      { z: 13, x: 2098, y: 3043, buffer: getTile('chicago', '13-2098-3043.mvt') },
      { z: 13, x: 2099, y: 3043, buffer: getTile('chicago', '13-2099-3043.mvt') },
      { z: 13, x: 2100, y: 3043, buffer: getTile('chicago', '13-2100-3043.mvt') },
      { z: 13, x: 2098, y: 3044, buffer: getTile('chicago', '13-2098-3044.mvt') },
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') },
      { z: 13, x: 2100, y: 3044, buffer: getTile('chicago', '13-2100-3044.mvt') },
      { z: 13, x: 2098, y: 3045, buffer: getTile('chicago', '13-2098-3045.mvt') },
      { z: 13, x: 2099, y: 3045, buffer: getTile('chicago', '13-2099-3045.mvt') },
      { z: 13, x: 2100, y: 3045, buffer: getTile('chicago', '13-2100-3045.mvt') }
    ]
  },
  {
    description: '9 tiles, chicago - only points',
    queryPoint: [-87.7164, 41.8705],
    options: { radius: 1000, geometry: 'point' },
    tiles: [
      { z: 13, x: 2098, y: 3043, buffer: getTile('chicago', '13-2098-3043.mvt') },
      { z: 13, x: 2099, y: 3043, buffer: getTile('chicago', '13-2099-3043.mvt') },
      { z: 13, x: 2100, y: 3043, buffer: getTile('chicago', '13-2100-3043.mvt') },
      { z: 13, x: 2098, y: 3044, buffer: getTile('chicago', '13-2098-3044.mvt') },
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') },
      { z: 13, x: 2100, y: 3044, buffer: getTile('chicago', '13-2100-3044.mvt') },
      { z: 13, x: 2098, y: 3045, buffer: getTile('chicago', '13-2098-3045.mvt') },
      { z: 13, x: 2099, y: 3045, buffer: getTile('chicago', '13-2099-3045.mvt') },
      { z: 13, x: 2100, y: 3045, buffer: getTile('chicago', '13-2100-3045.mvt') }
    ]
  },
  {
    description: 'single mapbox streets tile in chicago',
    queryPoint: [-87.7371, 41.8838],
    options: { radius: 0 },
    tiles: [
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
    ]
  },
  {
    description: 'single mapbox streets tile in chicago',
    queryPoint: [-87.7371, 41.8838],
    options: { radius: 2000 },
    tiles: [
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
    ]
  },
  {
    description: 'single mapbox streets tile in chicago',
    queryPoint: [-87.7371, 41.8838],
    options: { radius: 2000, geometry: 'point' },
    tiles: [
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
    ]
  },
  {
    description: 'single mapbox streets tile in chicago',
    queryPoint: [-87.7371, 41.8838],
    options: { radius: 2000, geometry: 'linestring' },
    tiles: [
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
    ]
  },
  {
    description: 'single mapbox streets tile in chicago',
    queryPoint: [-87.7371, 41.8838],
    options: { radius: 2000, geometry: 'polygon' },
    tiles: [
      { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
    ]
  }
];

function getTile(name, file) {
  return fs.readFileSync(path.join(__dirname, '..', 'node_modules', '@mapbox', 'mvt-fixtures', 'real-world', name, file))
}

// get all tiles
function getTiles(name) {
  let tiles = [];
  let dir = `./node_modules/@mapbox/mvt-fixtures/real-world/${name}`;
  var files = fs.readdirSync(dir);
  files.forEach(function(file) {
    let buffer = fs.readFileSync(path.join(dir, '/', file));
    file = file.replace('.mvt', '');
    let zxy = file.split('-');
    tiles.push({ buffer: buffer, z: parseInt(zxy[0]), x: parseInt(zxy[1]), y: parseInt(zxy[2]) });
  });
  return tiles;
}
