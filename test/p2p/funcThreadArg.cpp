#include <thread>

class Foo {
    // void run(void func())             // Doesn't work
    void run(std::function<void()> func) // Works
    {
        func();
    }

    void bar() {
        return;
    }

    void bof() {
        std::thread thread(&Foo::run, this, [&]{bar();});
        thread.join();
    }
};
