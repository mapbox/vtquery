var test = require('tape');
var vtquery = require('../lib/index.js');

test('success: prints regular busy world', function(t) {
  vtquery({}, function(err, result) {
    if (err) throw err;
    t.equal(result, 'hello');
    t.end();
  });
});

test('success: prints regular busy world', function(t) {
  vtquery({ louder: true }, function(err, result) {
    if (err) throw err;
    t.equal(result, 'hello!!!!');
    t.end();
  });
});
