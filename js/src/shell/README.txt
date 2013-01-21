===============================================================================
(1) Build TigerQuoll
===============================================================================

-------------------- Mac OS X --------------------

Assuming you have all the src in the folder

		TigerQuoll/js/src

type the following commands:

		cd TigerQuoll/js/src 
		autoconf213

		cd shell 
		./build_tq.sh

You should now have a runnig version of TigerQuoll's shell in

		TigerQuoll/js/src/_build/js/


-------------------- Linux --------------------


Assuming you have all the src in the folder

		TigerQuoll/js/src

type the following commands:

		cd TigerQuoll/js/src 
		autoconf2.13

		cd shell 
		./build_tq.sh

You should now have a runnig version of TigerQuoll's shell in

		TigerQuoll/js/src/_build/js/



===============================================================================
(2) SetUp Eclipse CDT
===============================================================================

Detailed instructions can be found here:
x
https://developer.mozilla.org/en-US/docs/SpiderMonkey/
Setting_up_CDT_to_work_on_SpiderMonkey

------------------------------ Mac OS X ------------------------------

Assuming you have Eclipse CDT already installed somewhere, do the following:

- Start Eclipse and create a workspace somewhere 
- Select “New > Makefile Project with Existing Code” from the “File” menu 
- Give the project a name you like (e.g., TigerQuoll) and use the “Browser…” 
	button to select your checkout’s "js/src" folder for the “Existing Code
	Location”
- Choose the correct toolchain for your platform (i.e. MacOS X GCC on Mac) 
	and click “Finish”

Now you can configure the project:

- Open the project’s properties by selecting its root and clicking “Properties”
	in the “File” menu and select “C/C++ Build” 
- Under “Builder Settings”,	deactivate “Use default build command” Instead, 
	change “Build command” to "make -w" 
- Change the “Build location” to the following build directory:
	${workspace_loc:/TigerQuoll/_debug/js} 
- Under “Behavior”, make sure that “Enable parallel build” is deactivated 
- Remove “all” from “Build (Incremental build)” 
- Deactivate “Clean” so that your builds don’t take ages 
- Start the build by selecting “Build All” from the “Project” menu 
- Start the indexer by selecting “Index > Rebuild” from the project’s 
	context menu




