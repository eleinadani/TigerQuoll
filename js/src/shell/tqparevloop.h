/*
 * tqparevloop.h
 *
 *  Created on: Nov 3, 2012
 *      Author: usiusi
 */

#ifndef TQPAREVLOOP_H_
#define TQPAREVLOOP_H_

#include <sys/time.h>

#define JS_ATOMIC_INCREMENT(p)      PR_ATOMIC_INCREMENT((int32_t *)(p))
#define JS_ATOMIC_DECREMENT(p)      PR_ATOMIC_DECREMENT((int32_t *)(p))

#include "tqtxmembrane.h"
#include "pjsutil.h"

#include "tqtxutils.h"
#include "tqevent.h"
#include "tqworker.h"


using namespace mozilla;
using namespace js;
using namespace js::cli;
using namespace tq::tx;
using namespace pjs;
using namespace std;

namespace tq {



//------------------------------------------------------------------------
// Parallel Event Loop class
//

class ParEvLoop {

	LoopWorker* _workers;
	int _workersCount;
	MainLoop* _main_loop;

public:
	ParEvLoop(MainLoop* mainLoop, int workersCount) {

		_workersCount = workersCount;
		_workers = new LoopWorker[workersCount];
		_main_loop = mainLoop;
	}

	~ParEvLoop() {
		delete[] _workers;
	}


	void
	RunLoop() {

		for(int i=0; i<_workersCount; i++)
			_workers[i].Start(i, _main_loop);

	}

	void
	JoinLoop() {

		for(int i=0; i<_workersCount; i++)
			_workers[i].Join();
	}

};


//
// Parallel Event Loop class
//------------------------------------------------------------------------





static
ParEvLoop*
InitLoop(MainLoop* mainLoop, int workersCount) {
	return new ParEvLoop(mainLoop, workersCount);
}

}

#endif /* TQPAREVLOOP_H_ */
