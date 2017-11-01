'use strict';

const fs = require('fs');
const path = require('path');
const mvtf = require('@mapbox/mvt-fixtures');

module.exports = [
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
  {
    description: 'query: many building polygons, single layer',
    queryPoint: [120.9667, 14.6028],
    options: { radius: 600, geometry: 'polygon', layers: ['building'] },
    tiles: [
      { z: 16, x: 54789, y: 30080, buffer: fs.readFileSync('./test/fixtures/manila-buildings-16-54789-30080.mvt')}
    ]
  },
  // {
  //   description: 'query: points, TODO',
  //   queryPoint: [120.991, 14.6147],
  //   options: { radius: 3000, geometry: 'linestring' },
  //   tiles: [
  //     { z: 14, x: 13698, y: 7519, buffer: fs.readFileSync('./test/fixtures/manila-roads-terrain-14-13698-7519.mvt')}
  //   ]
  // },
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
  {
    description: 'elevation: terrain tile nepal',
    queryPoint: [85.2765, 28.0537],
    options: {},
    tiles: [
      { z: 13, x: 6036, y: 3430, buffer: getTile('nepal', '13-6036-3430.mvt')}
    ]
  }
  // {
  //   description: '9 tiles, nepal terrain, radius 1000',
  //   queryPoint: [-87.7164, 41.8705],
  //   options: { radius: 1000 },
  //   tiles: [
  //     { z: 13, x: 2098, y: 3043, buffer: getTile('chicago', '13-2098-3043.mvt') },
  //     { z: 13, x: 2099, y: 3043, buffer: getTile('chicago', '13-2099-3043.mvt') },
  //     { z: 13, x: 2100, y: 3043, buffer: getTile('chicago', '13-2100-3043.mvt') },
  //     { z: 13, x: 2098, y: 3044, buffer: getTile('chicago', '13-2098-3044.mvt') },
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') },
  //     { z: 13, x: 2100, y: 3044, buffer: getTile('chicago', '13-2100-3044.mvt') },
  //     { z: 13, x: 2098, y: 3045, buffer: getTile('chicago', '13-2098-3045.mvt') },
  //     { z: 13, x: 2099, y: 3045, buffer: getTile('chicago', '13-2099-3045.mvt') },
  //     { z: 13, x: 2100, y: 3045, buffer: getTile('chicago', '13-2100-3045.mvt') }
  //   ]
  // },
  // {
  //   description: '9 tiles, chicago, only points',
  //   queryPoint: [-87.7164, 41.8705],
  //   options: { radius: 1000, geometry: 'point' },
  //   tiles: [
  //     { z: 13, x: 2098, y: 3043, buffer: getTile('chicago', '13-2098-3043.mvt') },
  //     { z: 13, x: 2099, y: 3043, buffer: getTile('chicago', '13-2099-3043.mvt') },
  //     { z: 13, x: 2100, y: 3043, buffer: getTile('chicago', '13-2100-3043.mvt') },
  //     { z: 13, x: 2098, y: 3044, buffer: getTile('chicago', '13-2098-3044.mvt') },
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') },
  //     { z: 13, x: 2100, y: 3044, buffer: getTile('chicago', '13-2100-3044.mvt') },
  //     { z: 13, x: 2098, y: 3045, buffer: getTile('chicago', '13-2098-3045.mvt') },
  //     { z: 13, x: 2099, y: 3045, buffer: getTile('chicago', '13-2099-3045.mvt') },
  //     { z: 13, x: 2100, y: 3045, buffer: getTile('chicago', '13-2100-3045.mvt') }
  //   ]
  // },
  // {
  //   description: 'mbx streets no radius',
  //   queryPoint: [-87.7371, 41.8838],
  //   options: { radius: 0 },
  //   tiles: [
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
  //   ]
  // },
  // {
  //   description: 'mbx streets 2000 radius',
  //   queryPoint: [-87.7371, 41.8838],
  //   options: { radius: 2000 },
  //   tiles: [
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
  //   ]
  // },
  // {
  //   description: 'mbx streets only points',
  //   queryPoint: [-87.7371, 41.8838],
  //   options: { radius: 2000, geometry: 'point' },
  //   tiles: [
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
  //   ]
  // },
  // {
  //   description: 'mbx streets only linestrings',
  //   queryPoint: [-87.7371, 41.8838],
  //   options: { radius: 2000, geometry: 'linestring' },
  //   tiles: [
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
  //   ]
  // },
  // {
  //   description: 'mbx streets only polys',
  //   queryPoint: [-87.7371, 41.8838],
  //   options: { radius: 2000, geometry: 'polygon' },
  //   tiles: [
  //     { z: 13, x: 2099, y: 3044, buffer: getTile('chicago', '13-2099-3044.mvt') }
  //   ]
  // },
  // {
  //   description: 'complex multipolygon',
  //   queryPoint: [10.6759453345, 64.8680179376],
  //   options: { radius: 4000, geometry: 'polygon' },
  //   tiles: [
  //     { z: 12, x: 2169, y: 1069, buffer: getTile('norway', '12-2169-1069.mvt') }
  //   ]
  // }
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
