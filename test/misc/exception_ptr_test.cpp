#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>

using Mutex = std::mutex;
using Guard = std::lock_guard<Mutex>;
using ExPtr = std::exception_ptr;

// The first exception thrown by a thread in a component with multiple threads
class ThreadEx
{

    mutable Mutex mutex; ///< For thread safety
    ExPtr         exPtr; ///< Pointer to exception

public:
    // Default constructs.
    ThreadEx()
        : mutex()
        , exPtr()
    {}

    // Sets the exception to the current exception if it's not set.
    void set() {
        Guard guard{mutex};
        if (!exPtr)
            exPtr = std::current_exception();
    }

    // Sets the exception to that of an exception pointer if it's not set.
    void set(ExPtr& exPtr) {
        Guard guard{mutex};
        if (!this->exPtr)
            this->exPtr = exPtr;
    }

    // Sets the exception to that of an exception pointer if it's not set.
    void set(ExPtr&& exPtr) {
        Guard guard{mutex};
        if (!this->exPtr)
            this->exPtr = exPtr;
    }

    // Throws the exception if it's set. Clears the exception.
    void throwIfSet() {
        Guard guard{mutex};
        if (exPtr) {
            ExPtr copy{};
            copy.swap(exPtr);
            std::rethrow_exception(copy);
        }
    }
};

static void setEx(
        ThreadEx&  threadEx,
        ExPtr&     exPtr) {
    threadEx.set(std::current_exception());
    // Other code here
}

int main(
        int    argc,
        char** argv)
{
    try {
        ThreadEx threadEx{};
        try {
            throw std::runtime_error("Runtime error");
        }
        catch (const std::exception& ex) {
            //threadEx.set();
            ExPtr exPtr = std::current_exception();
            setEx(threadEx, exPtr);
        }
        threadEx.throwIfSet();
    }
    catch (const std::exception& ex) {
        std::cout << ex.what() << '\n'; // Prints "std::exception" and not "Runtime error"
    }
}
