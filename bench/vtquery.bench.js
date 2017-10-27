"use strict";

const argv = require('minimist')(process.argv.slice(2));
if (!argv.iterations || !argv.concurrency) {
  console.error('Please provide desired iterations, concurrency');
  console.error('Example: \nnode bench/vtquery.bench.js --iterations 50 --concurrency 10');
  process.exit(1);
}

// This env var sets the libuv threadpool size.
// This value is locked in once a function interacts with the threadpool
// Therefore we need to set this value either in the shell or at the very
// top of a JS file (like we do here)
process.env.UV_THREADPOOL_SIZE = argv.concurrency;

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const d3_queue = require('d3-queue');
const mvtf = require('@mapbox/mvt-fixtures');
const vtquery = require('../lib/index.js');
const mapnik = require('mapnik');
const queue = d3_queue.queue();
let runs = 0;

// let tiles = getTiles('bangkok');
let tiles = [{buffer: fs.readFileSync('./mapbox-streets-v7-13-2098-3042.vector.pbf'), z: 13, x: 2098, y: 3042}];

function run(cb) {
  // let vt = new mapnik.VectorTile(13, 2098, 3042);
  //
  // vt.addData(tiles[0].buffer, function(err, vectorTile) {
  //   if (err) {
  //     return cb(err);
  //   }
  //   vt.query(-87.7799, 41.9513, {tolerance: 2000}, function(err, results) {
  //     if (err) {
  //       return cb(err);
  //     }
  //     ++runs;
  //     return cb();
  //   });
  // });

  vtquery(tiles, [-87.7799, 41.9513], {radius: 2000}, function(err, result) {
    if (err) {
      return cb(err);
    }
    ++runs;
    return cb();
  });
}

// Start monitoring time before async work begins within the defer iterator below.
// AsyncWorkers will kick off actual work before the defer iterator is finished,
// and we want to make sure we capture the time of the work of that initial cycle.
var time = +(new Date());

for (var i = 0; i < argv.iterations; i++) {
    queue.defer(run);
}

queue.awaitAll(function(error) {
  if (error) throw error;
  if (runs != argv.iterations) {
    throw new Error(`Error: did not run as expected - ${runs} != ${argv.iterations}`);
  }
  // check rate
  time = +(new Date()) - time;

  if (time == 0) {
    console.log("Warning: ms timer not high enough resolution to reliably track rate. Try more iterations");
  } else {
  // number of milliseconds per iteration
    var rate = runs/(time/1000);
    console.log('Benchmark speed: ' + rate.toFixed(0) + ' runs/s (runs:' + runs + ' ms:' + time + ' )');
  }

  console.log("Benchmark iterations:",argv.iterations,"concurrency:",argv.concurrency)

  // There may be instances when you want to assert some performance metric
  //assert.equal(rate > 1000, true, 'speed not at least 1000/second ( rate was ' + rate + ' runs/s )');

});

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
