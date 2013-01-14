load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var o = {}

o.on('foo', function(a, b, c, d) {
	print(typeof a)
	print(typeof b)
	print(typeof c)		
	print(typeof d)	
})

o.emit('foo', 33, 'xxx xxx xxx', {a:0}, true)	
