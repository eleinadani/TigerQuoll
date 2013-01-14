/*
 * tqtxversionedlock.h
 *
 *  Created on: Nov 20, 2012
 *      Author: usiusi
 */

#ifndef TQTXVERSIONEDLOCK_H_
#define TQTXVERSIONEDLOCK_H_


#include "prlock.h"
#include "pratom.h"
#include "pthread.h"
#include "prthread.h"

#include "jsapi.h"
#include <HashTable.h>
#include <deque>

//#define TXLOGLOCK
//#define NOTIFYMJIT


#ifdef TXLOGLOCK

#define TXLK(...) \
{ \
		char* __s;	\
		asprintf(&__s, __VA_ARGS__);	\
		fprintf(stderr, "[%s] %s", PR_GetThreadName(PR_GetCurrentThread()), __s );  \
} \

#else
#define TXLK(...) {};
#endif




# define JS_ATOMIC_INCREMENT(p)      PR_ATOMIC_INCREMENT((int32_t *)(p))
# define JS_ATOMIC_DECREMENT(p)      PR_ATOMIC_DECREMENT((int32_t *)(p))


namespace js {

template<>
struct DefaultHasher<jsid>
{
    typedef jsid Lookup;
    static HashNumber hash(const Lookup &l) {
        return HashNumber(JSID_BITS(l));
    }
    static bool match(const jsid &id, const Lookup &l) {
        return id == l;
    }
};
}



namespace tq {
namespace tx {

using namespace js;
using namespace std;

struct PropsHasher
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


struct PropLock {

private:
	pthread_rwlock_t _prop_rw_lock;

	int32_t _w;
	int32_t _r;
	int32_t _w_l;

public:
	int64_t _prop_version;

	PropLock() {
		_w = 0; _r = 0; _w_l = 0;
		_prop_version = 0;
		pthread_rwlock_init(&_prop_rw_lock, NULL);
	}

	void LockR() {
		JS_ATOMIC_INCREMENT(&_r);
		TXLK("( ? ) ReadLock\n");
		pthread_rwlock_rdlock(&_prop_rw_lock);
		TXLK("(+++) ReadLock\n");
	}
	void LockW() {
		JS_ATOMIC_INCREMENT(&_w);
		TXLK("( ? ) WriteLock\n");
		pthread_rwlock_wrlock(&_prop_rw_lock);
		TXLK("(+++) WriteLock\n");
	}
	void UnlockR() {
		JS_ATOMIC_DECREMENT(&_r);
		pthread_rwlock_unlock(&_prop_rw_lock);
		TXLK("(---) ReadLock\n");
	}
	void UnlockW() {
		JS_ATOMIC_DECREMENT(&_w);
		pthread_rwlock_unlock(&_prop_rw_lock);
		TXLK("(---) WriteLock\n");
	}

	bool IsWriteLocked() {
		return (_w_l == 1);
	}


	bool TryWriteLock() {

		if(PR_AtomicSet(&_w_l, 1) != 0) {
			return false;
		}

		LockW();
		return true;
	}

	void NotifyWriteLockIsFree() {

		JS_ASSERT(PR_AtomicSet(&_w_l, 0) == 1);
	}


};




typedef HashMap<Value, JSFunction*, PropsHasher, SystemAllocPolicy> EventualPropsMap;
//typedef HashMap<Value, PropLock*, PropsHasher, SystemAllocPolicy> PropsMap;
typedef HashMap<jsid, PropLock*, DefaultHasher<jsid>, SystemAllocPolicy> PropsMap;


typedef Vector<jsid, 0, SystemAllocPolicy> PropIdSet;
typedef HashSet<jsid, DefaultHasher<jsid>, SystemAllocPolicy> PropIdHSet;





struct VersionedLock {

private:

	/*
	 * Slow map with the version of each field.
	 * The map is lazy: unchanged fields will have version 0.
	 * The map is slow: it is accessed claiming a per-object mutex and is shared
	 * between multiple threads.
	 * */
	PropsMap _prop_versions;

	/*
	 * Fast thread-local cache.
	 * To be used by local_clones to acquire per-field locks
	 * */
	PropsMap _prop_versions_cache;

	/*
	 * Redo log and read set
	 * */
	PropIdHSet _prop_redo_log;
	PropIdHSet _prop_read_set;

	/*
	 * Set with eventual fields. jsid not in this table are regular (non-eventual) fields.
	 * */
	PropIdSet _eventual_props;
	EventualPropsMap _eventual_props_callbacks;


	/*
	 * Flag for marking the entire object eventual
	 * */
	bool _all_eventual;
	bool _has_eventuals;
	JSFunction* _all_eventual_cb;

	/*
	 * Object-wide write lock.
	 */
	// TODO: two locks, one commit-time only?
	pthread_mutex_t _obj_lock;
	pthread_mutex_t _prop_ver_lock;
	pthread_mutex_t _eventual_prop_lock;
	pthread_mutex_t _eventual_cb_lock;
	pthread_rwlock_t _obj_rw_lock;

	/*
	 * Mjit integration
	 * */
	bool _skip_mjit;
	bool _skip_mjit_notified;

public:

	VersionedLock() {
		_all_eventual = false;
		_has_eventuals = false;
		_prop_versions.init();
		_prop_versions_cache.init();

		_prop_redo_log.init();
		_prop_read_set.init();

		_eventual_props.clear();
		_eventual_props_callbacks.init();
		_skip_mjit = false;
		_skip_mjit_notified = false;
		_all_eventual_cb = NULL;

		pthread_mutex_init(&_obj_lock, NULL);
		pthread_mutex_init(&_prop_ver_lock, NULL);
		pthread_mutex_init(&_eventual_prop_lock, NULL);
		pthread_mutex_init(&_eventual_cb_lock, NULL);
		pthread_rwlock_init(&_obj_rw_lock, NULL);
	}

	void setSkipMjit(bool val) {
		_skip_mjit = val;
	}

	bool skipMjit() {
#ifdef NOTIFYMJIT
		if(_skip_mjit) {
			if(!_skip_mjit_notified) {
				_skip_mjit_notified = true;
				printf("Warning: disabled mjit for this function\n");
			}
		}
#endif
		return _skip_mjit;
	}


	void setAllEventual() {
		_all_eventual = true;
	}

	void setHasEventuals() {
		_has_eventuals = true;
	}

	bool hasEventualProps() {
		return _has_eventuals;
	}

	bool allPropsEventual() {
		return _all_eventual;
	}

	bool lookupEventualProp(jsid id) {

		for(unsigned i=0; i<_eventual_props.length(); i++)
			if(_eventual_props[i] == id)
				return true;

		return false;
	}

	bool addEventualProp(jsid id) {

		for(unsigned i=0; i<_eventual_props.length(); i++)
			if(_eventual_props[i] == id)
				return false;
		_eventual_props.append(id);
		return true;
	}

	void registerEventualCallback(jsval field, JSFunction* cb) {

		_eventual_props_callbacks.put(field, cb);
	}

	void registerEventualCallback(JSFunction* cb) {

		JS_ASSERT(_all_eventual);
		_all_eventual_cb = cb;
	}


	JSFunction* getEventualCallback(jsval field) {

		JSFunction* cb = NULL;

		if(!_all_eventual) {
			EventualPropsMap::Ptr p;

			JS_ASSERT( 0==pthread_mutex_lock(&_eventual_cb_lock));

			p = _eventual_props_callbacks.lookup(field);

			pthread_mutex_unlock(&_eventual_cb_lock);

			if (p.found()) {
				cb = p->value;
			}
		}
		else {
			cb = _all_eventual_cb;
		}

		return cb;
	}


	PropIdHSet* getPropWriteSet() {
		return &_prop_redo_log;
	}

	PropIdHSet* getPropReadSet() {
		return &_prop_read_set;
	}


	bool redoLogLookupOrAddProp(jsid id) {

		PropIdHSet::AddPtr p = _prop_redo_log.lookupForAdd(id);

		if(!p) {
			_prop_redo_log.add(p, id);
			return false;
		}

		return true;
	}


	bool redoLogLookupProp(jsid id) {

		if( PropIdHSet::Ptr p = _prop_redo_log.lookup(id) )
			return true;

		return false;
	}


	bool redoLogAddProp(jsid id) {

		_prop_redo_log.put(id);
		return true;
	}


	bool readSetAddProp(jsid id) {

		_prop_read_set.put(id);
		return true;
	}


	/*
	 * Get the PropLock for the given property
	 * */
	PropLock* getPropLock(JSContext* cx, jsid id) {

		PropsMap::AddPtr p;

		lockPropVersions();

		p = _prop_versions.lookupForAdd(id);

		if (!p) {
			PropLock* lk = new PropLock();
			_prop_versions.add(p, id, lk);
		}
		unlockPropVersions();

		return p->value;
	}

	/*
	 * Lookup in the thread-local cache
	 * */
	PropLock* propLockCacheLookup(JSContext* cx, jsid id) {

		// XXX we do not lock here: this map is for thread-local usage only!!!
		if (PropsMap::Ptr p = _prop_versions_cache.lookup(id))
			return p->value;

		return NULL;
	}

	/*
	 * Add a new PropLock to the local cache
	 * */
	void propLockCacheAdd(JSContext* cx, jsid id, PropLock* lk) {

		_prop_versions_cache.put(id, lk);
	}


    /*
     * Per-object Locking
     * */

	void wrLockObj() {
		pthread_rwlock_wrlock(&_obj_rw_lock);
	}

	void rdLockObj() {
		pthread_rwlock_rdlock(&_obj_rw_lock);
	}

	void rwUnlockObj() {
		pthread_rwlock_unlock(&_obj_rw_lock);
	}

    void lockObj() {
    	TXLK("( ? ) ObjLock\n");
    	if( 0!= pthread_mutex_lock(&_obj_lock))
    		printf("ErrorLk\n");
    	TXLK("( + ) ObjLock\n");
    }

    void unlockObj() {
    	if( 0 != pthread_mutex_unlock(&_obj_lock))
    		printf("ErrorUnLk\n");
    	TXLK("( - ) ObjLock\n");
    }

    void lockPropVersions() {
    	if( 0!= pthread_mutex_lock(&_prop_ver_lock))
    		printf("ErrorLk\n");
    }

    void unlockPropVersions() {
    	if( 0 != pthread_mutex_unlock(&_prop_ver_lock))
    		printf("ErrorUnLk\n");
    }


    void ClearMetadata() {
    	_prop_redo_log.clear();
    	_prop_read_set.clear();
    }
};







}
}

#endif /* TQTXVERSIONEDLOCK_H_ */
