load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var o = {}

o.on('A', function(a) {
	o.emit('B', a)
})
.on('B', function(a) {
	o.emit('C', a)
})
.on('C', function(a) {
	print(a)
})
.emit('A', 'hello world')
