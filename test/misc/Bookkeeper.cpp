#include "Thing.h"
#include "Bookkeeper.h"

void Bookkeeper::add(const Thing& thing) {
    map.insert({thing, 0});
}

void Bookkeeper::increment(const Thing& thing) {
    ++map[thing];
}
