#ifndef BOOKKEEPER_H
#define BOOKKEEPER_h

#include "Thing.h"
#include <unordered_map>

class Bookkeeper
{
    std::unordered_map<Thing, int> map;

public:
    void add(const Thing& thing);

    void increment(const Thing& thing);
};

#endif
