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

#ifndef TQJSAPI_H_
#define TQJSAPI_H_


namespace tq {
namespace api {



static JSBool
Die(JSContext *cx, unsigned argc, jsval *vp)
{
	int i=3;

	while(i!=0) {
		printf("Killing evloop: %d\n",i--);
		usleep(1000000);
	}

	_main_loop->setTerminate(true);



	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return true;
}










/*
 * __rt_tx_emit_async( callback_function, obj_owner, event_label, a1, a2 )
 *
 * */
static JSBool
Async(JSContext* cx, unsigned argc, jsval *vp)
{
	JS_ASSERT(_main_loop);

	EventLoop* worker = (EventLoop*) PR_GetThreadPrivate(*_main_loop->getThreadIndex());

	if( ! worker) {
		JS_ATOMIC_INCREMENT(&(_main_loop->getStats()->_ev_started));
		worker = _main_loop;
	}

	jsval *argv = JS_ARGV(cx, vp);
	EventCallback* cb = EventCallback::create(cx, argv[0], argv + 1, argc - 3);

	EventHandle *eh = EventHandle::create(cx, worker, _main_loop, cb);
	if (eh == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	worker->bufferEventEmission(cx, eh);

	return JS_TRUE;
}



/*
 * __rt_register_wait_guard( callback_function, obj_owner, event_label )
 *
 * */
static JSBool
Wait(JSContext* cx, unsigned argc, jsval *vp)
{
	LoopWorker* worker = NULL;
	if(_main_loop != NULL) {
		worker = (LoopWorker*) PR_GetThreadPrivate(*_main_loop->getThreadIndex());
	}

	jsval *argv = JS_ARGV(cx, vp);

	// TODO addroots!
	JSFunction* callback_function = JS_ValueToFunction(cx, argv[0]);
	JSObject* obj_owner; JS_ValueToObject(cx, argv[1], &obj_owner);
	JSString* event_label = JS_ValueToString(cx, argv[2]);

	// Async called from the main loop: the event goes directly into the global (main) ev queue
	if( ! worker) {
		EV("Registering Wait guard\n");

		_main_loop->lockMainLoop();

		_main_loop->setNewWaitGuard(cx, callback_function, obj_owner, event_label);

		_main_loop->unlockMainLoop();
	}
	// Async called by any worker: buffer it in the local ev queue
	else {
		EV("TODO: Buffered wait guard!\n");
	}

	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return true;
}







/*
 * __rt_mark_eventual( eventual_callback, owner_obj, field_name, all_eventual )
 *
 * */
static JSBool
MarkEventual(JSContext* cx, unsigned argc, jsval *vp)
{
	LoopWorker* worker = (LoopWorker*) PR_GetThreadPrivate(*_main_loop->getThreadIndex());

	if(worker) {
		fprintf(stderr,"*** ERROR: cannot mark a field as eventual from any parallel event handler***\n");
	}

	JS_ASSERT( ! worker);

	jsval *argv = JS_ARGV(cx, vp);

	JSFunction* callback_function = JS_ValueToFunction(cx, argv[0]);
	JSObject* obj_owner; JS_ValueToObject(cx, argv[1], &obj_owner);
	JSString* field_name = JS_ValueToString(cx, argv[2]);
	JSBool all_eventual; JS_ValueToBoolean(cx, argv[3], &all_eventual);

	obj_owner->getTxMetadata()->setHasEventuals();
	if(all_eventual) {
		obj_owner->getTxMetadata()->setAllEventual();
		obj_owner->getTxMetadata()->registerEventualCallback(callback_function);
		goto exit;
	}

	jsid fieldid;
	if( ! JS_ValueToId(cx, argv[2], &fieldid))
		fprintf(stderr,"*** FATAL: field not found\n");

	JS_ASSERT(JS_ValueToId(cx, argv[2], &fieldid));

	obj_owner->getTxMetadata()->addEventualProp(fieldid);

	obj_owner->getTxMetadata()->registerEventualCallback(argv[2], callback_function);
exit:
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return true;
}










}
}




#endif /* TQJSAPI_H_ */
