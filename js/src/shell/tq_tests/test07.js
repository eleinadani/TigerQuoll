load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var o = {tot : 0}

o.waitAll('foo', function() print(o.tot))

o.on('foo', function() {
	o.tot++
})

for(var i=0; i<10000; i++)
	o.emit('foo', i)
