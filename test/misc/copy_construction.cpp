#include <functional>
#include <thread>

class Foo {
    std::thread stdThread;
public:
    Foo(const Foo& foo);

    template<class Callable, class... Args>
    explicit Foo(Callable&& callable, Args&&... args)
        : stdThread{[this](decltype(
                std::bind(callable, std::forward<Args>(args)...))&&
                        boundCallable) mutable {
                    boundCallable();
        }, std::bind(callable, std::forward<Args>(args)...)}
    {}
};

int main() {
    Foo foo1{[]{}, 1};
    Foo foo2{foo1};
    return 0;
}
