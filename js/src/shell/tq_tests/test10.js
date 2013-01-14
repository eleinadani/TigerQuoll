//
// T09 - Write (and commit) multiple fields from (empty) global obj 
//
load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var o = {}

var foo = new Map()

var FIELDS = 1000;
var ITERS = 10


o.waitAll('do', function() {
	print('all done')
})

o.on('do', function(x) {

	foo[x] = 33
})

for(var it=0; it<ITERS; it++) {
	for(var i=0; i<FIELDS; i++)
		o.emit('do', '->'+i)
}
