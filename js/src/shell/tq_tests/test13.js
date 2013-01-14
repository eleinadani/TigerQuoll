//
// T13 - Async/Finish (paper version)
//
load('/Users/usiusi/Dev/SM/mozilla-central/js/src/shell/tq.js')

var _async = function(fun, a, b) {
    // schedule 'fun' for asynchronous execution
    // using the 'finish' global object
    _finish.emit('go', fun, a, b)
}
var _finish = function(f) {
    // register a callback for executing functions
    // in parallel using the 'finish' (this) object
    _finish.on('go', function(fun, a, b) {
        fun(a, b)
    })
    // execute the function, which will call 
    // async multiple times
    f()

    return {
        ondone : function(callback) {
            // register a wait guard for the
            // 'go' event
            _finish.waitAll('go', function() {
                callback()
            })
        }
    }
}

    
_finish(function() {

    for(var i=0; i<3; i++)
        _async(function() {
            print('foo')
        })
        
}).ondone(function() {
    print('bar')
})

