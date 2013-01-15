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
