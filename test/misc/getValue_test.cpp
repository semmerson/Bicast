#undef NDEBUG
#include <cassert>
#include <iostream>

int& getValue() {
    static int x = 42;
    return x;
}

int main() {
    assert(getValue() == 42);

    auto value1 = getValue();
    assert(value1 == 42);
    value1 = 1;
    assert(getValue() == 42);

    auto& value2 = getValue();
    assert(value2 == 42);
    value2 = 1;
    assert(getValue() == 1);

    return 0;
}
