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

#ifndef TQMAINLOOP_H_
#define TQMAINLOOP_H_

#include <queue>
#include <prmon.h>

#include "tqevent.h"
#include "tqwaitguard.h"


namespace tq {

struct stats {
	int32_t _tx_started;
	int32_t _ev_started;
	int32_t _tx_committed;
	int32_t _tx_completed;
	int32_t _tx_restarted;
	int32_t _tx_restarted_lock;
	int32_t _tx_restarted_version;
	int32_t _tx_commit_failed;
	int32_t _tx_commit_lock;
	int32_t _tx_commit_version;
};

typedef std::queue<EventHandle*> EventQueue;



class EventLoop {
public:
	virtual void bufferEventEmission(JSContext* cx, EventHandle* e) = 0;
	virtual void disposeEventCallback(EventCallback* cb) = 0;
};





class MainLoop : public EventLoop {

	stats _stats;

	int64_t _CurrentV;
	bool _die;
	int32_t _runningWorkers;

	unsigned gThreadIndex;

	EventQueue _event_queue;
	EventQueue _local_event_queue;
	pthread_mutex_t _queue_lk;

	JSContext* _global_cx;
	JSObject* _global_object;
	WaitGuardMap _wait_guard_map;

public:

	unsigned *getThreadIndex() {
		return &gThreadIndex;
	}

	MainLoop() : _CurrentV(0), _die(false), _runningWorkers(0) {
		resetStats();
		_wait_guard_map.init();
		gThreadIndex = -1;
		pthread_mutex_init(&_queue_lk, NULL);
	}

	~MainLoop() {
		pthread_mutex_destroy(&_queue_lk);
		// TODO: empty and free roots _wait_guard_map
	}


	void setGlobalAndContext(JSContext* cx, JSObject* global) {
		_global_cx = cx;
		_global_object = global;
	}

	JSObject* getGlobalObject() {

		return _global_object;
	}

	JSContext* getGlobalCx() {

		return _global_cx;
	}


	int32_t* getRunningWorkers() {
		return &_runningWorkers;
	}

	int64_t* getCurrentVersion() {
		return &_CurrentV;
	}

	bool terminate() {
		return _die;
	}

	void setTerminate(bool die) {
		_die = die;
	}


	void lockMainLoop() {
		pthread_mutex_lock(&_queue_lk);
	}


	void unlockMainLoop() {
		pthread_mutex_unlock(&_queue_lk);
	}


	void bufferEventEmission(JSContext* cx, EventHandle* e) {

		_local_event_queue.push(e);
	}


	void emitEvent(JSContext* cx, EventHandle* e) {

		lockMainLoop();
		checkAndSetWaitGuard(cx, e);
		_event_queue.push(e);
		unlockMainLoop();
	}


	void emitBufferedEvents(JSContext* cx) {

		if( ! _local_event_queue.empty()) {

			while( ! _local_event_queue.empty()) {
				EventHandle* e = _local_event_queue.front();
				_local_event_queue.pop();

				emitEvent(cx, e);
			}
		}
	}



	EventHandle* popEvent() {

		lockMainLoop();
		if ( ! _event_queue.empty()) {
			EventHandle* e = _event_queue.front();
			_event_queue.pop();
			unlockMainLoop();
			return e;
		}
		unlockMainLoop();
		return NULL;
	}


	void checkAndSetWaitGuard(JSContext* cx, EventHandle* e) {

		// TODO: lock to protect when registering new guard?

		WaitGuardMap::Ptr p = _wait_guard_map.lookup( OBJECT_TO_JSVAL(e->getObjOwner()) );

		if (p.found()) {
			WaitGuard* g = p->value;

			WaitCounterMap::Ptr p2 = g->getGuardCounter( e->getEventLabelString() );
			if (p2.found()) {
				GuardCounter* guard_counter = p2->value;
				e->_has_wait_guard = true;
				e->_wait_guard = guard_counter;
				e->_guard_callback = g->getCallback();

				JS_ATOMIC_INCREMENT(&(e->_wait_guard->_guard_counter));
			}
		}
	}

	void setNewWaitGuard(JSContext* cx, JSFunction* callbackFunction, JSObject* objOwner, JSString* eventLabel) {

		WaitGuardMap::Ptr p = _wait_guard_map.lookup( OBJECT_TO_JSVAL(objOwner) );

		if (p.found()) {
			WaitGuard* g = p->value;
			GuardCounter* gc = new GuardCounter();
			g->putGuardCounter(eventLabel, gc);
		}
		else {
			WaitGuard* g = new WaitGuard(callbackFunction);
			GuardCounter* gc = new GuardCounter();
			g->putGuardCounter(eventLabel, gc);

			_wait_guard_map.put(OBJECT_TO_JSVAL(objOwner), g);
		}
	}

	void clearWaitGuard(JSContext* cx, EventHandle* e) {

		_wait_guard_map.remove( OBJECT_TO_JSVAL(e->getObjOwner()) );
	}

	void resetStats() {
		_stats._tx_started = 0;
		_stats._ev_started = 0;
		_stats._tx_committed = 0;
		_stats._tx_completed = 0;
		_stats._tx_restarted = 0;
		_stats._tx_restarted_lock = 0;
		_stats._tx_restarted_version = 0;
		_stats._tx_commit_failed = 0;
		_stats._tx_commit_lock = 0;
		_stats._tx_commit_version = 0;
	}

	stats* getStats() {
		return &_stats;
	}

	void dumpStats() {

		double efficiency = (double)( (double)_stats._tx_committed / (double)_stats._tx_started );

		fprintf(stderr,"\n");
		fprintf(stderr,"       Events emitted      : %d \n", _stats._ev_started);
		fprintf(stderr,"       Tx committed        : %d \n", _stats._tx_committed);
		fprintf(stderr,"       Tx started          : %d \n", _stats._tx_started);
		fprintf(stderr,"       Tx failed (eff)     : %d (%.2f) \n", (_stats._tx_started-_stats._tx_committed), (double)efficiency );
		fprintf(stderr,"          +- Rt aborts     : %d \n", _stats._tx_restarted);
		fprintf(stderr,"              +-- locked   : %d \n", _stats._tx_restarted_lock);
		fprintf(stderr,"              +-- version  : %d \n", _stats._tx_restarted_version);
		fprintf(stderr,"          +- Commit aborts : %d \n", _stats._tx_commit_failed);
		fprintf(stderr,"              +-- locked   : %d \n", _stats._tx_commit_lock);
		fprintf(stderr,"              +-- version  : %d \n", _stats._tx_commit_version);
	}




	/*
	 * Dispose roots and deferred unwrapping of ids
	 * */

	struct UnwrappableId {
		jsid _towrap;
		jsid* _unwrapped;
		PRMonitor* _monitor;
		UnwrappableId(jsid towrap, jsid* unwrapped, PRMonitor* monitor) :
			_towrap(towrap), _unwrapped(unwrapped), _monitor(monitor) {}
	};



	pthread_t _disposer_thread;
	pthread_mutex_t _disposer_lock;
	pthread_mutex_t _unwrapper_lock;

	struct DisposerLoop {

		MainLoop* _main_loop;
		DisposerLoop(MainLoop* mainLoop) : _main_loop(mainLoop), _die(false) {
		}

		deque<EventCallback*> _disposable_events;
		deque<UnwrappableId*> _unwrappable_ids;
		bool _die;
		void loop() {
			while(!_die) {
				pthread_mutex_lock(&_main_loop->_disposer_lock);
				if(_disposable_events.empty()) {
					// wait...
				}
				else {
					EventCallback* cb = _disposable_events.back();
					_disposable_events.pop_back();
					cb->delRoots();
					delete cb;
				}
				pthread_mutex_unlock(&_main_loop->_disposer_lock);


				pthread_mutex_lock(&_main_loop->_unwrapper_lock);
				if(!_unwrappable_ids.empty()) {
					UnwrappableId* uid = _unwrappable_ids.back();
					_unwrappable_ids.pop_back();

					{
						JSAutoRequest rq(_main_loop->_global_cx);

						Value v;
						JS_IdToValue(_main_loop->_global_cx, uid->_towrap, &v);

						jsid newid;

						RootedValue orig(_main_loop->_global_cx, v);
						JSString *str = v.toString();
						if (!str)
							fprintf(stderr, "[TQ] Error: Cannot unwrap!\n");

						_main_loop->lockMainLoop();
						JSString *wrapped;
						if(!str->isAtom()) {
							wrapped = js_NewStringCopyN(_main_loop->_global_cx, str->getChars(_main_loop->_global_cx), str->length());
						}
						else {
							const jschar *chars = str->getChars(_main_loop->_global_cx);
							if (!chars)
								fprintf(stderr, "[TQ] Error: Cannot unwrap!\n");
							wrapped = AtomizeChars(_main_loop->_global_cx, chars, str->length());
						}
						_main_loop->unlockMainLoop();

						if (!wrapped)
							fprintf(stderr, "[TQ] Error: Cannot unwrap!\n");

						v.setString(wrapped);
						JS_ValueToId(_main_loop->_global_cx, v, &newid);

						*(uid->_unwrapped) = newid;

						PR_EnterMonitor(uid->_monitor);
						PR_Notify(uid->_monitor);
						PR_ExitMonitor(uid->_monitor);
					}

					delete uid;
				}
				pthread_mutex_unlock(&_main_loop->_unwrapper_lock);
			}
		}

		static void* runnable(void* ptr) {

			DisposerLoop* disposer = (DisposerLoop*) ptr;
			JS_SetRuntimeThread(disposer->_main_loop->_global_cx->runtime);

			disposer->loop();

			JS_ClearRuntimeThread(disposer->_main_loop->_global_cx->runtime);
			return NULL;
		}

	};


	DisposerLoop* _disposer_loop;


	void runEventDisposerLoop() {

		_disposer_loop = new DisposerLoop(this);

		pthread_mutex_init(&_disposer_lock, NULL);
		pthread_mutex_init(&_unwrapper_lock, NULL);
		pthread_create(&_disposer_thread, NULL, DisposerLoop::runnable, (void*)_disposer_loop);
	}


	void joinEventDisposerLoop() {

		_disposer_loop->_die = true;
		pthread_join(_disposer_thread, NULL);
		pthread_mutex_destroy(&_disposer_lock);
		pthread_mutex_destroy(&_unwrapper_lock);
	}


	void disposeEventCallback(EventCallback* cb) {

		pthread_mutex_lock(&_disposer_lock);
		_disposer_loop->_disposable_events.push_front(cb);
		pthread_mutex_unlock(&_disposer_lock);
	}


	void unwrapIdInRuntime(jsid towrap, jsid* unwrapped, PRMonitor* monitor) {

		UnwrappableId* uid = new UnwrappableId(towrap, unwrapped, monitor);
		pthread_mutex_lock(&_unwrapper_lock);
		_disposer_loop->_unwrappable_ids.push_front(uid);
		pthread_mutex_unlock(&_unwrapper_lock);
	}

};


}

#endif /* TQMAINLOOP_H_ */
