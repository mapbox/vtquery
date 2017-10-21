var express = require('express');
var ejs = require('ejs');
var bodyParser = require('body-parser');
var request = require('request');
var zlib = require('zlib');

var tilebelt = require('@mapbox/tilebelt');
var vtquery = require('../lib/index.js');

var app = express();
module.exports.app = app;
app.set('port', (process.env.PORT || 5000));
app.engine('html', require('ejs').renderFile);
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

app.get('/', function(req, res) {
  return res.render('./index.html');
});

app.get('/api', function(req, res) {
  // get tile to query
  console.log('zoom: ', req.query.zoom);
  var z = 15;

  var tile = tilebelt.pointToTile(req.query.lng, req.query.lat, z);
  console.log('sm tile: ', tile);

  var tileset = 'mapbox.mapbox-streets-v7';
  var x = tile[0];
  var y = tile[1];
  var url = `https://api.mapbox.com/v4/${tileset}/${z}/${x}/${y}.vector.pbf?access_token=${process.env.MapboxAccessToken}`;
  request.get(url, {encoding: null}, function(err, req_res, body) {
    if (err) throw err;
    if (res.statusCode !== 200) throw new Error(`status code error ${res.statusCode}`);


    zlib.gunzip(body, function(err, deflated) {
      if (err) throw err;

      vtquery([{buffer: deflated, z: parseInt(z), x: parseInt(x), y: parseInt(y)}], [parseFloat(req.query.lng), parseFloat(req.query.lat)], {radius: 100}, function(err, results) {
        if (err) throw err;

        return res.json(JSON.parse(results));
      });
    });
  });
});

app.listen(app.get('port'), function() {
  console.log('Node app is running on port', app.get('port'));
});
