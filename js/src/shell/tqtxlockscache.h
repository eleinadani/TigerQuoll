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
