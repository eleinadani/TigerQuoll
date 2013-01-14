/*
 * tqtxmembrane.h
 *
 *  Created on: Nov 13, 2012
 *      Author: usiusi
 */

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
