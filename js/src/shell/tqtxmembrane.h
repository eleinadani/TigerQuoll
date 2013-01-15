/* vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http: //www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the PJs project.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Nicholas Matsakis <nmatsakis@mozilla.com>
 * Donovan Preston <dpreston@mozilla.com>
 * Fadi Meawad <fmeawad@mozilla.com>
 *
 * The code has been forked from the Original Code to the TigerQuoll project.
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

#ifndef TQTXMEMBRANE_H_
#define TQTXMEMBRANE_H_

#include <jsapi.h>
#include <jsgc.h>
#include <jsfriendapi.h>
#include <HashTable.h>
#include <jsproxy.h>
#include <jscompartment.h>

#include "tqtxwrapper.h"
#include "pjsutil.h"

namespace tq {

class MainLoop;

namespace tx {


using namespace js;
using namespace std;
using namespace pjs;

class ProxyRooter
{
private:
    JSContext *_cx;
    auto_arr<Value> _values;
    int _size, _capacity;
    ProxyRooter *_next;

    ProxyRooter(JSContext *cx, auto_arr<Value> &v, int c, ProxyRooter *n)
        : _cx(cx)
        , _values(v)
        , _size(0)
        , _capacity(c)
        , _next(n)
    {}

public:
    static ProxyRooter *create(JSContext *cx, int c, ProxyRooter *n) {
        auto_arr<Value> values(new Value[c]);
        if (!values.get()) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }

        ProxyRooter *pr = new ProxyRooter(cx, values, c, n);
        if (!pr) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }

        return pr;
    }

    ~ProxyRooter() {

    	JSAutoRequest rq(_cx);

        for (int i = 0; i < _size; i++) {
            JS_RemoveValueRoot(_cx, &_values[i]);
        }
        delete _next;
    }

    JSBool add(Value value, ProxyRooter **rval) {
        JS_ASSERT(*rval == this);

        if (_size == _capacity) {
            ProxyRooter *pr = ProxyRooter::create(_cx, _capacity, this);
            if (!pr) {
                return false;
            }
            *rval = pr;
            return pr->add(value, rval);
        }

        _values[_size] = value;
        if (!JS_AddValueRoot(_cx, &_values[_size]))
            return false;
        _size++;
        return true;
    }
};






class Membrane {

	friend class JSFunction;

private:
	// Maps from proxies in the parent space to wrapper object in
	// child space.
	WrapperMap _map;
	JSContext *_parentCx;
	JSObject *_parentGlobal;
	JSContext *_childCx;
	JSObject *_childGlobal;
	JSCompartment *_childCompartment;
	JSNative *_safeNatives;
	TxWrappersRack *_wrappersRack;
public:
	MainLoop* _main_loop;
private:
	bool _needs_init;

	ProxyRooter* _rooter;


	Membrane(JSContext *parentCx, JSObject *parentGlobal, JSContext *childCx,
			JSObject *childGlobal, JSNative *safeNatives, TxWrappersRack *wrappersRack, MainLoop* ml) :
			_parentCx(parentCx), _parentGlobal(parentGlobal), _childCx(childCx),
					_childGlobal(childGlobal),
					_childCompartment(_childGlobal->compartment()),
					_safeNatives(safeNatives), _wrappersRack(wrappersRack), _main_loop(ml), _needs_init(true),
					_rooter(NULL) {	}

	bool isSafeNative(JSNative n);

	JSBool put(Value key, Value value);

	JSObject* copyAndWrapEnvironment(JSContext *cx, JSFunction *cloned_fn, JSObject* orig_env);
	bool copyAndWrapProperties(JSContext* cx, JSFunction *cloned_fn, JSObject* wrapped_fun);

	static char *MEMBRANE;

public:
	static Membrane *create(JSContext *parentCx, JSObject *parentGlobal,
			JSContext* childCx, JSObject *childGlobal, JSNative *safeNatives,
			TxWrappersRack *wrappersRack, MainLoop* mainLoop);
	~Membrane();
	void releaseProxies();

	bool needsReinit() {
		return _needs_init;
	}

	void setNeedsReinit(bool init) {
		_needs_init = init;
	}

	bool wrap(Value *vp, bool isArg=false);
	bool wrapId(jsid *idp);
	bool unwrap(Value *vp);

	bool unwrapId(jsid *idp);
	bool wrap(AutoIdVector &props);
	bool wrap(PropertyOp *propp);
	bool wrap(StrictPropertyOp *propp);
	bool wrap(PropertyDescriptor *desc);
	bool wrap(JSObject **objp);
	bool wrap(HeapPtrAtom *objp);
	bool wrap(JSString **strp);
	bool wrap(HeapPtrString *strp);

	void cacheWrappedIds(JSContext* cx, jsid mainObjId, jsid wrappedId);
	jsid getUnwrappedId(JSContext* cx, jsid wrappedId);


	static bool IsCrossThreadWrapper(/*const*/ JSObject *wrapper);

//	const static int TYPED_ARRAY_NOWRAP_SLOT = 5;
	// ______________________________________________________________________
};


}


}


#endif /* TQTXMEMBRANE_H_ */
