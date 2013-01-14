load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var o = { tot : 3, foo: { x:33 , bar:{ y:333 } } }

o.on('A', function(a) {
	print(o.tot)
	print(o.foo.x)
	print(o.foo.bar.y)
})

o.emit('A', 'hello world')
