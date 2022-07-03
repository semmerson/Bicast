include_directories(/usr/include/openssl11)

# Add the library
add_library(misc OBJECT
	logging.cpp	   logging.h
	error.cpp	   error.h
	                   Shield.h
#			   DelayQueue.h
			   FixedDelayQueue.h
	FileUtil.cpp       FileUtil.h
	MapOfLists.cpp	   MapOfLists.h
        Thread.cpp         Thread.h
	LinkedHashMap.cpp  LinkedHashMap.h
	LinkedHashSet.cpp  LinkedHashSet.h
	LinkedMap.cpp	   LinkedMap.h
	Preferences.cpp    Preferences.h
	                   Stopwatch.h
)