
#include "vm/ScopeObject.h"


/*****************************************************************/

#define USE_TX

/*
 * Tx Commit return codes
 * */
#define TX_COMMIT_OK 0
#define TX_COMMIT_NOK -1

// commit-time aborst
#define TX_COMM_ABORT_LOCK_WRITE_SET 1
#define TX_COMM_ABORT_VALIDATING_READ_SET_VERSION 2

// runtime abort
#define TX_RT_ABORT_VALIDATING_READ_SET_LOCK 3
#define TX_RT_ABORT_VALIDATING_READ_SET_VERSION 4

// generic abort
#define TX_ABORT_JS_ERROR 5




/*****************************************************************/

/*
 * Constant values
 * */

#define QUOLL_V "0.2b"
#define QUOLL_CODENAME "Young Quoll"

#define NUM_THREADS 2
#define HASPRTHREADNAME


/*****************************************************************/

/*
 * Log/info utils
 * */

// comment the following 3 define to run tests
//#define PRINTSTDERR
//#define PRINTTHREADNAME
//#define PRINTTQVERSION


// Print generic infos (using INFO macro)
//#define INFOLOG

// Print TX-related infos
//#define TXLOG
//#define TXLOGVERBOSE

// Print event emission infos
//#define EVLOG

// Print perf-related infos
//#define PERF
//#define PERFSUMMARY
//#define PERFSUMMARYCOMPACT

// Print wrapper-related infos
//#define TXWRAPPERLOG

// Dump a summary of the TX performance
//#define TXDUMPSUMMARY
//#define TXDUMPSUMMARY_LAG 0



/*****************************************************************/

#define TQEVAL(cx, global, text) \
	{ \
		JSAutoRequest ar(cx); \
		jsval _; \
		JS_EvaluateScript(cx, global, text, strlen(text), "", 0, &_); \
	} \


#define TQEVALF(cx, global, text) \
	{ \
		JSAutoRequest ar(cx); \
		jsval vp; \
		JSFunction* fun = JS_CompileFunction(cx, global, "untrusted", 0, NULL, text, strlen(text), "", 0); \
        JS_CallFunctionValue(cx, NULL, OBJECT_TO_JSVAL(fun), 0, NULL, &vp); \
	} \





/*****************************************************************/

/*
 * Util functions/macros
 * */

static
const char* _getThName(PRThread* th) {
#ifndef HASPRTHREADNAME
	return "xx";
#else
	return PR_GetThreadName(th);
#endif
}


#ifdef INFOLOG

#define INFO(...) fprintf(stdout, __VA_ARGS__)
#define DEBUGTQ(...) fprintf(stderr, __VA_ARGS__)

#endif
#ifndef INFOLOG
#define INFO(...) { }
#define DEBUGTQ(...) { }
#endif



#ifdef EVLOG

#define EV(...) \
{ \
		char* __s;	\
		asprintf(&__s, __VA_ARGS__);	\
		fprintf(stderr, "[%s] %s", _getThName(PR_GetCurrentThread()), __s );  \
} \

#endif
#ifndef EVLOG
#define EV(...) { }
#endif






#ifdef TXWRAPPERLOG

#define WLOG(...) \
{ \
		char* __s;	\
		asprintf(&__s, __VA_ARGS__);	\
		fprintf(stderr, "[%s] %s", _getThName(PR_GetCurrentThread()), __s );  \
} \

#endif
#ifndef TXWRAPPERLOG
#define WLOG(...) { }
#endif






#ifdef TXLOG

#define TX(...) \
{ \
		char* __s;	\
		asprintf(&__s, __VA_ARGS__);	\
		fprintf(stderr, "[%s] %s", _getThName(PR_GetCurrentThread()), __s );  \
} \

#endif
#ifndef TXLOG
#define TX(...) { }
#endif








#ifdef PERF

#define T(...) \
{ \
		char* __s;	\
		asprintf(&__s, __VA_ARGS__);	\
		fprintf(stderr, "[%s] %s", _getThName(PR_GetCurrentThread()), __s );  \
} \

#endif
#ifndef PERF
#define T(...) { }
#endif



static
void _printInfo(JSContext* cx, JSObject* obj, char* comment) {

	fprintf(stderr, "--------- BEGIN OBJECT DUMP (%s) ---------\n", comment);

	fprintf(stderr, "[%s] Obj: %p\n", _getThName(PR_GetCurrentThread()), obj );
	fprintf(stderr, "[%s] info: [", _getThName(PR_GetCurrentThread()), &obj );

	if(obj->isArguments()) fprintf(stderr, " +isarguments" );
	if(obj->isArray()) fprintf(stderr, " +isarray");;
	if(obj->isArrayBuffer()) fprintf(stderr, " +isArrayBuffer" );
	if(obj->isBlock()) fprintf(stderr, " +isBlock" );
	if(obj->isBoolean()) fprintf(stderr, " +isBoolean" );
	if(obj->isBoundFunction()) fprintf(stderr, " +isBoundFunction" );
	if(obj->isCall()) fprintf(stderr, " +isCall" );
	if(obj->isCallable()) fprintf(stderr, " +isCallable" );
	if(obj->isClonedBlock()) fprintf(stderr, " +isClonedBlock" );
	if(obj->isCrossCompartmentWrapper()) fprintf(stderr, " +isCrossCompartmentWrapper" );
	if(obj->isDataView()) fprintf(stderr, " +isDataView" );
	if(obj->isDate()) fprintf(stderr, " +isDate" );
	if(obj->isDebugScope()) fprintf(stderr, " +isDebugScope" );
	if(obj->isDeclEnv()) fprintf(stderr, " +isDeclEnv" );
	if(obj->isDelegate()) fprintf(stderr, " +isDelegate" );
	if(obj->isDenseArray()) fprintf(stderr, " +isDenseArray" );
	if(obj->isElementIterator()) fprintf(stderr, " +isElementIterator" );
	if(obj->isError()) fprintf(stderr, " +isError" );
	if(obj->isExtensible()) fprintf(stderr, " +isExtensible" );
	if(obj->isFunction()) fprintf(stderr, " +isFunction" );
	if(obj->isFunctionProxy()) fprintf(stderr, " +isFunctionProxy" );
	if(obj->isGenerator()) fprintf(stderr, " +isGenerator" );
	if(obj->isGlobal()) fprintf(stderr, " +isGlobal" );
	if(obj->isIndexed()) fprintf(stderr, " +isIndexed" );
	if(obj->isMapIterator()) fprintf(stderr, " +isMapIterator" );
	if(obj->isNamespace()) fprintf(stderr, " +isNamespace" );
	if(obj->isNative()) fprintf(stderr, " +isNative" );
	if(obj->isNestedScope()) fprintf(stderr, " +isNestedScope" );
	if(obj->isNormalArguments()) fprintf(stderr, " +isNormalArguments" );
	if(obj->isNumber()) fprintf(stderr, " +isNumber" );
	if(obj->isObject()) fprintf(stderr, " +isObject" );
	if(obj->isPrimitive()) fprintf(stderr, " +isPrimitive" );
	if(obj->isPropertyIterator()) fprintf(stderr, " +isPropertyIterator" );
	if(obj->isProxy()) fprintf(stderr, " +isProxy" );
	if(obj->isQName()) fprintf(stderr, " +isQName" );
	if(obj->isRegExp()) fprintf(stderr, " +isRegExp" );
	if(obj->isRegExpStatics()) fprintf(stderr, " +isRegExpStatics" );
	if(obj->isScope()) fprintf(stderr, " +isScope" );
	if(obj->isSetIterator()) fprintf(stderr, " +isSetIterator" );
	if(obj->isSlowArray()) fprintf(stderr, " +isSlowArray" );
	if(obj->isStaticBlock()) fprintf(stderr, " +isStaticBlock" );
	if(obj->isStopIteration()) fprintf(stderr, " +isStopIteration" );
	if(obj->isStrictArguments()) fprintf(stderr, " +isStrictArguments" );
	if(obj->isString()) fprintf(stderr, " +isString" );
	if(obj->isTypedArray()) fprintf(stderr, " +isTypedArray" );
	if(obj->isVarObj()) fprintf(stderr, " +isVarObj" );
	if(obj->isWeakMap()) fprintf(stderr, " +isWeakMap" );
	if(obj->isWith()) fprintf(stderr, " +isWith" );
	if(obj->isWrapper()) fprintf(stderr, " +isWrapper" );
	if(obj->isXML()) fprintf(stderr, " +isXML" );
	if(obj->isXMLId()) fprintf(stderr, " +isXMLId" );
	fprintf(stderr, " ]\n");

	fprintf(stderr, "[%s] props (tot:%d):\n", _getThName(PR_GetCurrentThread()), obj->propertyCount() );

	if(!obj->isProxy())
	{
		JSAutoCompartment ac(cx, obj);

		JS::AutoIdVector props(cx);
		if (!GetPropertyNames(cx, obj, JSITER_OWNONLY /*| JSITER_HIDDEN*/, &props))
			fprintf(stderr, "ERROR!!!\n");


		for (jsid *v = props.begin(), *v_end = props.end(); v < v_end; v++) {
			jsid pid = *v;

			if(JSID_IS_ATOM(pid))
			{
				JSString *str = JSID_TO_FLAT_STRING(pid);
				char *c_str = JS_EncodeString(cx, str);
				fprintf(stderr, "[%s]   +name:'%s'\n", _getThName(PR_GetCurrentThread()), c_str );
			}
		}
	}
	else {
		fprintf(stderr, "[%s]    (proxy) total prop: %d \n", _getThName(PR_GetCurrentThread()), obj->propertyCount() );
	}

	if(obj->isFunction()) {

		JSFunction* fun = obj->toFunction();

	    JSString *str = fun->displayAtom();
	    if(str) {
	    	char *c_str = JS_EncodeString(cx, str);
	    	fprintf(stderr, "[%s] function name '%s'\n", _getThName(PR_GetCurrentThread()), c_str );
	    }
		fprintf(stderr, "[%s] function info: [ ", _getThName(PR_GetCurrentThread()) );



		if(fun->isInterpreted()) fprintf(stderr, " +isInterpreted" );
		if(fun->isNative()) fprintf(stderr, " +isNative");
		if(fun->isNativeConstructor()) fprintf(stderr, " +isNativeConstructor");
		if(fun->isHeavyweight()) fprintf(stderr, " +isHeavyweight");
		if(fun->isFunctionPrototype()) fprintf(stderr, " +isFunctionPrototype");
		if(fun->isInterpretedConstructor()) fprintf(stderr, " +isInterpretedConstructor");
//		if(fun->isInterpreted()) if(fun->inStrictMode()) fprintf(stderr, " +inStrictMode");

		fprintf(stderr, " ]\n");

		JS::AutoAssertNoGC ng;

		if(fun->isInterpreted()) {

			JSScript* scr = fun->nonLazyScript();

			fprintf(stderr, "[%s] script info: [ ", _getThName(PR_GetCurrentThread()) );

			if(scr->argumentsHasVarBinding()) fprintf(stderr, " +argumentsHasVarBinding" );
			if(scr->analyzedArgsUsage()) fprintf(stderr, " +analyzedArgsUsage" );
			if(scr->needsArgsObj()) fprintf(stderr, " +needsArgsObj" );
			if(scr->argsObjAliasesFormals()) fprintf(stderr, " +argsObjAliasesFormals" );
			if(scr->hasAnalysis()) fprintf(stderr, " +hasAnalysis" );
			if(scr->hasConsts()) fprintf(stderr, " +hasConsts" );
			if(scr->hasObjects()) fprintf(stderr, " +hasObjects" );
			if(scr->hasRegexps()) fprintf(stderr, " +hasRegexps" );
			if(scr->hasTrynotes()) fprintf(stderr, " +hasTrynotes" );
			if(scr->isEmpty()) fprintf(stderr, " +isEmpty" );
			if(scr->argumentsHasVarBinding()) fprintf(stderr, " +argumentsHasVarBinding" );
			if(scr->isForEval()) fprintf(stderr, " +isForEval" );
			if(scr->isIonCompilingOffThread()) fprintf(stderr, " +isIonCompilingOffThread" );

			fprintf(stderr, "]\n");


			fprintf(stderr, "[%s] script attr: [ ", _getThName(PR_GetCurrentThread()) );

			fprintf(stderr, " +noScriptRval:%s", scr->noScriptRval? "T": "F" );
			fprintf(stderr, " +savedCallerFun:%s", scr->savedCallerFun? "T": "F" );
//			fprintf(stderr, " +strictModeCode:%s", scr->strictModeCode? "T": "F" );
			fprintf(stderr, " +compileAndGo:%s", scr->compileAndGo? "T": "F" );
			fprintf(stderr, " +bindingsAccessedDynamically:%s", scr->bindingsAccessedDynamically? "T": "F" );
			fprintf(stderr, " +funHasExtensibleScope:%s", scr->funHasExtensibleScope? "T": "F" );
			fprintf(stderr, " +funHasAnyAliasedFormal:%s", scr->funHasAnyAliasedFormal? "T": "F" );
			fprintf(stderr, " +warnedAboutTwoArgumentEval:%s", scr->warnedAboutTwoArgumentEval? "T": "F" );
			fprintf(stderr, " +warnedAboutUndefinedProp:%s", scr->warnedAboutUndefinedProp? "T": "F" );
			fprintf(stderr, " +hasSingletons:%s", scr->hasSingletons? "T": "F" );
			fprintf(stderr, " +isActiveEval:%s", scr->isActiveEval? "T": "F" );
			fprintf(stderr, " +isCachedEval:%s", scr->isCachedEval? "T": "F" );
			fprintf(stderr, " +isGenerator:%s", scr->isGenerator? "T": "F" );
			fprintf(stderr, " +isGeneratorExp:%s", scr->isGeneratorExp? "T": "F" );
			fprintf(stderr, " +isCachedEval:%s", scr->isCachedEval? "T": "F" );
			fprintf(stderr, " +isCachedEval:%s", scr->isCachedEval? "T": "F" );
			fprintf(stderr, " +isCachedEval:%s", scr->isCachedEval? "T": "F" );

			fprintf(stderr, "]\n");


			fprintf(stderr, "[%s] bindings attr: [ ", _getThName(PR_GetCurrentThread()) );

			fprintf(stderr, " +hasAnyAliasedBindings:%s", scr->bindings.hasAnyAliasedBindings()? "T": "F" );

			fprintf(stderr, "]\n");

			fprintf(stderr, "     +bindings count :%d\n", scr->bindings.count() );
			fprintf(stderr, "     +bindings numArgs :%d\n", scr->bindings.numArgs() );
			fprintf(stderr, "     +bindings numVars :%d\n", scr->bindings.numVars() );



		}
	}

	if(obj->isCall()) {

		js::CallObject* co = &obj->asCall() ;

		fprintf(stderr, "[%s] callobj info: [ ", _getThName(PR_GetCurrentThread()) );

		fprintf(stderr, " +isForEval:%s", co->isForEval()? "T": "F" );

		fprintf(stderr, " ]\n");

		fprintf(stderr, "[%s] callobj encscope: %p\n", _getThName(PR_GetCurrentThread()), &co->enclosingScope() );
		fprintf(stderr, "[%s] callobj callee: %p\n", _getThName(PR_GetCurrentThread()), &co->callee() );

	}





	fprintf(stderr, "--------- END OBJECT DUMP ---------\n");
}












