/*
 * tqtxwrapper.cpp
 *
 *  Created on: Nov 16, 2012
 *      Author: usiusi
 */



/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the PJs project.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fadi Meawad <fmeawad@mozilla.com>
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

#include "tqtxwrapper.h"
#include "tqtxmembrane.h"
// TODO: do we really need this anymore?
#include "pjsutil.h"

#include "jswrapper.h"
#include "tqtxutils.h"

#include "tqtxlockscache.h"

#include "jscntxtinlines.h"

#include "tqmainloop.h"

//#define NOTHING (true)

namespace tq {
namespace tx {


char *TxWrappersRack::PROXYRACK = "ProxyRack";

/* Wrappers rack */

TxWrappersRack::TxWrappersRack(JSContext* cx, JSObject* global) :
		_cx(cx), _global(global), _proxyRackLock(PR_NewLock()), _getCount(0), _m(NULL) {
	_globalVersion = NULL;
}


TxWrappersRack* TxWrappersRack::create(JSContext* cx, JSObject* global) {
	TxWrappersRack* rack = new TxWrappersRack(cx, global);
	if ((!rack->_map.init() )|| (!rack->_ids_cache.init())) {
		delete rack;
		return NULL;
	}

	return rack;
}


JSObject *TxWrappersRack::getWrapperObject(JSContext* cx, const Value& priv,
		JSObject* proto, JSObject* parent, JSObject* call,
		JSObject* construct) {
	{
		pjs::AutoLock hold(_proxyRackLock);
		/* If we already have a proxy for this value, use it. */
		ProxyMap::Ptr p = _map.lookup(priv);
		if (p.found())
			return p->value;

		ASSERT(_m.get());

		JSObject* obj = JSVAL_TO_OBJECT(priv);

		JS_ASSERT( ! obj->isProxy() );

		TxWrapper* txw = new TxWrapper(0, this, obj);

		JSObject *wrapper = NewProxyObject(cx,
				txw /*TxWrapper::singleton.toBaseProxyHandler() */,
				priv,
				proto,
				parent,
				call,
				construct);

		_map.put(priv, wrapper);

		return wrapper;
	}
}


void TxWrappersRack::TxInit() {

	_RV = -1;
	_tx_redo = false;
	_tx_redo_why = TX_COMMIT_NOK;
}

void TxWrappersRack::TxStart(int64_t* globalVersion) {

	_globalVersion = globalVersion;
	_RV = -1;
	_tx_redo = false;
	_tx_redo_why = TX_COMMIT_NOK;

	for(unsigned i = 0; i<_read_set.length(); i++)
		_read_set[i]->getLocalClone()->getTxMetadata()->ClearMetadata();

	for(unsigned i = 0; i<_write_set.length(); i++)
		_write_set[i]->getLocalClone()->getTxMetadata()->ClearMetadata();

	for(unsigned i = 0; i<_ev_write_set.length(); i++)
		_ev_write_set[i]->invalidateLocalClones();

	_read_set.clear();
	_write_set.clear();
	_lock_set.clear();
	_ev_write_set.clear();
}

void TxWrappersRack::TxAbort(JSContext* cx, int why) {

	_tx_redo = true;
	_tx_redo_why = why;
	TX("Tx aborted (reason:%d)\n",why);
}

#define TXABORT(n) \
{ \
	TX("Aborting Tx (reason:%d)\n", n); \
	_tx_ok = n; \
	goto abort;	\
} \





bool TxWrappersRack::eventualLogAddObject(TxWrapper* wrapperObj) {

	TxWrapper** w_begin = _ev_write_set.begin();
	TxWrapper** w_end = _ev_write_set.end();

	bool found = false;
	for(; w_begin < w_end; w_begin++ ) {
		TxWrapper* w_ = *w_begin;

		if(w_ == wrapperObj)
			return false;
	}

	_ev_write_set.append(wrapperObj);
	return true;
}

bool TxWrappersRack::redoLogAddObject(TxWrapper* wrapperObj) {

	TxWrapper** w_begin = _write_set.begin();
	TxWrapper** w_end = _write_set.end();

	bool found = false;
	for(; w_begin < w_end; w_begin++ ) {
		TxWrapper* w_ = *w_begin;

		if(w_ == wrapperObj)
			return false;
	}

	_write_set.append(wrapperObj);
	return true;
}

bool TxWrappersRack::readSetAddObject(TxWrapper* wrapperObj) {

	TxWrapper** w_begin = _read_set.begin();
	TxWrapper** w_end = _read_set.end();

	bool found = false;
	for(; w_begin < w_end; w_begin++ ) {
		TxWrapper* w_ = *w_begin;

		if(w_ == wrapperObj)
			return false;
	}

	_read_set.append(wrapperObj);
	return true;
}






//
// Commit algo:
//
// (1) lock write set: acquire a lock for each field in the write set (only per-field locking)
//		--> abort whenever a single lock cannot be acquired
//		--> else, we now have exclusive access to the write set
//
// (2) validate read set
//
// (3) get new unique Tx global version
//
// (4) write back updates to global objects (acquire a per-object lock just before modifying it)
//		--> while committing, set the new write version for all the committed fields
//		--> also set the version of the object: JS_ATOMIC_INCREMENT(&globalClock)
//
// (5) unlock all the locks acquired and return true :)
//
// abort: unlock all the locks acquired and return false :(
//

int TxWrappersRack::TxCommit(JSContext* cx, int64_t &globalClock) {

	int _tx_ok = TX_COMMIT_OK;
	int64_t WV = -1;
	int64_t RV = _RV;
	JSAutoRequest ar(cx);

	TX("Starting tx commit: write set size: %d\n", _write_set.length());

	// (1) lock write set: acquire a lock for each field in the write set (only per-field locking)
    for (unsigned i = 0; i < _write_set.length(); i++) {

    	// TODO: use RootedObject
    	JSObject* _local_clone = _write_set[i]->getLocalClone();
    	JSObject* _actual_obj = _write_set[i]->getActualObj();

		for(PropIdHSet::Range r = _local_clone->getTxMetadata()->getPropWriteSet()->all(); !r.empty(); r.popFront()) {

			jsid lid =  r.front();
			jsid gid = TxGetMembrane()->getUnwrappedId(cx, lid);

			PropLock* _pl = PropLockCache::getPropLock(cx, gid, lid, _actual_obj, _local_clone);

#ifdef TXLOGVERBOSE
			if(JSID_IS_ATOM(lid))
			{
				JSString *str = JSID_TO_FLAT_STRING(lid);
				TX("Locking field:%s\n", JS_EncodeString(cx, str));
			}
			else if(JSID_IS_INT(lid)) {
				TX("Locking field: %d\n", lid);
			}
			else {
				TX("Locking field (unknown)\n");
			}
#endif
			// abort whenever a single lock cannot be acquired
			if( ! _pl->TryWriteLock() ) {
				TXABORT(TX_COMM_ABORT_LOCK_WRITE_SET);
			}
			else {
				_lock_set.append(_pl);
			}
		}
    }
    TX("Write-set locked. Locks: %d\n", _lock_set.length());


    // (2) validate read set
	TX("Validating read set (size: %d)\n", _read_set.length());

    for (unsigned i = 0; i < _read_set.length(); i++) {

    	// TODO: use RootedObject
    	JSObject* _local_clone = _read_set[i]->getLocalClone();
    	JSObject* _actual_obj = _read_set[i]->getActualObj();

    	for(PropIdHSet::Range r = _local_clone->getTxMetadata()->getPropReadSet()->all(); !r.empty(); r.popFront()) {

    		jsid lid = r.front();
			jsid gid = TxGetMembrane()->getUnwrappedId(cx, lid);

			int64_t globalV = PropLockCache::getGlobalPropVersionUnsafe(cx, gid, lid, _actual_obj, _local_clone);

			// abort if the read-set changed
			if( globalV > RV )
				TXABORT(TX_COMM_ABORT_VALIDATING_READ_SET_VERSION);
		}
    }
	TX("read set is valid: now committing\n");


	// Ok, now we have exclusive access to the write set,
	// (3) get new unique Tx global version
    WV = JS_ATOMIC_INCREMENT(&globalClock);
    TX("New WV version: %d\n", WV);


	// (4) write back updates to global objects (acquire a per-object lock just before modifying it)
	//		--> while committing, set the new write version for all the committed fields
	//		--> also set the version of the object
    for (unsigned i = 0; i < _write_set.length(); i++) {

    	RootedObject _local_clone(cx, _write_set[i]->getLocalClone());
    	RootedObject _actual_obj(cx, _write_set[i]->getActualObj());

    	{
    		_actual_obj->getTxMetadata()->wrLockObj();

    		for(PropIdHSet::Range r = _local_clone->getTxMetadata()->getPropWriteSet()->all(); !r.empty(); r.popFront()) {

    			jsid lid;
    			jsid gid;
    			{
    				JSAutoCompartment ac(cx, _local_clone);
    				lid =  r.front();
    			}
    			{
    				JSAutoCompartment ac(cx, _actual_obj);
    				gid = TxGetMembrane()->getUnwrappedId(cx, lid);
    			}
    			jsval vv;
    			{
    				JSAutoCompartment ac(cx, _local_clone);
    				JS_GetPropertyById(cx, _local_clone, lid, &vv);
    			}
    			{
    				JS_ASSERT( ! _actual_obj->isProxy());
    				JSAutoCompartment ac(cx, _actual_obj);

    				if(JSVAL_IS_NUMBER(vv)) {

    					JSBool has;
//    					if(!JS_HasPropertyById(cx, _actual_obj, gid, &has))
//    						TXABORT(TX_ABORT_JS_ERROR)

    					if(!has) {
//        					_actual_obj->getTxMetadata()->rwUnlockObj();
//    						_actual_obj->getTxMetadata()->wrLockObj();
#ifdef TXLOGVERBOSE
//    						TX(stderr, "Acquiring wl to add a new property\n");
#endif
    					}

    					if (!JS_SetPropertyById(cx, _actual_obj, gid, &vv))
    						TXABORT(TX_ABORT_JS_ERROR)

    					if(!has) {
//    						_actual_obj->getTxMetadata()->rwUnlockObj();
#ifdef TXLOGVERBOSE
//    						TX(stderr, "Releasing wl\n");
#endif
    					}
    				}
    				else {
    					fprintf(stderr, "[TQ] Warning: committing unsupported data type!\n");
//        				JSAutoCompartment ac(cx, _local_clone);
//
//    					jsval cloned;
//
//    					if(!JS_StructuredClone(cx, vv, &cloned, NULL, NULL))
//    						TXABORT(TX_ABORT_JS_ERROR)
//
//    	    				JSAutoCompartment acd(cx, _actual_obj);
//
//    					if (!JS_SetPropertyById(cx, _actual_obj, lid, &cloned))
//    						TXABORT(TX_ABORT_JS_ERROR)
    				}
    			}
    			PropLockCache::setPropVersion(cx, gid, _actual_obj, WV);
    			PropLockCache::setPropVersion(cx, lid, _local_clone, WV);
    		}

    		_actual_obj->getTxMetadata()->rwUnlockObj();
    	}
    }

abort:
	TX("Unlocking %d locks\n", _lock_set.length())
	// (5) unlock all the locks acquired and return
	for (unsigned i = 0; i < _lock_set.length(); i++) {

		_lock_set[i]->UnlockW();
		_lock_set[i]->NotifyWriteLockIsFree();
	}

	return _tx_ok;
}










int TxWrappersRack::TxEventualCommit(JSContext* cx) {

//	return TX_COMMIT_OK;
	int _tx_ok = TX_COMMIT_OK;
	JSAutoRequest ar(cx);

	TX("Starting eventual tx commit: write set size: %d\n", _ev_write_set.length());

	// (1) lock write set: acquire a lock for each field in the write set (only per-field locking)
    for (unsigned i = 0; i < _ev_write_set.length(); i++) {

    	// TODO: use RootedObject
    	JSObject* _local_clone = _ev_write_set[i]->getLocalClone();
    	JSObject* _local_clone_initial = _ev_write_set[i]->getLocalCloneInitial();
    	JSObject* _actual_obj = _ev_write_set[i]->getActualObj();

    	_actual_obj->getTxMetadata()->wrLockObj();

    	// Iterate over the properties of _local_clone_initial: the obj contains eventual props only
    	AutoIdVector props(cx);
    	if (!GetPropertyNames(cx, _local_clone_initial, JSITER_OWNONLY /*| JSITER_HIDDEN*/, &props))
    		TXABORT(TX_ABORT_JS_ERROR);

    	for (jsid *v = props.begin(), *v_end = props.end(); v < v_end; v++) {
    		jsid lid = *v;
			jsid gid = TxGetMembrane()->getUnwrappedId(cx, lid);

    		jsval finalVal;
			if (!JS_GetPropertyById(cx, _local_clone, lid, &finalVal))
				TXABORT(TX_ABORT_JS_ERROR);

    		jsval initialVal;
			if (!JS_GetPropertyById(cx, _local_clone_initial, lid, &initialVal))
				TXABORT(TX_ABORT_JS_ERROR);

    		jsval globalVal;
			{
	    		JSAutoCompartment ac(cx, _actual_obj);

	    		if (!JS_GetPropertyById(cx, _actual_obj, gid, &globalVal))
	    			TXABORT(TX_ABORT_JS_ERROR);
			}

			int argv = 3;
			jsval args[3];
			args[0] = globalVal;
			args[1] = initialVal;
			args[2] = finalVal;

#ifdef TXLOGVERBOSE
			if(JSID_IS_ATOM(lid))
			{
				JSString *str = JSID_TO_FLAT_STRING(lid);
				TX("Writing field:%s\n", JS_EncodeString(cx, str));
			}
			else if(JSID_IS_INT(lid)) {
				TX("Locking field: %d\n", lid);
			}
			else {
				TX("Locking field (unknown)\n");
			}
#endif

			jsval field;
			Value vf;
			JS_IdToValue(cx, gid, &field);

			JSFunction* evcb = _local_clone->getTxMetadata()->getEventualCallback(field);

			if(evcb == NULL) {
				evcb = _actual_obj->getTxMetadata()->getEventualCallback(field);
				vf = OBJECT_TO_JSVAL(evcb);
				_m->wrap(&vf);
				evcb = JS_ValueToFunction(cx, vf);
				_local_clone->getTxMetadata()->registerEventualCallback(field, evcb);
			}
			else {
				vf = OBJECT_TO_JSVAL(evcb);
			}
			JS_ASSERT(evcb);

			jsval res;
			{
				JSAutoCompartment ac(cx, _local_clone);
				if( ! JS_CallFunctionValue(cx, NULL, vf, argv, args, &res))
					TXABORT(TX_ABORT_JS_ERROR);
			}
			{
				JS_ASSERT( ! _actual_obj->isProxy());
				JSAutoCompartment ac(cx, _actual_obj);

				if(JSVAL_IS_NUMBER(res)) {
					if (!JS_SetPropertyById(cx, _actual_obj, gid, &res))
						TXABORT(TX_ABORT_JS_ERROR)
				}
				else {
					// TODO: unwrap strings and objects
					fprintf(stderr, "[TQ] Committing unsupported data type\n");
				}
			}
    	}

		_actual_obj->getTxMetadata()->rwUnlockObj();
    }

    TX("Eventual commit ok\n");
abort:
	return _tx_ok;
}





























/* Tx Wrapper */

#define TXMAKEABORT(n) \
{ \
	_rk->TxAbort(cx, n); \
	return false;	\
} \


TxWrapper::TxWrapper(unsigned flags, TxWrappersRack* rack, JSObject* actualObj, bool hasPrototype)
	: CrossCompartmentWrapper(flags), _rk(rack) {

	_actual_obj = actualObj;
	_local_clone = NULL;
	_local_clone_initial = NULL;

	_eventual_fields_checked = false;
	_all_eventual = false;
	_has_eventuals = false;
}

TxWrapper::~TxWrapper() {}




bool
TxWrapper::propIsEventual(jsid gid) {

	if(_all_eventual)
		return true;

	for(unsigned i=0; i<_eventual_props_cache.length(); i++)
		if(_eventual_props_cache[i] == gid)
			return true;

	bool hit = _actual_obj->getTxMetadata()->lookupEventualProp(gid);
	if(hit)
		_eventual_props_cache.append(gid);

	return hit;
}



bool
TxWrapper::directWrapperGet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, Value *vp) {

	AutoCompartment ac(cx, _actual_obj);

	bool r = true;
//	JSBool found;
//	JS_HasPropertyById(cx,_actual_obj,gid, &found);

//	if(found) {
	_actual_obj->getTxMetadata()->rdLockObj();

	r = Wrapper::get(cx, wrapper, receiver, gid, vp);

	_actual_obj->getTxMetadata()->rwUnlockObj();
//	}
	return r;
}




/*
 * TxWrapper::Get is the hook for TxFieldRead.
 * */
bool
TxWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid lid, Value *vp)
{
#ifdef USE_TX
#ifdef TXLOGVERBOSE
	JSObject* _g = GetProxyPrivate(receiver).toObjectOrNull();

	if(JSID_IS_ATOM(lid)) {

		JSString *str = JSID_TO_FLAT_STRING(lid);

		if((*vp != JSVAL_NULL)&&(*vp != JSVAL_VOID)) {
			if(JSVAL_IS_NUMBER(*vp)) {
				double dob = 0;
				JS_ValueToNumber(cx, *vp, &dob);
				fprintf(stderr, "[%s] Global obj:%p, GET PROP: '%s' (value :%f)\n", _getThName(PR_GetCurrentThread()), _g, JS_EncodeString(cx, str), dob);
			}
			else {
				fprintf(stderr, "[%s] Global obj:%p, GET PROP: '%s'\n", _getThName(PR_GetCurrentThread()), _g, JS_EncodeString(cx, str));
			}
		}
		else {
			fprintf(stderr, "[%s] Global obj:%p, GET PROP: '%s'\n", _getThName(PR_GetCurrentThread()), _g, JS_EncodeString(cx, str));
		}
	}
	else if(JSID_IS_INT(lid)) {
		fprintf(stderr, "[%s] Global obj:%p, GET PROP: '%d'\n", _getThName(PR_GetCurrentThread()), _g, JSID_TO_INT(lid));
	}
	else {
		fprintf(stderr, "[%s] Global obj:%p, GET PROP: (unknown)\n", _getThName(PR_GetCurrentThread()), _g);
	}
#endif

	if( ! _eventual_fields_checked) {
		_all_eventual = _actual_obj->getTxMetadata()->allPropsEventual();
		_has_eventuals = _actual_obj->getTxMetadata()->hasEventualProps();
		_eventual_fields_checked = true;
	}


	jsid gid = _rk->TxGetMembrane()->getUnwrappedId(cx, lid);

	if(_has_eventuals) {
		// If the field is eventual (or all fields are) just read the local log
		// and return with the value. Else, continue with the regular TxGet
		if(propIsEventual(gid))
			return evGet(cx, wrapper, receiver, gid, lid, vp);
	}

	if(_rk->_RV == -1)
		_rk->_RV = *(_rk->_globalVersion);

	return txGet(cx, wrapper, receiver, gid, lid, vp);

#else
	return DirectWrapper::get(cx, wrapper, receiver, id, vp);
#endif
}




/*
 * TxWrapper::Set is the hook for TxFieldWrite.
 * */
bool
TxWrapper::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid lid, bool strict, Value *vp)
{
#ifdef USE_TX
#ifdef TXLOGVERBOSE
	JSObject* _g = GetProxyPrivate(receiver).toObjectOrNull();

	if(JSID_IS_ATOM(lid)) {

		JSString *str = JSID_TO_FLAT_STRING(lid);

		if((*vp != JSVAL_NULL)&&(*vp != JSVAL_VOID)) {
			if(JSVAL_IS_NUMBER(*vp)) {
				double dob = 0;
				JS_ValueToNumber(cx, *vp, &dob);
				fprintf(stderr, "[%s] Global obj:%p, SET PROP: '%s' (value :%f)\n", _getThName(PR_GetCurrentThread()), _g, JS_EncodeString(cx, str), dob);
			}
			else {
				fprintf(stderr, "[%s] Global obj:%p, SET PROP: '%s'\n", _getThName(PR_GetCurrentThread()), _g, JS_EncodeString(cx, str));
			}
		}
		else {
			fprintf(stderr, "[%s] Global obj:%p, SET PROP: '%s'\n", _getThName(PR_GetCurrentThread()), _g, JS_EncodeString(cx, str));
		}
	}
	else if(JSID_IS_INT(lid)) {
		fprintf(stderr, "[%s] Global obj:%p, SET PROP: '%d'\n", _getThName(PR_GetCurrentThread()), _g, JSID_TO_INT(lid));
	}
	else {
		fprintf(stderr, "[%s] Global obj:%p, SET PROP: (unknown)\n", _getThName(PR_GetCurrentThread()), _g);
	}
#endif

	if( ! _eventual_fields_checked) {
		_all_eventual = _actual_obj->getTxMetadata()->allPropsEventual();
		_has_eventuals = _actual_obj->getTxMetadata()->hasEventualProps();
		_eventual_fields_checked = true;
	}

	jsid gid = _rk->TxGetMembrane()->getUnwrappedId(cx, lid);

	if(_has_eventuals) {
		// If the field is eventual (or all fields are) just read the local log
		// and return with the value. Else, continue with the regular TxGet
		if(propIsEventual(gid))
			return evSet(cx, wrapper, receiver, gid, lid, strict, vp);
	}

	return txSet(cx, wrapper, receiver, gid, lid, strict, vp);

#else
	return DirectWrapper::set(cx, wrapper, receiver, id, strict, vp);
#endif
}





/* Tx Wrappers */

bool
TxWrapper::txGet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, Value *vp) {

	int64_t RV = _rk->_RV;
	int64_t globalV1 = PropLockCache::getGlobalPropVersion(cx, gid, lid, _actual_obj, _local_clone);

	// If we have a local clone for the object value, we use it
	if(_local_clone != NULL) {

		// If the local clone has the property in the redo_log the prop has already been
		// modified, so we can safely read it from the local object
		if(_local_clone->getTxMetadata()->redoLogLookupProp(lid)) {

			if(!JS_GetPropertyById(cx, _local_clone, lid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);

			PropLockCache::setPropVersion(cx, lid, _local_clone, 0);

			// Just return, if this is a write-only tx we do not have to
			// check the version. Else, if the field has previously been
			// read it will be re-validated at commit-time (potentially
			// failing).
			//
			// XXX An optimization would check the version if the field
			// is also in the read set.
			return true;
		}
		// Else, the prop is not in the redo_log.
		// We look for it in the clone and in the global_obj,
		// and we add it to the read_set (read through
		// proxy, check version using global_obj)
		else {
			bool ret = true;

			if(!JS_GetPropertyById(cx, _local_clone, lid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);

			// No property on the local object: check the global obj for the property and set it
			if((*vp == JSVAL_NULL)||(*vp == JSVAL_VOID)) {

				if(!directWrapperGet(cx, wrapper, receiver, gid, vp))
					TXMAKEABORT(TX_ABORT_JS_ERROR);

				if(!vp->isNumber()) {
					// XXX is there any fundamental difference between wrapping obj and fun here?
					_rk->TxGetMembrane()->wrap(vp);
				}

				if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
					TXMAKEABORT(TX_ABORT_JS_ERROR);

				PropLockCache::setPropVersion(cx, lid, _local_clone, globalV1);

				if(_local_clone->getTxMetadata()->readSetAddProp(lid))
					TX("  +++ added new entry to the read set (property)\n")

				if(_rk->readSetAddObject(this))
					TX("  +++ added new entry to the read set (local clone)\n")
			}
			// Else the local_clone has the property: check its version
			else {
				int64_t cloneV = PropLockCache::getPropVersion(cx, lid, _local_clone);

				// if the prop version is different, we have to update/reset the local object
				if(cloneV != globalV1) {

					if(!directWrapperGet(cx, wrapper, receiver, gid, vp))
						TXMAKEABORT(TX_ABORT_JS_ERROR);

					if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
						TXMAKEABORT(TX_ABORT_JS_ERROR);

					PropLockCache::setPropVersion(cx, lid, _local_clone, globalV1);

				}
				if(_local_clone->getTxMetadata()->readSetAddProp(lid))
					TX("  +++ added new entry to the read set (property)\n");

				if(_rk->readSetAddObject(this))
					TX("  +++ added new entry to the read set (local clone)\n");
			}

			int64_t globalV2 = PropLockCache::getGlobalPropVersion(cx, gid, lid, _actual_obj, _local_clone);
			bool isPropLocked = PropLockCache::isPropLockHeld(cx, gid, lid, _actual_obj, _local_clone);

			// Something changed: we have to abort the tx: another thread has already modified this prop
			if((globalV1 != globalV2) || (globalV1 > RV) || (isPropLocked)) {

				if(isPropLocked) {
					TXMAKEABORT(TX_RT_ABORT_VALIDATING_READ_SET_LOCK);
				}
				else {
					TXMAKEABORT(TX_RT_ABORT_VALIDATING_READ_SET_VERSION);
				}
			}

			return ret;
		}
	}
	// No local clone: we have to create it
	else {

		if(!directWrapperGet(cx, wrapper, receiver, gid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);

		int64_t globalV2 = PropLockCache::getGlobalPropVersion(cx, gid, lid, _actual_obj, _local_clone);
		bool isPropLocked = PropLockCache::isPropLockHeld(cx, gid, lid, _actual_obj, _local_clone);

		if((globalV1 == globalV2) && (globalV1 <= RV) && (!isPropLocked)) {

			// Create the new clone object
			_local_clone = JS_NewObject(cx, NULL, NULL, NULL);

			if(!vp->isNumber()) {
				_rk->TxGetMembrane()->wrap(vp);
			}

			// Save the local prop value
			if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);

			PropLockCache::setPropVersion(cx, lid, _local_clone, globalV1);

			if(_local_clone->getTxMetadata()->readSetAddProp(lid))
				TX("  +++ added new entry to the read set (property)\n")

			if(_rk->readSetAddObject(this))
				TX("  +++ added new entry to the read set (local clone)\n")

			return true;
		}
		else {
			// else we have to abort the tx: another thread has already modified this prop
			if(isPropLocked) {
				TXMAKEABORT(TX_RT_ABORT_VALIDATING_READ_SET_LOCK);
			}
			else {
				TXMAKEABORT(TX_RT_ABORT_VALIDATING_READ_SET_VERSION);
			}
		}
	}
}





bool
TxWrapper::txSet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, bool strict, Value *vp) {

	// If we have a local clone for the object value, we use it
	if(_local_clone != NULL) {

		if( ! _local_clone->getTxMetadata()->redoLogLookupOrAddProp(lid)) {

			TX("  +++ added new entry to the write set (property)\n")

			if(_rk->redoLogAddObject(this))
				TX("  +++ added new entry to the write set (local clone)\n")
		}

		if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);
	}
	// No local clone: we have to create it
	else {

		// Create a new object
		_local_clone = JS_NewObject(cx, NULL, NULL, NULL);

		if(_local_clone->getTxMetadata()->redoLogAddProp(lid))
			TX("  +++ added new entry to the write set (property)\n")

		if(_rk->redoLogAddObject(this))
			TX("  +++ added new entry to the write set (local clone)\n")

		// Save the local prop value
		if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);
	}

	return true;
}





//bool
//TxWrapper::txSet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, bool strict, Value *vp) {
//
//	// If we have a local clone for the object value, we use it
//	if(_local_clone != NULL) {
//
//		if( ! _local_clone->getTxMetadata()->redoLogLookupProp(lid)) {
//
//			if(_local_clone->getTxMetadata()->redoLogAddProp(lid))
//				TX("  +++ added new entry to the write set (property)\n")
//
//			if(_rk->redoLogAddObject(this))
//				TX("  +++ added new entry to the write set (local clone)\n")
//		}
//
//		if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
//			TXMAKEABORT(TX_ABORT_JS_ERROR);
//	}
//	// No local clone: we have to create it
//	else {
//
//		// Create a new object
//		_local_clone = JS_NewObject(cx, NULL, NULL, NULL);
//
//		if(_local_clone->getTxMetadata()->redoLogAddProp(lid))
//			TX("  +++ added new entry to the write set (property)\n")
//
//		if(_rk->redoLogAddObject(this))
//			TX("  +++ added new entry to the write set (local clone)\n")
//
//		// Save the local prop value
//		if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
//			TXMAKEABORT(TX_ABORT_JS_ERROR);
//	}
//
//	return true;
//}







/* Eventual Tx Wrappers */

bool
TxWrapper::evGet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, Value *vp) {

	// Check and create local clones
	if((_local_clone_initial == NULL)||(_local_clone == NULL)) {

		if(!directWrapperGet(cx, wrapper, receiver, gid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);

		if(_local_clone_initial == NULL) {

			// Create the new clone object
			_local_clone_initial = JS_NewObject(cx, NULL, NULL, NULL);

			if(!vp->isNumber()) {
				_rk->TxGetMembrane()->wrap(vp);
			}

			if(!JS_SetPropertyById(cx, _local_clone_initial, lid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);
		}

		if(_local_clone == NULL) {

			// Create the new clone object
			_local_clone = JS_NewObject(cx, NULL, NULL, NULL);

			if(!vp->isNumber()) {
				_rk->TxGetMembrane()->wrap(vp);
			}

			if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);
		}

		return true;
	}

	// if we are here the local clone exists and has already read/written the prop once
	if(!JS_GetPropertyById(cx, _local_clone, lid, vp))
		TXMAKEABORT(TX_ABORT_JS_ERROR);


	if((*vp == JSVAL_NULL)||(*vp == JSVAL_VOID)) {
		if(!directWrapperGet(cx, wrapper, receiver, gid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);

		if(!vp->isNumber()) {
			_rk->TxGetMembrane()->wrap(vp);
		}

		if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);


		if(!JS_SetPropertyById(cx, _local_clone_initial, lid, vp))
			TXMAKEABORT(TX_ABORT_JS_ERROR);
	}

	return true;
}


bool
TxWrapper::evSet(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid gid, jsid lid, bool strict, Value *vp) {

	bool wrapped = false;

	// Check and create local clones
	if((_local_clone_initial == NULL)||(_local_clone == NULL)) {

		if(_local_clone_initial == NULL) {

			if(!directWrapperGet(cx, wrapper, receiver, gid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);

			// Create the new clone object
			_local_clone_initial = JS_NewObject(cx, NULL, NULL, NULL);

			if(!vp->isNumber()) {
				wrapped = true;
				_rk->TxGetMembrane()->wrap(vp);
			}

			if(!JS_SetPropertyById(cx, _local_clone_initial, lid, vp))
				TXMAKEABORT(TX_ABORT_JS_ERROR);
		}

		if(_local_clone == NULL) {

			// Create the new clone object
			_local_clone = JS_NewObject(cx, NULL, NULL, NULL);
		}
	}

	if(!wrapped)
		if(!vp->isNumber()) {
			_rk->TxGetMembrane()->wrap(vp);
		}

	if(!JS_SetPropertyById(cx, _local_clone, lid, vp))
		TXMAKEABORT(TX_ABORT_JS_ERROR);

	_rk->eventualLogAddObject(this);

	return true;
}







/* Debug wrappers */

bool
TxWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set, PropertyDescriptor *desc) {

	fprintf(stderr, "--WR-- getPropertyDescriptor\n");
	return Wrapper::getPropertyDescriptor(cx, wrapper, id, set, desc);
}


bool
TxWrapper::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, bool set, PropertyDescriptor *desc) {

	fprintf(stderr, "--WR-- getOwnPropertyDescriptor\n");
	return Wrapper::getOwnPropertyDescriptor(cx, wrapper, id, set, desc);
}

bool
TxWrapper::defineProperty(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc) {

	fprintf(stderr, "--WR-- defineProperty\n");
	return Wrapper::defineProperty(cx, wrapper, id, desc);
}


bool
TxWrapper::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, AutoIdVector &props) {

	fprintf(stderr, "--WR-- getOwnPropertyNames\n");
	return Wrapper::getOwnPropertyNames(cx, wrapper, props);
}


bool
TxWrapper::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp) {

	fprintf(stderr, "--WR-- delete_\n");
	return Wrapper::delete_(cx, wrapper, id, bp);
}


bool
TxWrapper::enumerate(JSContext *cx, JSObject *wrapper, AutoIdVector &props) {

	fprintf(stderr, "--WR-- enumerate\n");
	return Wrapper::enumerate(cx, wrapper, props);
}


bool
TxWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp) {

	fprintf(stderr, "--WR-- has\n");
	return Wrapper::has(cx, wrapper, id, bp);
}

bool
TxWrapper::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp) {

	fprintf(stderr, "--WR-- hasOwn\n");
	return Wrapper::hasOwn(cx, wrapper, id, bp);
}





bool
TxWrapper::keys(JSContext *cx, JSObject *wrapper, AutoIdVector &props) {
	fprintf(stderr, "--WR-- keys\n");
	return Wrapper::keys(cx, wrapper, props);
}

bool
TxWrapper::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, Value *vp) {
	fprintf(stderr, "--WR-- iterate\n");

	AutoCompartment ac(cx, _actual_obj);
	return Wrapper::iterate(cx, wrapper, flags, vp);
}

bool
TxWrapper::call(JSContext *cx, JSObject *wrapper, unsigned argc, Value *vp) {
	fprintf(stderr, "--WR-- call\n");
	return Wrapper::call(cx, wrapper, argc, vp);
}

bool
TxWrapper::construct(JSContext *cx, JSObject *wrapper, unsigned argc, Value *argv, Value *rval) {
	fprintf(stderr, "--WR-- construct\n");
	return Wrapper::construct(cx, wrapper, argc, argv, rval);
}

bool
TxWrapper::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl, CallArgs args) {
	fprintf(stderr, "--WR-- nativeCall\n");
	return Wrapper::nativeCall(cx, test, impl, args);
}

bool
TxWrapper::hasInstance(JSContext *cx, HandleObject wrapper, MutableHandleValue v, bool *bp) {
	fprintf(stderr, "--WR-- hasInstance\n");
	return Wrapper::hasInstance(cx, wrapper, v, bp);
}

JSString*
TxWrapper::obj_toString(JSContext *cx, JSObject *wrapper) {
	fprintf(stderr, "--WR-- obj_toString\n");
	return Wrapper::obj_toString(cx, wrapper);
}

JSString*
TxWrapper::fun_toString(JSContext *cx, JSObject *wrapper, unsigned indent) {
	fprintf(stderr, "--WR-- fun_toString\n");
	return Wrapper::fun_toString(cx, wrapper, indent);
}

bool
TxWrapper::regexp_toShared(JSContext *cx, JSObject *proxy, RegExpGuard *g) {
	fprintf(stderr, "--WR-- regexp_toShared\n");
	return Wrapper::regexp_toShared(cx, proxy, g);
}

bool
TxWrapper::defaultValue(JSContext *cx, JSObject *wrapper, JSType hint, Value *vp) {
	fprintf(stderr, "--WR-- defaultValue\n");
	return Wrapper::defaultValue(cx, wrapper, hint, vp);
}

//bool
//TxWrapper::iteratorNext(JSContext *cx, JSObject *wrapper, Value *vp) {
//	fprintf(stderr, "--WR-- iteratorNext\n");
//	return Wrapper::iteratorNext(cx, wrapper, vp);
//}

bool
TxWrapper::getPrototypeOf(JSContext *cx, JSObject *proxy, JSObject **protop) {
	fprintf(stderr, "--WR-- getPrototypeOf\n");
	return Wrapper::getPrototypeOf(cx, proxy, protop);
}























}
}





