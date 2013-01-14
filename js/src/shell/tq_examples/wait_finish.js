

load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var t = {tot:0}

do(function(){

	async(function() {
		t.tot++
	})

	async(function() {
		t.tot++
	})
	
}).finish(function(x) print(t.tot)))








