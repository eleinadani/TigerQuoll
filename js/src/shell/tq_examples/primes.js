
load('/home/bonettad/SM/mozilla-central/js/src/shell/tq.js')
//load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')


function isPrime(n) {
	for (var i = 2; i*i <= n; i++) {
		if (n % i == 0)
			return 0;
	}
	return 1;
};

// this object is shared between event handlers 
var counters = {
		processed : 0,
		primes : 0
}
//function sum(global, initial, final) { 
//	return global+(final-initial)
//}
//counters.markEventual('processed', sum)
//counters.markEventual('primes', sum)


function inc_primes(n) {
	counters.primes += n;
	return counters.primes;
}

function inc_processed(n) {
	counters.processed += n;
	return counters.processed;
}

// returns a closure
function searchPrimes(first, last) {

		var local_count = 0;
		for (var i = first; i < last; i++) {
			if (isPrime(i))
				local_count++;
		}

		var global_count = inc_primes(local_count);
		var iii = inc_processed(last - first) 

		if (iii >= LAST - FIRST) 
			print(' -> ' + global_count + " primes.");

};


var FIRST = 2;
var LAST =  10000000;
var BATCH = 100 


print('range: '+LAST+'/'+BATCH+' (= '+LAST/BATCH+')')


for (var i = FIRST; i < LAST; i += BATCH) {
	var first = i;
	var last = i + BATCH;
	if (last > LAST)
		last = LAST;
	
//	var closure = searchPrimes(first,last)
//	async(closure);
	async(searchPrimes, first, last);	
};


// http://primes.utm.edu/howmany.shtml
// pi(10000) = 1229
// pi(100000) = 9592
// pi(1000000) = 78498
// pi(10000000) = 664579
