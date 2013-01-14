// ------------------------------------------------------------------------------------ //
// ------------------------------------------------------------------------------------ //
Object.prototype.on = function(ev, cb) {
	if( ! this._cbs) {
		this._cbs = []
	}
	this._cbs[ev] = cb
	return this
}
Object.prototype.emit = function(ev, arg) {

	if( ! this._cbs) {
		this._cbs = []
	}
	else {	
 		if(this._cbs[ev]) { 
			var args = []
			args.push(this._cbs[ev])
			args.push(this)
			args.push(ev)
			for(var i=1; i<arguments.length; i++)
				args.push(arguments[i])

		    __rt_tx_emit_async.apply(this, args)
 		}
	}
	return this
}
Object.prototype.waitAll = function(ev, cb) {
	__rt_register_wait_guard(cb, this, ev)
	return this
}
Object.prototype.markEventual = function(field, cb) {
	__rt_mark_eventual(cb, this, field, (field === '*'))
	return this
}
//------------------------------------------------------------------------------------ //
//------------------------------------------------------------------------------------ //
var console = {
		log : function(txt) {
			var obj = {}
			obj.on('print', function(){
				print(txt)
			}).emit('print')
		}
}
//------------------------------------------------------------------------------------ //
//------------------------------------------------------------------------------------ //
var par = function(cb) {
	
	var obj = {}
	obj.on('startPar', function() {
	 	cb()
	}).emit('startPar')
	
	return {
		finish : function(fincb) {
			obj.waitAll('startPar', fincb)
	}}
}
//------------------------------------------------------------------------------------ //
//------------------------------------------------------------------------------------ //
var spawn = function(cb) {
	
	var obj = {}
	obj.on('startAsyncSimple', function() {
	 	obj.r = cb()
	 	obj.emit('done', obj.r)
	})
	.emit('startAsyncSimple')
	
	return {
		get : function(cb) {
			obj.on('done', function(r) {
				cb(r)
			})
		}
	}
}
//------------------------------------------------------------------------------------ //
//------------------------------------------------------------------------------------ //
var async = function(cb, a1, a2) {
	
	var obj = {}
	obj.on('startAsync', function(fun, a, b) {
		fun(a,b)
	})
	.emit('startAsync', cb, a1, a2)
}
//------------------------------------------------------------------------------------ //
//------------------------------------------------------------------------------------ //
Array.prototype.par = function(cb) {
	var obj = {}	
	obj.waitAll('go', function() obj.emit('donePar'))

	.on('go', function(x) {
		cb(x)
	})
	
	for(var v=0; v<this.length; v++) {
		obj.emit('go', this[v])
	}
	
	return {
		finish : function(fincb) {
			obj.on('donePar', fincb)
	}}
}
//------------------------------------------------------------------------------------ //
//------------------------------------------------------------------------------------ //
