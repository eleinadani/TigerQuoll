load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var o = {}

var file = new Array()

var LINES = 1000;
var WPERLINES = 10;
var WORDS = 1000;


var wc = 0;
	
for(var lc=0; lc<LINES; lc++) {

	var l = ''
		
	for(var wl=0; wl<WPERLINES; wl++) {

		var word = ' xxx'+((wc++)%WORDS)		
		l += word		
	}
	file[lc] = l
}

var totals = new Map()

file.par(function(w) {

	var s = w.split(' ')
	
	for(var i=0; i<s.length; i++) {
		
		var id = s[i]
		
		if(id != '') {
			if(totals[id] == undefined)
				totals[id] = 1
			else
				totals[id]++
		}
	}
})
.finish(function() {
	print('done :)')
})