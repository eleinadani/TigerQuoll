load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
var o = {}

o.on('foo', function() {
	print('hello world')	
}).emit('foo')
