'use strict';

const fs = require('fs');
const path = require('path');
const mvtf = require('@mapbox/mvt-fixtures');

module.exports = [

  // point in polygon "within/intersects"
  {
    description: 'pip: many building polygons',
    queryPoint: [120.9667, 14.6028],
    options: { radius: 0 },
    tiles: [
      { z: 16, x: 54789, y: 30080, buffer: fs.readFileSync('./test/fixtures/manila-buildings-16-54789-30080.mvt')}
    ]
  },
  {
    description: 'pip: many building polygons, single layer',
    queryPoint: [120.9667, 14.6028],
    options: { radius: 0, layers: ['building'] },
    tiles: [
      { z: 16, x: 54789, y: 30080, buffer: fs.readFileSync('./test/fixtures/manila-buildings-16-54789-30080.mvt')}
    ]
  },

  // queries "closest point"
  {
    description: 'query: many building polygons, single layer',
    queryPoint: [120.9667, 14.6028],
    options: { radius: 600, geometry: 'polygon', layers: ['building'] },
    tiles: [
      { z: 16, x: 54789, y: 30080, buffer: fs.readFileSync('./test/fixtures/manila-buildings-16-54789-30080.mvt')}
    ]
  },
  {
    description: 'query: linestrings, mapbox streets roads',
    queryPoint: [120.991, 14.6147],
    options: { radius: 3000, geometry: 'linestring' },
    tiles: [
      { z: 14, x: 13698, y: 7519, buffer: fs.readFileSync('./test/fixtures/manila-roads-terrain-14-13698-7519.mvt')}
    ]
  },
  {
    description: 'query: polygons, mapbox streets buildings',
    queryPoint: [120.9667, 14.6028],
    options: { radius: 600, geometry: 'polygon' },
    tiles: [
      { z: 16, x: 54789, y: 30080, buffer: fs.readFileSync('./test/fixtures/manila-buildings-16-54789-30080.mvt')}
    ]
  },
  {
    description: 'query: all things - dense single tile',
    queryPoint: [-122.4372, 37.7663],
    options: { radius: 1000 },
    tiles: [
      { z: 15, x: 5239, y: 12666, buffer: fs.readFileSync('./test/fixtures/sf-15-5239-12666.mvt')}
    ]
  },
  {
    description: 'query: all things - dense nine tiles',
    queryPoint: [-122.4371, 37.7663],
    options: { radius: 2000 },
    tiles: [
      { z: 15, x: 5238, y: 12665, buffer: fs.readFileSync('./test/fixtures/sf-15-5238-12665.mvt')},
      { z: 15, x: 5239, y: 12665, buffer: fs.readFileSync('./test/fixtures/sf-15-5238-12666.mvt')},
      { z: 15, x: 5240, y: 12665, buffer: fs.readFileSync('./test/fixtures/sf-15-5238-12667.mvt')},
      { z: 15, x: 5238, y: 12666, buffer: fs.readFileSync('./test/fixtures/sf-15-5239-12665.mvt')},
      { z: 15, x: 5239, y: 12666, buffer: fs.readFileSync('./test/fixtures/sf-15-5239-12666.mvt')},
      { z: 15, x: 5240, y: 12666, buffer: fs.readFileSync('./test/fixtures/sf-15-5239-12667.mvt')},
      { z: 15, x: 5238, y: 12667, buffer: fs.readFileSync('./test/fixtures/sf-15-5240-12665.mvt')},
      { z: 15, x: 5239, y: 12667, buffer: fs.readFileSync('./test/fixtures/sf-15-5240-12666.mvt')},
      { z: 15, x: 5240, y: 12667, buffer: fs.readFileSync('./test/fixtures/sf-15-5240-12667.mvt')}
    ]
  },

  // elevation
  {
    description: 'elevation: terrain tile nepal',
    queryPoint: [85.2765, 28.0537],
    options: {},
    tiles: [
      { z: 13, x: 6036, y: 3430, buffer: getTile('nepal', '13-6036-3430.mvt')}
    ]
  },

  // geometry
  {
    description: 'geometry: 2000 points in a single tile',
    queryPoint: [-122.3302, 47.6639],
    options: { radius: 500, geometry: 'point' },
    tiles: [
      { z: 16, x: 10498, y: 22872, buffer: fs.readFileSync('./test/fixtures/points-16-10498-22872.mvt')}
    ]
  },
  {
    description: 'geometry: 2000 linestrings in a single tile',
    queryPoint: [-122.3302, 47.6639],
    options: { radius: 500, geometry: 'linestring' },
    tiles: [
      { z: 16, x: 10498, y: 22872, buffer: fs.readFileSync('./test/fixtures/linestrings-16-10498-22872.mvt')}
    ]
  },
  {
    description: 'geometry: 2000 polygons in a single tile',
    queryPoint: [-122.3302, 47.6639],
    options: { radius: 500, geometry: 'polygon' },
    tiles: [
      { z: 16, x: 10498, y: 22872, buffer: fs.readFileSync('./test/fixtures/polygons-16-10498-22872.mvt')}
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
