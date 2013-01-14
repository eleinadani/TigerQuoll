
load('/home/bonettad/SM/mozilla-central/js/src/shell/tq.js')
//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')



var o = {}

var totals = new Map()


var file = new Array()

var BATCH		= 5;
var ITERS		= 1;

var LINES 		= 3000;
var WPERLINES 	= 1000;
var TOTWORDS   	= 500; //00000000;   // <- LC
var DIFFWORD	= 5;
var HITPROB		= 0.05///BATCH;


var _words = []

for(var w=0; w<TOTWORDS; w++) {
	var word = '_'+w
	_words[w] = word
}


function sum(global, initial, final) { 
	return global+(final-initial)
}
totals.markEventual('*', sum)


var wc = 0;

print('generating file...')	

var bword = 0
	
for(var lc=0; lc<LINES; lc++) {

	var line = ''

	var n = 0;
		
	for(var wl=0; wl<WPERLINES; wl++) {
	
		word = _words[ ((n++%DIFFWORD) + bword) % TOTWORDS ]
		line += ' '+word
	}
	
	bword+= DIFFWORD	
	
	var n = (Math.floor(Math.random() * 100) + 1)/100
	if(n <= HITPROB) {
		line += ' _hit'
	//	print('hit!')
	}
	
	file[lc] = line	
}



o.waitAll('foo', function() {
	print('all done')
	
	for(var p in totals) {
		if(typeof totals[p] == 'number')
			print("all --> valueof '"+p+"' : "+totals[p])
	}	
})

o.on('foo', function(from, to) {

	for(var line=from; line<to; line++) {
		var l = file[line]

		if(l == undefined) {
			print(from+' - '+to)
		}
		else {

		var s = l.split(' ')
		
		for(var i=0; i<s.length; i++) {
		
			var id = s[i]
		
			if(id != '') {
				if(totals[id] == undefined)
					totals[id] = 1
				else
					totals[id]++
			}
		}
		
		}
	}
})




for(var iter=0; iter<ITERS; iter++)
	for(var lc=0; lc<file.length; lc+=BATCH) 
		o.emit('foo', lc, lc+BATCH)


	
print('going parallel!')	