//
// T11 - Tx logs
//
load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var N = 1000

var x = { tot : 0 }
var y = { tot : 0 }

var o = {}

o.waitAll('do', function() {
	print('all done '+x.tot+', '+y.tot)
})

o.on('do', function(foo) {

	foo.tot++
})

for(var it=0; it<N; it++)
		o.emit('do', x).emit('do',y)
