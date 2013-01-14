/*
 * tqwaitguard.h
 *
 *  Created on: Dec 9, 2012
 *      Author: usiusi
 */

#ifndef TQWAITGUARD_H_
#define TQWAITGUARD_H_

#include "jsapi.h"
#include <HashTable.h>


namespace tq {

struct WaitGuardHasher
{
    typedef const Value Lookup;

    static HashNumber hash(Lookup &key) {
    	//FIXME total hack 3 is randomly chosen.
        return key.payloadAsRawUint32() | uint32_t(3);
    }

    static bool match(Lookup &l, Lookup &k) {
        return l == k;
    }
};


struct GuardCounter {
	int64_t _guard_counter;
	GuardCounter() :_guard_counter(0) {}
};




typedef HashMap<Value, GuardCounter*, WaitGuardHasher, SystemAllocPolicy> WaitCounterMap;

class WaitGuard {

	WaitCounterMap _event_guards;
	JSFunction* _callback;

public:
	WaitGuard(JSFunction* cb) {
		_event_guards.init();
		_callback = cb;
	}

	void
	putGuardCounter(JSString* ev, GuardCounter* gc) {

		_event_guards.put( STRING_TO_JSVAL(ev), gc);
	}

	WaitCounterMap::Ptr
	getGuardCounter(JSString* ev) {
		return _event_guards.lookup( STRING_TO_JSVAL(ev) );
	}

	JSFunction*
	getCallback() {
		return _callback;
	}
};


typedef HashMap<Value, WaitGuard*, WaitGuardHasher, SystemAllocPolicy> WaitGuardMap;

}



#endif /* TQWAITGUARD_H_ */
