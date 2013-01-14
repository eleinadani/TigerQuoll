/*
 * tqevent.h
 *
 *  Created on: Dec 7, 2012
 *      Author: usiusi
 */

#ifndef TQEVENT_H_
#define TQEVENT_H_

#include <jsapi.h>
#include "jscntxt.h"
#include <deque>
#include "pjsutil.h"


namespace tq {

class MainLoop;
struct GuardCounter;
class EventLoop;
class EventHandle;

namespace tx {
class TxWrappersRack;
class Membrane;
}


using namespace tx;
using namespace std;
using namespace pjs;


//------------------------------------------------------------------------
// EventCallback class
//

enum {
	PX_CALLBACK, PX_OBJ_OWNER, PX_EV_LABEL
};

class EventCallback {
private:
	JSContext* _original_cx;
	auto_arr<jsval> _toProxy; // [0] == fn, [1..argc] == args
	unsigned _argc;
	unsigned _rooted;
	int _taskindex;
	bool _has_roots;

	EventCallback(JSContext* cx, auto_arr<jsval>& toProxy, unsigned argc, unsigned taskIndex = -1) :
					_original_cx(cx), _toProxy(toProxy), _argc(argc), _rooted(0),
					_taskindex(taskIndex), _has_roots(true) {}

public:
	static EventCallback *create(JSContext* cx, jsval fn, const jsval *argv, int argc, int taskIndex = -1, bool addRoots = true);
	~EventCallback();

	JSBool call(JSContext* cx, Membrane* m, JSObject* global, jsval* rval);

	bool addRoots();
	void delRoots();
	void hasRoots(bool has) {
		_has_roots = has;
	}

	friend class EventHandle;
};

//
// end EventCallback class
//------------------------------------------------------------------------





//------------------------------------------------------------------------
// EventHandle class
//

class EventHandle {

	EventLoop* _parent;
	MainLoop* _main_loop;

	EventCallback* _cb;
	JSFunction* _guard_callback;


public:
	EventHandle(JSContext* cx, EventLoop* parentWorker, MainLoop* mainLoop,
			EventCallback* callback);

	static EventHandle* create(JSContext* cx, EventLoop* parentWorker, MainLoop* mainLoop,
			EventCallback* callback);

	bool _has_wait_guard;
	GuardCounter* _wait_guard;

	Membrane* getNewMembraneObj(JSContext* mainCx, JSObject* mainGlobal, JSContext* workerCx, JSObject* workerGlobal, tx::TxWrappersRack* rack);
	unsigned wrapGlobalObject(JSContext* cx, JSObject* mainGlobal, JSObject* workerGlobal, tx::Membrane* m);
	bool wrapObjectPrototype(JSContext* cx, JSObject* mainGlobal, JSObject* workerGlobal, Membrane* m);
	bool initMembrane(JSContext* cx, JSObject* mainGlobal, JSObject* workerGlobal, tx::Membrane* m);

	JS::Value wrapCallbackEvent(JSContext* cx, JSFunction* toWrap, Membrane* m);
	jsval* wrapCallbackArgs(JSContext* cx, Membrane* m);
	void emitBufferedEvents(JSContext* cx, deque<EventHandle*>& localQueue, MainLoop* mainLoop);
	void prepareTxMetadata(JSContext* cx, TxWrappersRack* rack, MainLoop* mainLoop, deque<EventHandle*>& localQueue);
	char* getPrintableEventLabel(JSContext* cx);

	bool execute(JSContext* cx, tx::TxWrappersRack* rack,
			MainLoop* mainLoop, deque<EventHandle*>& localQueue);

	EventHandle* getGuardEvent(JSContext* cx) {

		int argc = 3;
		jsval *argv = new jsval[argc];

		argv[0] = OBJECT_TO_JSVAL(_guard_callback);
		argv[1] = _cb->_toProxy[PX_OBJ_OWNER];
		argv[2] = _cb->_toProxy[PX_EV_LABEL];

		EventCallback* cb = EventCallback::create(cx, argv[0], argv + 1, argc - 1, -1, false);
		return EventHandle::create(cx, _parent, _main_loop, cb);
	}


	JSObject* getObjOwner() {
		return JSVAL_TO_OBJECT(_cb->_toProxy[PX_OBJ_OWNER]);
	}


	JSString* getEventLabelString() {
		return JSVAL_TO_STRING(_cb->_toProxy[PX_EV_LABEL]);
	}

	~EventHandle() {
		//_fun.~Rooted();
	}

	friend class MainLoop;
	friend class EventCallback;
	friend class LoopWorker;
};

//
// End EventHandle Class
//------------------------------------------------------------------------






}




#endif /* TQEVENT_H_ */
