#include <cstdint>
#include <cstdlib>
#include <iostream>

using Type = uint16_t;

enum Enum : Type {
    VALUE
};

static void func(const uint16_t value) {
    std::cout << "Unsigned, 16-bit argument\n";
}

static void func(const int32_t value) {
    std::cout << "Signed, 32-bit argument\n";
}

int main(int argc, char** argv) {
    Type value = 1;
    func(value);
    func(Enum::VALUE);
    func(static_cast<Type>(Enum::VALUE));
}
