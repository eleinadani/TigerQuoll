/*
 * tqevent.cpp
 *
 *  Created on: Dec 7, 2012
 *      Author: usiusi
 */



#include "tqevent.h"

#include <jsapi.h>

#include <sys/time.h>

#include "tqtxmembrane.h"
#include "tqtxwrapper.h"
#include "tqtxutils.h"
#include "pjsutil.h"
#include "tqmainloop.h"


using namespace tq::tx;
using namespace pjs;
using namespace std;

namespace tq {



bool
EventCallback::addRoots() {
	for (; _rooted <= _argc+2; _rooted++) {
		if (!JS_AddNamedValueRoot(_original_cx, &_toProxy[_rooted],
				"Closure::toProxyArgv[]"))
			return false;
	}
	return true;
}

void
EventCallback::delRoots() {
	if(_has_roots) {
		JSAutoRequest r(_original_cx);
		for (unsigned i = 0; i < _rooted; i++)
			JS_RemoveValueRoot(_original_cx, &_toProxy[i]);
	}
}


EventCallback*
EventCallback::create(JSContext* cx, jsval fn, const jsval *argv, int argc, int taskIndex, bool addRoots) {

	// Create an array containing
	// [0] - the function
	// [1..N] - the arguments to the function

	auto_arr<jsval> toProxy(new jsval[3 + argc]);
	if (!toProxy.get()) {
		JS_ReportOutOfMemory(cx);
		return NULL;
	}
	int p = 0;
	toProxy[p++] = fn;
	for (int i = 0; i < argc+2; i++) {
		toProxy[p++] = argv[i];
	}

	EventCallback* cb = new EventCallback(cx, toProxy, argc, taskIndex);

	if(addRoots) {
		if (!cb->addRoots()) {
			return NULL;
		}
	}
	else
		cb->hasRoots(false);

	return cb;
}


EventCallback::~EventCallback() {}


JSBool
EventCallback::call(JSContext* cx, Membrane* m, JSObject* global, jsval* rval) {

	// Wrap the function:
	int p = 0;
	AutoValueRooter fn(cx, _toProxy[p++]);
	if (!m->wrap(fn.addr())) {
		return false;
	}

	// [ fun_cb, owner, evlabel, args... ]
	p+=2;
	auto_arr<jsval> argv(new jsval[_argc]); // ensure it gets freed
	if (!argv.get())
		return JS_FALSE;
	AutoArrayRooter argvRoot(cx, _argc, argv.get()); // ensure it is rooted
	for (int i = 0; i < _argc; i++) {
		argv[i] = _toProxy[p++];
		if (!m->wrap(&argv[i], this->_taskindex >= 0 ? true : false))
			return JS_FALSE;
	}

	return JS_CallFunctionValue(cx, global, fn.value(), _argc, argv.get(), rval);

//	jsval* aaa = NULL;
//	return JS_CallFunctionValue(cx, global, fn.value(), /*_argc, argv.get(), */ 0, aaa, rval);
}












































EventHandle*
EventHandle::create(JSContext* cx, EventLoop* parentWorker, MainLoop* mainLoop,
		EventCallback* callback) {

	return new EventHandle(cx, parentWorker, mainLoop, callback);
}


EventHandle::EventHandle(JSContext* cx, EventLoop* parentWorker, MainLoop* mainLoop,
		EventCallback* callback) :
				_parent(parentWorker), _main_loop(mainLoop), _cb(callback) {

	_has_wait_guard = false;
	_wait_guard = NULL;
	_guard_callback = NULL;
}


Membrane*
EventHandle::getNewMembraneObj(JSContext* mainCx, JSObject* mainGlobal, JSContext* workerCx, JSObject* workerGlobal, TxWrappersRack* rack) {

	if(rack->TxGetMembrane() == NULL) {

		// TODO: do we need safenatives here?
		JSNative n;
		JSNative safeNatives[] = { n };

		rack->TxSetMembrane(Membrane::create( mainCx, mainGlobal,
				workerCx, workerGlobal, safeNatives, rack, _main_loop));
	}

	return rack->TxGetMembrane();
}


unsigned
EventHandle::wrapGlobalObject(JSContext* cx, JSObject* mainGlobal, JSObject* workerGlobal, Membrane* m) {

	AutoIdVector props(cx);
	if (!GetPropertyNames(cx, mainGlobal,
			JSITER_OWNONLY
			| JSITER_HIDDEN
			,
			&props))
		return false;

	unsigned i = 0;

	for (jsid *v = props.begin(), *v_end = props.end(); v < v_end; v++) {
		jsid gid = *v, wid = *v; i++;

		//
		// TODO: here we should read-lock the global obj when emitting
		// 		from Tx (it's ok not to lock it for events emitted by
		//		the main JS script)
		//
#ifdef TXWRAPPERLOG
			if(JSID_IS_ATOM(gid)) {
				fprintf(stderr, "[%s] Wrapping 'global.%s' \n", _getThName(PR_GetCurrentThread()), JS_EncodeString(cx, JSID_TO_FLAT_STRING(gid)) );
			}
			else {
				fprintf(stderr, "[%s] Wrapping 'global.(unknown)' \n", _getThName(PR_GetCurrentThread()));
			}
#endif
		bool cont = false;
		{
			JSAutoCompartment ac(cx, workerGlobal);

			if (!m->wrapId(&wid))
				return false;

			// stop if this prop is already present on the child
			JSBool foundp;
			if (!JS_HasPropertyById(cx, workerGlobal, wid, &foundp))
				return false;
			cont = foundp;
#ifdef TXWRAPPERLOG
			if(foundp)
				fprintf(stderr,"[%s]   I: Prop already wrapped, skipping\n", _getThName(PR_GetCurrentThread()));
#endif
		}
		if(cont) continue;

		jsval pval;
		{
			JSAutoCompartment ac(cx, mainGlobal);

			if (!JS_GetPropertyById(cx, mainGlobal, gid, &pval))
				return false;
		}
		{
			JSAutoCompartment ac(cx, workerGlobal);

			if(!m->wrap(&pval))
				return false;
			if (!JS_SetPropertyById(cx, workerGlobal, wid, &pval))
				return false;
		}
	}

	return i;
}




bool
EventHandle::wrapObjectPrototype(JSContext* cx, JSObject* mainGlobal, JSObject* workerGlobal, Membrane* m) {

	AutoIdVector protoprops(cx);
	JSObject* globalObjectProto;
	JSObject* wrappedObjectProto;

	// XXX HACK: Wrap Object prorotype
	{
		JSAutoCompartment ac(cx, mainGlobal);
		globalObjectProto = (&mainGlobal->asGlobal())->getOrCreateObjectPrototype(cx);

		if (!GetPropertyNames(cx, globalObjectProto, JSITER_OWNONLY /*| JSITER_HIDDEN*/, &protoprops))
			return false;
	}
	{
		JSAutoCompartment ac(cx, workerGlobal);
		wrappedObjectProto = (&workerGlobal->asGlobal())->getOrCreateObjectPrototype(cx);
	}

	for (jsid *v = protoprops.begin(), *v_end = protoprops.end(); v < v_end; v++) {
		jsid gid = *v, wid = *v;

		JSBool foundp;
		{
			JSAutoCompartment ac(cx, workerGlobal);

			if (!m->wrapId(&wid))
				return false;

			if(!JS_HasPropertyById(cx, wrappedObjectProto, wid, &foundp))
				return false;
		}

		if( ! foundp) {
#ifdef TXWRAPPERLOG
			if(JSID_IS_ATOM(gid)) {
				fprintf(stderr, "[%s] Wrapping Object.prototype.%s \n", _getThName(PR_GetCurrentThread()), JS_EncodeString(cx, JSID_TO_FLAT_STRING(gid)) );
			}
			else {
				fprintf(stderr, "[%s] Wrapping global.(unknown) \n", _getThName(PR_GetCurrentThread()));
			}
#endif
			jsval pval;
			{
				JSAutoCompartment ac(cx, mainGlobal);

				if (!JS_GetPropertyById(cx, globalObjectProto, gid, &pval))
					return false;
			}
			{
				JSAutoCompartment ac(cx, workerGlobal);

				if(!m->wrap(&pval))
					return false;

				if (!JS_SetPropertyById(cx, wrappedObjectProto, wid, &pval))
					return false;
			}
		}
	}

	return true;
}







bool
EventHandle::initMembrane(JSContext* cx, JSObject* mainGlobal, JSObject* workerGlobal, Membrane* m) {

#ifdef TXWRAPPERLOG
	fprintf(stderr, "\n[%s] -- BEGIN MEMBRANE -- Creating new membrane (glob_obj props:%d, &:%p)\n", _getThName(PR_GetCurrentThread()), mainGlobal->propertyCount(), mainGlobal );
#endif

	if(!m->needsReinit()) {
#ifdef TXWRAPPERLOG
		fprintf(stderr, "[%s] -- END MEMBRANE -- Membrane creation skipped, using cached\n", _getThName(PR_GetCurrentThread()));
#endif
		return true;
	}

	unsigned i = 0;
	{
		m->_main_loop->lockMainLoop();
		//
		// (1) Wrap the global object
		//
		JSAutoRequest ar(cx);
		JSAutoCompartment ac(cx, mainGlobal);

		i = wrapGlobalObject(cx, mainGlobal, workerGlobal, m);

		//
		// (2) Wrap the Object prototype
		//
		JS_ASSERT( wrapObjectPrototype(cx, mainGlobal, workerGlobal, m) );

		m->setNeedsReinit(false);
		m->_main_loop->unlockMainLoop();
	}

#ifdef TXWRAPPERLOG
		fprintf(stderr, "[%s] -- END MEMBRANE -- Wrapped %d glob_obj props\n", _getThName(PR_GetCurrentThread()), i );
#endif

	return true;
}






void
EventHandle::prepareTxMetadata(JSContext* cx, TxWrappersRack* rack, MainLoop* mainLoop, deque<EventHandle*>& localQueue) {

	JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_started));
	rack->TxStart(mainLoop->getCurrentVersion());

	localQueue.clear();
}


void
EventHandle::emitBufferedEvents(JSContext* cx, deque<EventHandle*>& localQueue, MainLoop* mainLoop) {
	if( ! localQueue.empty()) {

		while( ! localQueue.empty()) {
			EventHandle* e = localQueue.front();
			localQueue.pop_front();

			JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_ev_started));
			mainLoop->emitEvent(cx, e);
		}
	}
}


char *
EventHandle::getPrintableEventLabel(JSContext* cx) {
	JSAutoRequest ar(cx);
	return JS_EncodeString(cx, getEventLabelString());
}


bool
EventHandle::execute(JSContext* cx, TxWrappersRack* rack, MainLoop* mainLoop, deque<EventHandle*>& localQueue) {

	struct timeval tv, start_tv, membrane_tv;
	gettimeofday(&start_tv, NULL);
	gettimeofday(&membrane_tv, NULL);

	JSContext* mainCx = _main_loop->getGlobalCx();
	JSObject* mainGlobal = mainLoop->getGlobalObject();
	JSContext* workerCx = cx;
	JSObject* workerGlobal = cx->global();

	JS_ASSERT(workerCx); JS_ASSERT(mainCx); JS_ASSERT(workerGlobal); JS_ASSERT(mainGlobal);

	//
	// Create a TxMembrane (with TxLog) for running the callback function
	//
	Membrane* m = getNewMembraneObj(mainCx, mainGlobal, workerCx, workerGlobal, rack);

	JS_ASSERT(m);

	double elapsed_m = 0.0;
	gettimeofday(&tv, NULL);
	elapsed_m = (tv.tv_sec - membrane_tv.tv_sec) + (tv.tv_usec - membrane_tv.tv_usec) / 1000000.0;
	T("Membrane created in : %.2fs\n", (double)(elapsed_m));

	//
	// Populate the new membrane adding the (proxied) globals
	//
	JS_ASSERT( initMembrane(cx, mainGlobal, workerGlobal, m) );

	//
	// Call the cloned function in the Tx
	//
	rack->TxInit();
	int _tx_result = TX_COMMIT_NOK;
	bool _tx_restart;
	bool _iterate = true;

	do
	{
		_tx_restart = false;
		JSAutoRequest ar(cx);

		prepareTxMetadata(cx, rack, mainLoop, localQueue);
		JS_ASSERT(localQueue.size()==0);

		TX("Tx starting (event '%s')\n", JS_EncodeString(cx, this->getEventLabelString()));

		Value v;
		if( ! _cb->call(cx, m, workerGlobal, &v) ) {

			if(rack->_tx_redo) {
				_tx_restart = true;
				JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_restarted));
				TX("Tx aborted, restarting\n");
				if(rack->_tx_redo_why == TX_RT_ABORT_VALIDATING_READ_SET_LOCK)
					JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_restarted_lock));
				if(rack->_tx_redo_why == TX_RT_ABORT_VALIDATING_READ_SET_VERSION)
					JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_restarted_version));
			}
			else {
				TX("OTHER ERROR\n");// XXX ?
				return false;
			}
		}
		else {
			_tx_result = rack->TxCommit(cx, *mainLoop->getCurrentVersion());

			if(_tx_result != TX_COMMIT_OK) {
				TX("Tx failed committing (reason:%d), restarting\n", _tx_result);
				JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_commit_failed));

				if(_tx_result == TX_COMM_ABORT_LOCK_WRITE_SET)
					JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_commit_lock));
				if(_tx_result == TX_COMM_ABORT_VALIDATING_READ_SET_VERSION)
					JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_commit_version));
			}
			else {
				TX("Tx committed :)\n");
				JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_committed));
			}
		}

		_iterate = ((_tx_restart == true) || (_tx_result != TX_COMMIT_OK));
	}
	while( _iterate );

	JS_ASSERT(TX_COMMIT_OK == rack->TxEventualCommit(cx));

	emitBufferedEvents(cx, localQueue, mainLoop);

	JS_ATOMIC_INCREMENT(&(mainLoop->getStats()->_tx_completed));

	double elapsed = 0.0;
	gettimeofday(&tv, NULL);
	elapsed = (tv.tv_sec - start_tv.tv_sec) + (tv.tv_usec - start_tv.tv_usec) / 1000000.0;
	T("Time taken: %.2fs\n", (double)(elapsed));

	return true;
}





}
