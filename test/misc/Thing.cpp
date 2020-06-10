#include "Thing.h"

Thing::Thing(Bookkeeper& bookkeeper)
    : bookkeeper(bookkeeper) {
}

void Thing::increment() {
    bookkeeper.increment(*this);
}
