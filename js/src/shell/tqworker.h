/*
 * tqworker.h
 *
 *  Created on: Dec 7, 2012
 *      Author: usiusi
 */

#ifndef TQWORKER_H_
#define TQWORKER_H_

#include "mozilla/Util.h"
#include <jsapi.h>
#include <deque>
#include <memory>
#include "tqmainloop.h"


namespace tq {

namespace tx {
class TxWrappersRack;
}
class EventHandle;
class MainLoop;

using namespace tx;
using namespace std;

//------------------------------------------------------------------------
// Worker Thread class
//

class LoopWorker : public EventLoop {

	pthread_t _thread;
	JSRuntime* _rt;
	JSContext* _cx;
	JSObject* _global;

	int _id;
	MainLoop* _main_loop;

	deque<EventHandle*> _local_event_queue;

	auto_ptr<TxWrappersRack> _tx_wrappers;

	bool initLoop();

	static void* runnable(void* ptr) {

		LoopWorker* worker = (LoopWorker*) ptr;

		worker->_main_loop->lockMainLoop();
		worker->initLoop();
		worker->_main_loop->unlockMainLoop();

		worker->runEvLoop();

		worker->disposeLoop();

		return NULL;
	}

	JSBool delRoot(JSContext *cx);
	JSBool addRoot(JSContext *cx);

public:

	LoopWorker() {

		_rt = NULL;
		_cx = NULL;
		_id = -1;
		_main_loop = NULL;
	}

	~LoopWorker() {}


	int Start(int id, MainLoop* mainLoop) {

		_id = id;
		_main_loop = mainLoop;

		return pthread_create(&_thread, NULL, runnable, (void*)this);
	}

	void Join() {

		pthread_join(_thread, NULL);
	}

	/*
	 * Main event loop
	 **/
	void runEvLoop();

	void disposeLoop() {
//	    JS_DestroyContextNoGC(_cx);
//	    JS_DestroyRuntime(_rt);
		delRoot(_cx);
	}

	void bufferEventEmission(JSContext* cx, EventHandle* e) {
		_local_event_queue.push_back(e);
	}

	void disposeEventCallback(EventCallback* cb) {
		// TODO: implement this!!
	}
};

//
// End Worker Thread class
//------------------------------------------------------------------------

}



#endif /* TQWORKER_H_ */
