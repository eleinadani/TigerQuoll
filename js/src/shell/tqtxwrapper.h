/*
 * tqtxwrapper.h
 *
 *  Created on: Nov 16, 2012
 *      Author: usiusi
 */

#ifndef TQTXWRAPPER_H_
#define TQTXWRAPPER_H_

#include <jsapi.h>
#include <jsgc.h>
#include <jsfriendapi.h>
#include <HashTable.h>
#include <Vector.h>
#include <jsproxy.h>
#include <jscompartment.h>

#include <jswrapper.h>
#include <queue>
//#include "tqtxutils.h"
#include <memory>

namespace tq {
namespace tx {

class Membrane;

using namespace js;
using namespace std;

struct ProxyHasher
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

class TxWrapper;

typedef HashMap<Value, JSObject*, ProxyHasher, SystemAllocPolicy> ProxyMap;
typedef HashMap<Value, JSObject*, ProxyHasher, SystemAllocPolicy> TxLog;
typedef HashMap<Value, jsid, ProxyHasher, SystemAllocPolicy> JsidMap;

typedef Vector<JSObject*, 0, SystemAllocPolicy> JSObjectSet;
typedef Vector<TxWrapper*, 0, SystemAllocPolicy> TxWrapperSet;
typedef Vector<PropLock*, 0, SystemAllocPolicy> PropLockSet;



/*
 * TxWrappersRack is a per-thread, per-transaction object. The Tx logs are stored
 * in this structure and are restored/cleaned for every new Tx.
 *
 * The Rack also uses the TxMembrane. For now the membrane is created for each Tx.
 * In the future the membrane can be cached for performance reasons.
 *
 * */

class TxWrappersRack {
private:

	JSContext *_cx;
	JSObject *_global;

	static char *PROXYRACK;

	PRLock *_proxyRackLock;
	ProxyMap _map;
	int _getCount;
	JsidMap _ids_cache;

	auto_ptr<Membrane> _m;

	TxWrappersRack(JSContext* cx, JSObject* global);

public:

	/*
	 * Tx metadata
	 * */
	int64_t* _globalVersion;
	int64_t _RV;
	bool _tx_redo;
	int _tx_redo_why;

	TxWrapperSet _read_set;
	TxWrapperSet _write_set;
	TxWrapperSet _ev_write_set;
	PropLockSet _lock_set;

	/*
	 * Tx management
	 * */
	void TxInit();
	void TxStart(int64_t* globalVersion);
	void TxAbort(JSContext* cx, int why);
	int TxCommit(JSContext* cx, int64_t &globalClock);
	int TxEventualCommit(JSContext* cx);

	bool redoLogAddObject(TxWrapper* wrapperObj);
	bool readSetAddObject(TxWrapper* wrapperObj);
	bool eventualLogAddObject(TxWrapper* wrapperObj);

	/*
	 * TxWrappers management
	 * */
	static TxWrappersRack *create(JSContext *parentCx, JSObject *parentGlobal);

	JSObject *getWrapperObject(JSContext* cx, const Value& priv, JSObject* proto,
			JSObject* parent, JSObject* call, JSObject* construct);

	~TxWrappersRack() {
	}

	void TxSetMembrane(Membrane* membrane) {
		_m.reset(membrane);
	}

	Membrane* TxGetMembrane() {
		return _m.get();
	}


	bool
	idCacheLookup(JSContext* cx, Value val, jsid *id) {
		// If the id has been cached, return it
		if (JsidMap::Ptr p = _ids_cache.lookup(val)) {
			*id = p->value;
			return true;
		}

		return false;
	}

	void
	idCachePut(JSContext* cx, Value val, jsid id) {

//		fprintf(stderr, " -- cache add '%s'\n", JS_EncodeString(cx, JSVAL_TO_STRING(val)));
		_ids_cache.put(val, id);
	}
};






/* Base class for all cross compartment wrapper handlers. */
class JS_FRIEND_API(TxWrapper) : public CrossCompartmentWrapper
{
  private:
	TxWrappersRack* _rk;

	JSObject* _local_clone;
	JSObject* _actual_obj;

	// eventual Tx
	JSObject* _local_clone_initial;
	bool _eventual_fields_checked;
	bool _all_eventual;
	bool _has_eventuals;

	Vector<jsid, 0, SystemAllocPolicy> _eventual_props_cache;

  public:

	JSObject* getLocalClone() { return _local_clone; };
	JSObject* getLocalCloneInitial() { return _local_clone_initial; };
	JSObject* getActualObj() { return _actual_obj; };
    bool propIsEventual(jsid pid);

    void invalidateLocalClones() {
    	_local_clone = NULL;
    	_local_clone_initial = NULL;
    }

	TxWrapper(unsigned flags, TxWrappersRack* rack, JSObject* actualObj, bool hasPrototype = false);

	// Tx wrapper traps
	bool txGet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, Value *vp);
    bool txSet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, bool strict, Value *vp);
	bool directWrapperGet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, Value *vp);

	// eventual Tx wrapper traps
	bool evGet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, Value *vp);
    bool evSet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, bool strict, Value *vp);

    virtual ~TxWrapper();

    /* ES5 Harmony fundamental wrapper traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                       PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                          PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                PropertyDescriptor *desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props) MOZ_OVERRIDE;

    /* ES5 Harmony derived wrapper traps. */
    virtual bool has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp) MOZ_OVERRIDE;

    /**************************************************************************************************/
    virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid lid, Value *vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid lid, bool strict,
                     Value *vp) MOZ_OVERRIDE;
    /**************************************************************************************************/

    virtual bool keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp) MOZ_OVERRIDE;

    /* Spidermonkey extensions. */
    virtual bool call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp) MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, JSObject *wrapper, unsigned argc, Value *argv, Value *rval) MOZ_OVERRIDE;
    virtual bool nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                            CallArgs args) MOZ_OVERRIDE;
    virtual bool hasInstance(JSContext *cx, HandleObject wrapper, MutableHandleValue v, bool *bp) MOZ_OVERRIDE;
    virtual JSString *obj_toString(JSContext *cx, JSObject *wrapper) MOZ_OVERRIDE;
    virtual JSString *fun_toString(JSContext *cx, JSObject *wrapper, unsigned indent) MOZ_OVERRIDE;
    virtual bool regexp_toShared(JSContext *cx, JSObject *proxy, RegExpGuard *g) MOZ_OVERRIDE;
    virtual bool defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp) MOZ_OVERRIDE;
//    virtual bool iteratorNext(JSContext *cx, JSObject *wrapper, Value *vp);
    virtual bool getPrototypeOf(JSContext *cx, JSObject *proxy, JSObject **protop);

//    static TxWrapper singleton;
//    static TxWrapper singletonWithPrototype;
};



}

} /* namespace tq::tx */


#endif /* TQTXWRAPPER_H_ */
