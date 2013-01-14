
load('/Users/usiusi/Dev/__github/TigerQuoll/js/src/shell/tq.js')

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




































/*

load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')


var p = {
	
	_ : {},
	x : 0,
	res : [],
	
	spawn : function(f, n) {
		
		p._.on('go', function(t, f, a) {
			
			p.res[t] = f(a)
		})
		.emit('go', p.x, f, n)
		
		p.x++;
		return this;		
	},

	ondone : function(cb) {
		p._.waitAll('go', function() {
			cb(p.res)
		})
		return this;
	}
}



var f = function(n) { 
	return n
}


p
.ondone(function(r) print('done '+r[0]+' '+r[1]))
.spawn(f, 33)
.spawn(f, 3)

*/















//
//
//var fibo = function(n) {
//	
//	if(n <= 1) {
//		p.result(1)
//	}
//	else {
//		p.spawn(fibo, n-1)
//		.spawn(fibo, n-2)
//		.ondone(function(res1, res2) {
//			p.result(res1+res2)
//		})
//	}
//}
//
//
//
//p.spawn(fibo, 3)
//.ondone(function(res) print('res '+res))








































//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
//
//var o = {}
////var foo = { fld : 3 }
//
//o.on('tic', function() {
//	
////	var foo = { fld : 3, aaa : 'dd' }
//	var foo = { xxx:3 }
//	
//	o.on('tac', function() {
//		
//		print(' -----------------------> '+foo.xxx)
////		var x=[a,b,c,d]
//		
//	}).emit('tac')
//	
//}).emit('tic')
//
//
//
//
//







































//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
//
//var o = {}
//
////var e = { e : 3 }
//
//o.on('tic', function() {
//	
//	var e = { e : 3 }
//	
//	o.on('tac', function() {
//		o.on('toe', function() {
//		
//
//			print('toe! '+e.e)	
//		}).emit('toe')
//		
//		
//		print('tac! '+e.e)	
//	}).emit('tac')
//	
//	
//	print('tic! '+e.e)
//}).emit('tic')




































//
////load('/home/bonettad/SM/mozilla-central/js/src/shell/tq.js')
//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
//
//
//
//var o = {}
//
//var totals = new Map()
//
//
//var file = new Array()
//
//var BATCH		= 5;
//var ITERS		= 1;
//
//var LINES 		= 2000;
//var WPERLINES 	= 1000;
//var TOTWORDS   	= 500;
//var DIFFWORD	= 5;
//var HITPROB		= 0.03
//
//
//var _words = []
//
//for(var w=0; w<TOTWORDS; w++) {
//	var word = '_'+w
//	_words[w] = word
//}
//
//
//function sum(global, initial, final) { 
//	return global+(final-initial)
//}
////totals.markEventual('*', sum)
//
//
//var wc = 0;
//
//print('generating file...')	
//
//var bword = 0
//	
//for(var lc=0; lc<LINES; lc++) {
//
//	var line = ''
//
//	var n = 0;
//		
//	for(var wl=0; wl<WPERLINES; wl++) {
//	
//		word = _words[ ((n++%DIFFWORD) + bword) % TOTWORDS ]
//		line += ' '+word
//	}
//	
//	bword+= DIFFWORD	
//	
//	var n = (Math.floor(Math.random() * 100) + 1)/100
//	if(n <= HITPROB) {
//		line += ' _hit'
//	}
//	
//	file[lc] = line	
//}
//
//
//
//o.waitAll('foo', function() {
//	print('all done')
//	
////	for(var p in totals) {
////		if(typeof totals[p] == 'number')
////			print("all --> valueof '"+p+"' : "+totals[p])
////	}	
//})
//
//o.on('foo', function(from, to) {
//
//	for(var line=from; line<to; line++) {
//		var l = file[line]
//
//		if(l == undefined) {
//			print(from+' - '+to)
//		}
//		else {
//
//		var s = l.split(' ')
//		
//		for(var i=0; i<s.length; i++) {
//		
//			var id = s[i]
//		
//			if(id != '') {
//				if(totals[id] == undefined)
//					totals[id] = 1
//				else
//					totals[id]++
//			}
//		}
//		
//		}
//	}
//})
//
//
//
//
//for(var iter=0; iter<ITERS; iter++)
//	for(var lc=0; lc<file.length; lc+=BATCH) 
//		o.emit('foo', lc, lc+BATCH)
//
//
//	
//print('going parallel!')	
//
//
//






//
//
//
//
////load('/home/bonettad/SM/mozilla-central/js/src/shell/tq.js')
//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
//
//
//
//var o = {}
//
//var totals = new Map()
//
//
//var file = new Array()
//
//var LINES 		= 10;
//var WPERLINES 	= 3000;
//var TOTWORDS   	= 5000000000;   // <- LC
////var WLEN		= 1;
//
//var ALPHABET 	= ['a','b','c','d','e','f','g','h','i','l','m','n','o','p','q','r','s','t','u','v','z']
//var LETTERS		= ALPHABET.length
//
////var WORDS = 500000000;   // <- MC
////var WORDS = 50000000;   // <- HC
//
//
//
//
//
//
//function sum(global, initial, final) {
//	return global+(final-initial)
//}
//totals.markEventual('*', sum)
//
//
//var wc = 0;
//
//print('generating file...')	
//	
//for(var lc=0; lc<LINES; lc++) {
//
//	var line = ''
//		
//	for(var wl=0; wl<WPERLINES; wl++) {
//	
//		var baseWord = 'xxx'
////		for(var wc=0; wc<WLEN; wc++) {
////			var letter = Math.floor((Math.random()*LETTERS)+1);
////			baseWord += ALPHABET[letter-1]
////		}
//		
//		var num = Math.floor((Math.random()*TOTWORDS)+1);		
//		var word = ' ' + baseWord + num;
//
//		line += word
//	}
//	file[lc] = line
//}
//
//
//o.waitAll('foo', function() {
//	print('all done')
//	
////	for(var p in totals) {
////		if(typeof totals[p] == 'number')
////			print("all --> valueof '"+p+"' : "+totals[p])
////	}	
//})
//
//o.on('foo', function(w) {
//	
//	var o = JSON.parse(w)
//	var s = o.body.split(' ')
//	var l = o.body.split('')
//	
////	var xx = w.split('')
////	var s = w.match(/\S+/g)	
//	
//	for(var i=0; i<s.length; i++) {
//		
//		var id = s[i]
//		
//		if(id != '') {
//			if(totals[id] == undefined)
//				totals[id] = 1
//			else
//				totals[id]++
//		}
//	}	
//})
//
//for(var lc=0; lc<file.length; lc++) 
//	o.emit('foo', JSON.stringify({ body : file[lc]}))
//	
//print('going parallel!\n\n\n')


//for(var l=0; l<file.length; l++) { 
//	print(file[l])
//	
//	var s = file[l]
//	var a = s.match(/\S+/g)
//	
//	print(a)
//}

















































//
////load('/home/bonettad/SM/mozilla-central/js/src/shell/tq.js')
//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
//
//
//var o = {}
//
//var N = 100000
//
//var x = {tot : 0}
//var y = {tot : 0}
//
//
//o.waitAll('go', function() {
//	print('all done: '+x.tot+', '+y.tot)
//})
//.on('go', function(obj) {
//	obj.tot++
//})
//
//
//for(var i=0; i<N; i++)
//	o.emit('go', x)
//	






//
//
//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')
//
//var o = {}
//
//var finish = function(f) {
//	
//	o.on('go', function(cb) {
//		cb()
//	})
//	
//	f()
////	var self = finish
//	return {
//		ondone : function(cb) {
//			o.waitAll('go', cb)
//		}
//	}
//}
//
//
//var asy = function(cb) {
//	
//	o.emit('go', cb)
//}
//
//
//var x = {x:0}
//
//finish(function() {
//	
//	asy(function() {
//		x.x++
//		print('a\n')
//	})
//
//	asy(function() {
//		x.x++
//		print('b\n')
//	})	
//	
//}).ondone(function() {
//	print('done: '+x.x)
//})
//
