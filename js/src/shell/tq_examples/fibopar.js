

load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')


var fiboS = function(n) {
	if(n>1) return n;
	else return fiboS(n-1)+fiboS(n-2);
}


var fibo = function(n) {
	
	let XXX = 20
	var obj = {}
	
	
	obj.on('fib', function(n) {
		if(n<XXX) {
			obj[n] = fiboS(n)
		}
		else {
			obj.emit('fib', n-1)
			obj.emit('fib', n-2)
		}
	})
	
	.waitAll('fib', function() {
		let r = 0
		for (var x in obj) {
			r+= obj[x]
		}
		obj.emit('result', r)
	})

	if(n>20)
		obj
		.emit('fib', n-1)
		.emit('fib', n-2)	
	else
		obj.emit('result', fiboS(n))
	
	return {
		finish : function(fincb) {
			obj.on('result', function(r) {
				fincb(r)
			})
		}
	}
}





fibo(25)
.finish(function(res) print(res))



