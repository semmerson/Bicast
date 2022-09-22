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
                       HashSetQueue.h
                       HashMapQueue.h
	Preferences.cpp    Preferences.h
	                   Stopwatch.h
)