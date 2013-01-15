/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the TigerQuoll project (https://github.com/eleinadani/TigerQuoll/).
 *
 * The Initial Developer of the Original Code are
 * Mozilla Corporation and University of Lugano (USI).
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniele Bonetta <bonettad@usi.ch>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
