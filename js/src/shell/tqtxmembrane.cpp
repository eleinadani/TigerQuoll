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

#include "tqtxmembrane.h"
#include <vm/String.h>
#include <prmon.h>


#include "jsobj.h"
#include "jscompartment.h"
#include "jsinterp.h"
#include "jsatominlines.h"
#include "pjsutil.h"
#include "jstypedarray.h"
#include "jswrapper.h"
#include "jsobjinlines.h"

#include "tqtxutils.h"
#include "tqmainloop.h"


using namespace pjs;
using namespace JS;
using namespace js;
using namespace std;


namespace tq {
namespace tx {



char *Membrane::MEMBRANE = "Membrane";

static JSObject *wrappedObject(JSObject *obj) {
	return GetProxyPrivate(obj).toObjectOrNull();
}

Membrane *Membrane::create(JSContext *parentCx, JSObject *parentGlobal,
		JSContext* childCx, JSObject *childGlobal, JSNative *safeNatives,
		TxWrappersRack *proxyRack, MainLoop* mainLoop) {
	Membrane *m = new Membrane(parentCx, parentGlobal, childCx, childGlobal,
			safeNatives, proxyRack, mainLoop);
	if (!m->_map.init()) {
		delete m;
		return NULL;
	}

	return m;
}



Membrane::~Membrane() {
//	delete _rooter;
}

bool Membrane::IsCrossThreadWrapper(/*const*/ JSObject *wrapper) {
	return wrapper->isProxy() && GetProxyHandler(wrapper)->family() == (void*) MEMBRANE;

}

JSBool Membrane::put(Value key, Value value) {
	//TODO: restore rooter, or translate it into a JSArray.
    if (_rooter == NULL) {
        _rooter = ProxyRooter::create(_childCx, 128, NULL);
        if (!_rooter)
            return false;
    }
	if (!_map.put(key, value)) {
		return false;
	}
    if (!_rooter->add(value, &_rooter)) {
        _map.remove(key);
        return false;
    }
	return true;
}

bool Membrane::isSafeNative(JSNative n) {
	for (JSNative *p = _safeNatives; *p; p++)
		if (*p == n)
			return true;
	return false;
}





JSObject* Membrane::copyAndWrapEnvironment(JSContext *cx, JSFunction *cloned_fn, JSObject* orig_env) {

	RootedScript script(cx);
	cloned_fn->maybeGetOrCreateScript(cx, &script);

	if (!script)
		return NULL;

	RootedObject enclosing(cx, cx->global());
	RootedFunction callee(cx, cloned_fn);

	CallObject* new_co = CallObject::create(cx, script, enclosing, callee);

	JSAutoCompartment ac(cx, orig_env);

	JS::AutoIdVector props(cx);
	if (!GetPropertyNames(cx, orig_env, JSITER_OWNONLY /*| JSITER_HIDDEN*/, &props))
		return NULL;

	for (jsid *v = props.begin(), *v_end = props.end(); v < v_end; v++) {
		jsid lid = *v, nid = *v;

		jsval pval;
		{
			JSAutoCompartment ac(cx, orig_env);

			if (!JS_GetPropertyById(cx, orig_env, lid, &pval))
				return NULL;
		}

		{
			JSAutoCompartment ac(cx, new_co);

			if (!wrapId(&nid))
				return NULL;

			if(!wrap(&pval))
				return NULL;

			if (!JS_SetPropertyById(cx, new_co, nid, &pval))
				return NULL;
		}
	}

	return new_co;
}








bool Membrane::copyAndWrapProperties(JSContext* cx, JSFunction *orig_fn, JSObject* wrapped_fun) {

//	JSContext* ccx = _childCx;
//	JSContext* pcx = _parentCx;
//
//	JSAutoCompartment ac(pcx, orig_fn);
//
//	JS::AutoIdVector props(pcx);
//	if (!GetPropertyNames(pcx, orig_fn, JSITER_OWNONLY /*| JSITER_HIDDEN*/, &props))
//		fprintf(stderr, "ERROR!!!\n");
//
//	for (jsid *v = props.begin(), *v_end = props.end(); v < v_end; v++) {
//		jsid pid = *v, cid = *v;
//
//		fprintf(stderr, "---- WRAPPING ---\n");
//
//		jsval pval;
//		{
//			JSAutoCompartment ac(cx, orig_fn);
//
//			if (!JS_GetPropertyById(cx, orig_fn, pid, &pval))
//				fprintf(stderr, "ERROR!!!\n");
//		}
//
//		{
//			JSAutoCompartment ac(cx, wrapped_fun);
//
//
//								if(JSID_IS_ATOM(pid))
//								{
//									fprintf(stderr,"ID '");
//
//									JSAtom* a = JSID_TO_ATOM(pid);
//									const jschar* chars = a->getChars(pcx);
//									for (const jschar *p = chars; *p; p++)
//										        fprintf(stderr, "%c", char(*p));
//								    fprintf(stderr, "'\n");
//								}
//								else
//									fprintf(stderr,"ID unknown\n");
//
//
//			if (!wrapId(&pid))
//				return false;
//
//			if(!wrap(&pval))
//				fprintf(stderr, "ERROR!!!\n");
//
//			if (!JS_SetPropertyById(cx, wrapped_fun, cid, &pval))
//				fprintf(stderr, "ERROR!!!\n");
//		}
//	}




//	JSContext *ccx = _childCx;
//	JSContext* pcx = _parentCx;
//
//	JS::AutoIdVector props(pcx);
//
//	{
//		JSAutoCompartment ac(pcx, from);
//
//		if ( ! GetPropertyNames(pcx, from, JSITER_OWNONLY /*| JSITER_HIDDEN */, &props))
//			return false;
//	}
//
//
//	for (jsid *v = props.begin(), *v_end = props.end(); v < v_end; v++) {
//
//		jsid pid = *v, cid = *v;
//		jsval pval;
////		JSPropertyDescriptor desc;
//
//		{
//			JSAutoCompartment ac(pcx, from);
//
//			if (!JS_GetPropertyById(cx, from, pid, &pval))
//				return false;
//		}
//
//
//		//			if(JSID_IS_ATOM(propid))
//		//			{
//		//				fprintf(stderr,"ID '");
//		//
//		//				JSAtom* a = JSID_TO_ATOM(propid);
//		//				const jschar* chars = a->getChars(pcx);
//		//				for (const jschar *p = chars; *p; p++)
//		//					        fprintf(stderr, "%c", char(*p));
//		//			    fprintf(stderr, "'\n");
//		//			}
//		//			else
//		//				fprintf(stderr,"ID unknown\n");
//
//		{
//			JSAutoCompartment ac(cx, to);
//
//			//			if (!wrapId(&pid))
//			//				return false;
//
//			if (!wrap(&pval))
//				return false;
//
//			if (!JS_SetPropertyById(cx, to, cid, &pval))
//				return false;
//		}
//	}
//
	return true;
}



bool
Membrane::wrap(JSString **strp)
{
    RootedValue value(_childCx, StringValue(*strp));
    if (!wrap(value.address()))
        return false;
    *strp = value.get().toString();
    return true;
}

bool
Membrane::wrap(HeapPtrString *strp)
{
    RootedValue value(_childCx, StringValue(*strp));
    if (!wrap(value.address()))
        return false;
    *strp = value.get().toString();
    return true;
}



bool Membrane::wrap(JSObject **objp) {

    if (!*objp)
        return true;
    RootedValue value(_childCx, ObjectValue(**objp));
    if (!wrap(value.address()))
        return false;
    *objp = &value.get().toObject(); _childCompartment->wrap(_childCx, objp);
    return true;

//	if (!*objp)
//		return true;
//	AutoValueRooter tvr(_childCx, ObjectValue(**objp));
//	if (!wrap(tvr.addr()))
//		return false;
//	*objp = &tvr.value().toObject();
//	_childCompartment->wrap(_childCx, objp);
//	return true;
}

bool Membrane::wrap(HeapPtrAtom *objp) {
	if (!*objp)
		return true;

	AutoValueRooter tvr(_childCx, StringValue(*objp));
	if (!wrap(tvr.addr()))
		return false;
	JSString *str = tvr.value().toString();
	*objp = &str->asAtom();
	return true;
}

bool Membrane::unwrap(Value *vp) {
	JSContext *cx = _parentCx; //_childCx;

	//	JS_CHECK_RECURSION(cx, return false);

	if (!vp->isMarkable())
		return true;

	/* Unwrap incoming objects. */
	if (vp->isObject()) {
		JSObject *obj = &vp->toObject();
		JS_ASSERT(obj->compartment() == _childCompartment);

		if (IsCrossThreadWrapper(obj)) {
			vp->setObject(*wrappedObject(obj));
			return true;
		}

		if (obj == _childGlobal) {
			return _parentGlobal;
		}
	}

	/* Lookup strings and return any existing ones. */
	if (vp->isString()) {
		Value orig = *vp;
		JSString *str = vp->toString();
		size_t length = str->length();
		const jschar *chars = str->getChars(cx);
		if (!chars)
			return false;


		JSAtom *atom = NULL;
		{
			_main_loop->lockMainLoop();
			JSAtom *a = NULL;
			if (cx->runtime->staticStrings.lookup(chars, length))
				atom = a;
			if(a==NULL) {
				if (AtomSet::Ptr p = cx->runtime->atoms.lookup(AtomHasher::Lookup(chars, length)))
					atom = p->asPtr();
			}
			_main_loop->unlockMainLoop();
		}

		if (atom != NULL) {
			vp->setString(atom);
			return true;
		}
	}

	/* We can't always unwrap. */
	return false;
}

bool Membrane::wrapId(jsid *idp) {
	if (JSID_IS_INT(*idp))
		return true;
	AutoValueRooter tvr(_childCx, IdToValue(*idp));
	if (!wrap(tvr.addr()))
		return false;
	return ValueToId(_childCx, tvr.value(), idp);
}

bool Membrane::unwrapId(jsid *idp) {
	if (JSID_IS_INT(*idp))
		return true;
	AutoValueRooter tvr(_childCx, IdToValue(*idp));
	if (!unwrap(tvr.addr()))
		return false;
	return ValueToId(_childCx, tvr.value(), idp);
}

bool Membrane::wrap(AutoIdVector &props) {
	jsid *vector = props.begin();
	int length = props.length();
	for (size_t n = 0; n < size_t(length); ++n) {
		if (!wrapId(&vector[n]))
			return false;
	}
	return true;
}

bool Membrane::wrap(PropertyOp *propp) {
	Value v = CastAsObjectJsval(*propp);
	if (!wrap(&v))
		return false;
	*propp = CastAsPropertyOp(v.toObjectOrNull());
	return true;
}

bool Membrane::wrap(StrictPropertyOp *propp) {
	Value v = CastAsObjectJsval(*propp);
	if (!wrap(&v))
		return false;
	*propp = CastAsStrictPropertyOp(v.toObjectOrNull());
	return true;
}

bool Membrane::wrap(PropertyDescriptor *desc) {
	// Some things that were non-obvious to me at first:
	// 1. We are mutating the fields of the PropertyDescriptor in place. It
	//    is an rval.
	// 2. If this is a "real" property, then the result of the "get()"
	//    will be desc->value (which we will wrap) and the desc->getter
	//    will either be NULL or JS_PropertyStub.
	// 3. Otherwise, desc->getter may be a JS function (or other kind of
	//    object).  In that case, the JSPROP_GETTER/JSPROP_SETTER flag
	//    will be set.

	return wrap(&desc->obj)
			&& (!(desc->attrs & JSPROP_GETTER) || wrap(&desc->getter))
			&& (!(desc->attrs & JSPROP_SETTER) || wrap(&desc->setter))
	//FIXME: do we need to wrap the desc value?
	&& wrap(&desc->value)
			;
}

#define PIERCE(cx, pre, op, post)      \
    JS_BEGIN_MACRO                              \
        AutoReadOnly ro(cx);                    \
        bool ok = (pre) && (op);                \
        return ok && (post);                    \
    JS_END_MACRO

#define NOTHING (true)



static bool
WrapForSameCompartment(JSContext *cx, HandleObject obj, Value *vp)
{
    JS_ASSERT(cx->compartment == obj->compartment());
    if (!cx->runtime->sameCompartmentWrapObjectCallback) {
        vp->setObject(*obj);
        return true;
    }

    JSObject *wrapped = cx->runtime->sameCompartmentWrapObjectCallback(cx, obj);
    if (!wrapped)
        return false;
    vp->setObject(*wrapped);
    return true;
}



void
Membrane::cacheWrappedIds(JSContext* cx, jsid mainObjId, jsid wrappedId) {

	if(JSID_IS_INT(wrappedId))
		return;

	Value v;
	JS_IdToValue(cx, wrappedId, &v);
	_wrappersRack->idCachePut(cx, v, wrappedId);
}


jsid
Membrane::getUnwrappedId(JSContext* cx, jsid wrappedId) {

	if(JSID_IS_INT(wrappedId))
		return wrappedId;

	Value v;
	JS_IdToValue(cx, wrappedId, &v);

	jsid unwrapped;
	if(_wrappersRack->idCacheLookup(cx, v, &unwrapped)) {

#ifdef TXWRAPPERLOG
			fprintf(stderr, " -- cache hit '%s'\n", JS_EncodeString(cx, JSVAL_TO_STRING(v)));
#endif
		return unwrapped;
	}

	// else, try to unwrap it
	unwrapped = wrappedId;

//	_main_loop->lockMainLoop();
	if(!unwrapId(&unwrapped)) {

		jsid newUnwrappedId;

		PRMonitor* mon = PR_NewMonitor();

		PR_EnterMonitor(mon);

		_main_loop->unwrapIdInRuntime(unwrapped, &newUnwrappedId, mon);

		PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
		PR_ExitMonitor(mon);
		PR_DestroyMonitor(mon);

		unwrapped = newUnwrappedId;
		_wrappersRack->idCachePut(cx, v, unwrapped);
	}
	else {
		_wrappersRack->idCachePut(cx, v, unwrapped);
	}
//	_main_loop->unlockMainLoop();

	return unwrapped;
}





bool
Membrane::wrap(Value *vp, bool isArg)
{
	JSContext *cx = _childCx;
	JSRuntime *rt = cx->runtime;
	JSCompartment *com = cx->compartment;

	unsigned flags = 0;

    JS_CHECK_RECURSION(cx, return false);

#ifdef DEBUG
    struct AutoDisableProxyCheck {
        JSRuntime *runtime;
        AutoDisableProxyCheck(JSRuntime *rt) : runtime(rt) {
            runtime->gcDisableStrictProxyCheckingCount++;
        }
        ~AutoDisableProxyCheck() { runtime->gcDisableStrictProxyCheckingCount--; }
    } adpc(rt);
#endif


#ifdef TXWRAPPERLOG
    if (vp->isObject()) {
    	Rooted<JSObject*> obj(cx, &vp->toObject());

    	if (JS_ObjectIsFunction(cx, obj)) {

    		JSFunction* fun = obj->toFunction();
    		JSString *str = fun->displayAtom();
    		if(str) {
    			char *c_str = JS_EncodeString(cx, str);

    			int num = -1;
    			if(fun->isInterpreted())
    				if(fun->environment())
    					num = fun->environment()->propertyCount();

    			WLOG("  >>> Wrapping function (name: %s, env.prop %d, isproxy:%s)\n", c_str, num, obj->isProxy()? "true" : "false");
    		}
    		else {
    			int num = -1;
    			if(fun->isInterpreted())
    				if(fun->environment())
    					num = fun->environment()->propertyCount();

    			WLOG("  >>> Wrapping lambda function (env.prop %d, isproxy:%s)\n", num, obj->isProxy()? "true" : "false");
    		}
    	}
    	else {
    		bool arg = obj->isScope();
    		bool glo = obj->isGlobal();

    		WLOG("  >>> Wrapping object (prop: %d, scopeobj:%s, isglob:%s, isproxy:%s)\n", obj->propertyCount(), arg? "true" : "false", glo? "true" : "false", obj->isProxy()? "true" : "false" );

//    		_printInfo(cx, obj, "foo");
    	}
    }
    if (vp->isString()) {
        JSString *str = vp->toString();

        char* s = JS_EncodeString(cx, str);

        WLOG("  >>> Wrapping atom '%s'\n", s)
    }
#endif

    /* Only GC things have to be wrapped or copied. */

    if (!vp->isMarkable())
        return true;


    if (vp->isString()) {
        JSString *str = vp->toString();

        /* If the string is already in this compartment, we are done. */
        if (str->compartment() == com) {

        	WLOG("  >>> Cache hit! (same com)\n");
            return true;
        }
    }

    /* If we already have a wrapper for this value, use it. */
    RootedValue key(cx, *vp);
    if (WrapperMap::Ptr p = _map.lookup(key)) {
        *vp = p->value;
		WLOG("  >>> Cache hit!\n");
        return true;
    }


//    if (vp->isString()) {
//        RootedValue orig(cx, *vp);
//        JSStableString *str = vp->toString()->ensureStable(cx);
//        if (!str)
//            return false;
//        JSString *wrapped;
//        if(!str->isAtom()) {
//        	wrapped = js_NewStringCopyN(cx, str->chars().get(), str->length());
//        }
//        else {
//        	const jschar *chars = str->chars().get();
//        	wrapped = AtomizeChars(cx, chars, str->length());
//        }
//        if (!wrapped)
//            return false;
//        vp->setString(wrapped);
//        return _map.put(orig, *vp);
//    }


    if (vp->isString()) {
        RootedValue orig(cx, *vp);
        JSString *str = vp->toString();
        if (!str)
            return false;
        JSString *wrapped;
        if(!str->isAtom()) {
        	wrapped = js_NewStringCopyN(cx, str->getChars(cx), str->length());
        }
        else {
    		const jschar *chars = str->getChars(cx);
    		if (!chars)
    			return false;
        	wrapped = AtomizeChars(cx, chars, str->length());
        }
        if (!wrapped)
            return false;
        vp->setString(wrapped);

		return put(orig, *vp);
    }


	Rooted<JSObject*> obj(cx, &vp->toObject());

	if (obj == _parentGlobal) {
		printf("\n\nGGGG\n\n");
		vp->setObject(*_childGlobal);
		return true;
	}
    JSObject* global = _childGlobal;

	/* Split closures */
    if (vp->isObject()) {

    	if (JS_ObjectIsFunction(cx, obj)) {
    		Rooted<JSFunction*> fn(cx, obj->toFunction());
    		JSObject* env = NULL;

    		JS_ASSERT(global);
    		JS_ASSERT(obj);

    		JSObject* eee = global;
    		if(fn->isInterpreted()) {
    			if( ! fn->environment()->isGlobal())
    				eee = NULL;
    		}
    		else {
//    			if (fn->isNativeConstructor())
//    				return false;
//    			if (!isSafeNative(fn->native())) {
//    				JS_ReportError(cx, "Cannot access native functions "
//    						"from child tasks");
//    				return false;
//    				return true;
//    			}
    		}

    		WLOG("  >>> Cloning function object with global: %p\n", eee);
    		Rooted<JSObject*> wrapper(cx, JS_CloneFunctionObject(cx, obj, eee));
    		JS_ASSERT(wrapper);
    		Rooted<JSFunction*> cloned_fn(cx, wrapper->toFunction());

    		if (!wrap(&cloned_fn->atom_)) // FIXME: total hack :)
				return false;

    		if(fn->isInterpreted()) {

        		{
        			RootedScript script(cx);
        			cloned_fn->maybeGetOrCreateScript(cx, &script);

//        			RootedScript script(cx, cloned_fn->script());

        			for(unsigned n=0; n<script->natoms; n++) {
        				if (!wrap(&script->atoms[n]))
        					return false;
        			}
        		}

    			if(eee == NULL) {
    				env = copyAndWrapEnvironment(cx, cloned_fn, fn->environment());

    				WLOG("  >>> Wrapping function environment\n");

    				// TODO do we need both?
    				cloned_fn->setEnvironment(env);
    				cloned_fn->initEnvironment(env);
    				cloned_fn->getTxMetadata()->setSkipMjit(true);
    			}
    		}


    		vp->setObject(*wrapper);

    		if (!_map.put( OBJECT_TO_JSVAL(obj), *vp))
    			return false;

    		// We now have to copy over any properties.  I do this *after*
    		// putting the wrapper into the table lest there is a
    		// recursive reference.
//    		if (!copyAndWrapProperties(cx, fn, wrapper))
//    			return false;

    		return true;
    	}
    }



    JSObject *proto = Proxy::LazyProto;
//    JSObject *proto = obj->getProto();
//    if (!wrap(&proto))
//    	return false;

	JSObject *o = &vp->toObject();
	JSObject *wrapper = this->_wrappersRack->getWrapperObject(cx, ObjectValue(*o),
			proto, _childGlobal, o->isCallable() ? o : NULL, NULL);

    vp->setObject(*wrapper);

	if (!put(OBJECT_TO_JSVAL(obj), *vp))
        return false;

    return true;
}









void Membrane::releaseProxies() {}



}



}
