load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
var o = {}

o.on('foo', function(a) {
	print(a)	
}).emit('foo', 'hello world')
