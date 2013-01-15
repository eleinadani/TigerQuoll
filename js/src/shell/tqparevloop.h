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
