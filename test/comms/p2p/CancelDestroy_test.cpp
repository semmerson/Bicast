/**
 * This file tests the interaction between thread-cancellation and object
 * destruction.
 */

#include <climits>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

class Obj {
public:
	Obj()  { std::clog << "Obj() called\n"; }
	~Obj() { std::clog << "~Obj() called\n"; }
};

static void cleanup(void* arg) {
	std::clog << "cleanup() called\n";
}

static void run() {
	Obj obj{};
	pthread_cleanup_push(cleanup, nullptr);
	::pause(); // Thread cancelled here. `~Obj()` is called.
	pthread_cleanup_pop(1);
}

int main(int argc, char **argv) {
	std::thread thread([]{run();});
	::sleep(1);
	::pthread_cancel(thread.native_handle());
	thread.join();
}
