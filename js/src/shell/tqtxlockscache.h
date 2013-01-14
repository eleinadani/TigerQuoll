/*
 * tqtxlockscache.h
 *
 *  Created on: Nov 23, 2012
 *      Author: usiusi
 */

#ifndef TQTXLOCKSCACHE_H_
#define TQTXLOCKSCACHE_H_

#include "jsapi.h"

#include "tqtxversionedlock.h"

namespace tq {
namespace tx {

using namespace js;

/*
 * Fast thread-local lookup of per-property locks
 * */
struct PropLockCache {

	static
	PropLock* getPropLock(JSContext* cx, jsid id, JSObject* obj) {

		JS_ASSERT(obj);
		return obj->getTxMetadata()->getPropLock(cx, id);
	}

	static
	PropLock* getPropLock(JSContext* cx, jsid gid, jsid lid, JSObject* globalObj, JSObject* localClone) {

		JS_ASSERT(localClone);
		JS_ASSERT(globalObj);

		PropLock* ret = localClone->getTxMetadata()->propLockCacheLookup(cx, lid);

		if( ret != NULL )
			return ret;

		ret = globalObj->getTxMetadata()->getPropLock(cx, gid);
		localClone->getTxMetadata()->propLockCacheAdd(cx, lid, ret);

		return ret;
	}

	static
	int64_t getGlobalPropVersion(JSContext* cx, jsid gid, jsid lid, JSObject* globalObj, JSObject* localClone) {

		int64_t globalVer = -1;
		PropLock* pLock = NULL;

		if(localClone != NULL)
			pLock = getPropLock(cx, gid, lid, globalObj, localClone);
		else
			pLock = getPropLock(cx, gid, globalObj);

		JS_ASSERT(pLock);

		pLock->LockR();
		globalVer = pLock->_prop_version;
		pLock->UnlockR();

		return globalVer;
	}

	static
	int64_t getGlobalPropVersionUnsafe(JSContext* cx, jsid gid, jsid lid, JSObject* globalObj, JSObject* localClone) {

		int64_t globalVer = -1;
		PropLock* pLock = NULL;

		if(localClone != NULL)
			pLock = getPropLock(cx, gid, lid, globalObj, localClone);
		else
			pLock = getPropLock(cx, gid, globalObj);

		JS_ASSERT(pLock);

		globalVer = pLock->_prop_version;

		return globalVer;
	}

	static
	int64_t getPropVersion(JSContext* cx, jsid id, JSObject* obj) {

		int64_t ver = -1;
		PropLock* pLock = NULL;

		pLock = getPropLock(cx, id, obj);

		JS_ASSERT(pLock);

		pLock->LockR();
		ver = pLock->_prop_version;
		pLock->UnlockR();

		return ver;
	}

	static
	void setPropVersion(JSContext* cx, jsid id, JSObject* obj, int64_t version) {

		PropLock* pLock = getPropLock(cx, id, obj);

		JS_ASSERT(pLock);

		pLock->_prop_version = version;
	}

	static
	bool isPropLockHeld(JSContext* cx, jsid gid, jsid lid, JSObject* globalObj, JSObject* localClone) {

		JS_ASSERT(globalObj);

		if(localClone != NULL)
			return getPropLock(cx, gid, lid, globalObj, localClone)->IsWriteLocked();
		else
			return getPropLock(cx, gid, globalObj)->IsWriteLocked();
	}

};


}
}

#endif /* TQTXLOCKSCACHE_H_ */
