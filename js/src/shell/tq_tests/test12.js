//
// T12 - Spawn with result (paper version)
//
load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')


var _spawn = function(f, a, b) {

    var _A = {}

    _A.on('go!', function() {
        // Call the function
        _A.result = f(a,b)
        // Create the future callback
    })
    .on('get!', function(cb) {
        cb(_A.result)
    })    
    .emit('go!')  
    
    return {                
        get: function(callback) {
        	print('get '+_A.result)
            if (_A.result) {
            	callback(_A.result)
            } else {
            	_A.emit('get!', callback)
            }
        }
    }
}

function f(a,b) { return a*b; }
function cb() {}

var fut = _spawn(f, 2, 3)

var e = {}

fut.get(cb)

e.on('go', function() {
	fut.get(cb)
})
.emit('go')