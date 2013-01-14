/*
 * tqworker.cpp
 *
 *  Created on: Dec 7, 2012
 *      Author: usiusi
 */


#include "tqworker.h"

#include <jsapi.h>

#include "tqtxwrapper.h"
#include "tqtxutils.h"
#include "tqmainloop.h"

#include <unistd.h>


namespace tq {

using namespace JS;


/**********************************************************************************************************************************/
/**********************************************************************************************************************************/
/**********************************************************************************************************************************/



static JSBool
worker_global_enumerate(JSContext *cx, HandleObject obj)
{
#ifdef LAZY_STANDARD_CLASSES
	return JS_EnumerateStandardClasses(cx, obj);
#else
	return true;
#endif
}

static JSBool
worker_global_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
		MutableHandleObject objp)
{
#ifdef LAZY_STANDARD_CLASSES
	JSBool resolved;

	if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
		return false;
	if (resolved) {
		objp.set(obj);
		return true;
	}
#endif

#if defined(SHELL_HACK) && defined(DEBUG) && defined(XP_UNIX)
	if (!(flags & JSRESOLVE_QUALIFIED)) {
		/*
		 * Do this expensive hack only for unoptimized Unix builds, which are
		 * not used for benchmarking.
		 */
		char *path, *comp, *full;
		const char *name;
		bool ok, found;
		JSFunction *fun;

		if (!JSVAL_IS_STRING(id))
			return true;
		path = getenv("PATH");
		if (!path)
			return true;
		path = JS_strdup(cx, path);
		if (!path)
			return false;
		JSAutoByteString name(cx, JSVAL_TO_STRING(id));
		if (!name)
			return false;
		ok = true;
		for (comp = strtok(path, ":"); comp; comp = strtok(NULL, ":")) {
			if (*comp != '\0') {
				full = JS_smprintf("%s/%s", comp, name.ptr());
				if (!full) {
					JS_ReportOutOfMemory(cx);
					ok = false;
					break;
				}
			} else {
				full = (char *)name;
			}
			found = (access(full, X_OK) == 0);
			if (*comp != '\0')
				free(full);
			if (found) {
				fun = JS_DefineFunction(cx, obj, name, Exec, 0,
						JSPROP_ENUMERATE);
				ok = (fun != NULL);
				if (ok)
					objp.set(obj);
				break;
			}
		}
		JS_free(cx, path);
		return ok;
	}
#else
	return true;
#endif
}

void worker_reportError(JSContext *cx, const char *message, JSErrorReport *report) {
	fprintf(stderr, "%s:%u:%s\n",
			report->filename ? report->filename : "<no filename="">",
					(unsigned int) report->lineno,
					message);
}

JSClass worker_global_class = {
		"global", JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
		JS_PropertyStub,  JS_PropertyStub,
		JS_PropertyStub,  JS_StrictPropertyStub,
		worker_global_enumerate, (JSResolveOp) worker_global_resolve,
		JS_ConvertStub,   NULL
};

static JSBool
WorkerPrint(JSContext *cx, unsigned argc, jsval *vp)
{
	jsval *argv;
	unsigned i;
	JSString *str;
	char *bytes;

	argv = JS_ARGV(cx, vp);
	for (i = 0; i < argc; i++) {
		str = JS_ValueToString(cx, argv[i]);
		if (!str)
			return false;
		bytes = JS_EncodeString(cx, str);
		if (!bytes)
			return false;

#ifdef PRINTTHREADNAME
#ifdef HASPRTHREADNAME
        fprintf(stderr, "[%s] %s%s", PR_GetThreadName(PR_GetCurrentThread()), i ? " " : "", bytes);
#else
        fprintf(stderr, "[xx] %s%s", i ? " " : "", bytes);
#endif

#if JS_TRACE_LOGGING
TraceLog(TraceLogging::defaultLogger(), bytes);
#endif

#else
#ifdef PRINTSTDERR
	fprintf(stderr, "%s%s\n", i ? " " : "", bytes);
#else
	fprintf(stdout, "%s%s\n", i ? " " : "", bytes);
#endif

#endif
		JS_free(cx, bytes);
	}


//	fputc('\n', stderr);
	fflush(stderr);

	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return true;
}

static JSFunctionSpecWithHelp WorkerGlobalFunctions[] = {

		JS_FN_HELP("print", WorkerPrint, 0, 0,
				"print([exp ...])",
				"  Evaluate and print expressions to stdout."),

				JS_FS_HELP_END
};

/**********************************************************************************************************************************/
/**********************************************************************************************************************************/
/**********************************************************************************************************************************/






bool LoopWorker::initLoop() {

	JSRuntime* rt = JS_NewRuntime(8L * 1024L * 1024L, JS_NO_HELPER_THREADS);
	if (!rt)
		return false;

	JSContext* cx = JS_NewContext(rt, 8192);
	if (cx == NULL)
		return false;

	JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_METHODJIT);
	JS_SetVersion(cx, JSVERSION_LATEST);
	JS_SetErrorReporter(cx, worker_reportError);

	{
		JSAutoRequest ar(cx);

		js::RootedObject global(cx, JS_NewGlobalObject(cx, &worker_global_class, NULL));
		if (!global)
			return false;

		if (!JS_InitStandardClasses(cx, global))
			return false;

		if (!JS_DefineFunctionsWithHelp(cx, global, WorkerGlobalFunctions))
			return false;

		if (!_tx_wrappers.get())
			_tx_wrappers.reset(tq::tx::TxWrappersRack::create(cx, global));

		_global = global;

		if(!addRoot(cx))
			return false;
	}

#ifdef HASPRTHREADNAME
	char* s;
	asprintf(&s, "T%d", _id);
	PR_SetCurrentThreadName(s);
#endif
	_rt = rt;
	_cx = cx;

	JS_ClearRuntimeThread(_rt);
	JS_SetRuntimeThread(_rt);

//	_main_loop->lockMainLoop();

	if(*_main_loop->getThreadIndex() == -1) {
		if (PR_FAILURE == PR_NewThreadPrivateIndex(_main_loop->getThreadIndex(), NULL)) {
			return false;
		}
	}
//	_main_loop->unlockMainLoop();

	if(PR_FAILURE == PR_SetThreadPrivate(*_main_loop->getThreadIndex(), this)) {
		return false;
	}
	return true;
}





void LoopWorker::runEvLoop() {

	while ( ! _main_loop->terminate()) {

		EventHandle* e = NULL;

		e = _main_loop->popEvent();

		if(e == NULL) {
			if(*_main_loop->getRunningWorkers() == 0)
				_main_loop->setTerminate(true);
		}

		if (e != NULL)
		{
			JS_ATOMIC_INCREMENT(_main_loop->getRunningWorkers());

			EV("Event '%s' started\n", e->getPrintableEventLabel(_cx));

			if( ! e->execute(_cx, _tx_wrappers.get(), _main_loop, _local_event_queue) )
				printf("Error Executing CB! \n");

			EV("Event '%s' executed (tot: %d)\n", e->getPrintableEventLabel(_cx), _main_loop->getStats()->_tx_committed);

			JS_ATOMIC_DECREMENT(_main_loop->getRunningWorkers());

			if(e->_has_wait_guard) {
				JS_ATOMIC_DECREMENT(&e->_wait_guard->_guard_counter);
				EV("Wait guard count for event '%s' dec (%d)\n", e->getPrintableEventLabel(_cx), e->_wait_guard->_guard_counter);

				if(e->_wait_guard->_guard_counter == 0) {

					_main_loop->clearWaitGuard(_cx, e);
					JS_ATOMIC_INCREMENT(&(_main_loop->getStats()->_ev_started));

					_main_loop->emitEvent(_cx, e->getGuardEvent(_cx));

					EV("Executing wait guard for ev '%s'\n", e->getPrintableEventLabel(_cx));
				}
			}

			// push back callback object to original context
			e->_parent->disposeEventCallback(e->_cb);
			delete e;
		}

		if(_main_loop->terminate() == true) {
			bool die = (_main_loop->getStats()->_ev_started == _main_loop->getStats()->_tx_committed);
			_main_loop->setTerminate(die);
		}
	}
}




JSBool LoopWorker::addRoot(JSContext *cx) {

	if (!JS_AddNamedObjectRoot(cx, &_global, "TaskContext::_global"))
		return false;
	return true;
}



JSBool LoopWorker::delRoot(JSContext *cx) {
	JSAutoRequest ar(cx);
	JS_RemoveObjectRoot(cx, &_global);
	return true;
}




}




