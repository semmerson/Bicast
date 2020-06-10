#ifndef THING_H
#define THING_H

#include "Bookkeeper.h"
#include <cstddef>

class Thing {
    Bookkeeper bookkeeper;

public:
    Thing(Bookkeeper& bookkeeper);

    void increment();
};

namespace std {
    template<>
    struct hash<Thing>
    {
        size_t operator()(const Thing& thing) const noexcept {
            return 0; // Actual value is irrelevant
        }
    };
}

#endif
