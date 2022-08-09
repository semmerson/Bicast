#include "map_queue.h"

// C API:

using cComp  = int (*)(const void* key1, const void* key2);

struct map_queue {
    struct Comp {
        cComp compare;
        Comp(cComp compare)
            : compare(compare)
        {}
        bool operator()(const void* key1, const void* key2) const {
            return compare(key1, key2) < 0;
        }
    };
    Comp                           comp;
    MapQ<const void*, void*, Comp> mapQ;

    map_queue(cComp compare)
        : comp(compare)
        , mapQ(comp)
    {}
};

MapQueue* sq_alloc(cComp compare) {
    try {
        return new map_queue(compare);
    }
    catch (const std::exception& ex) {
        return nullptr;
    }
}

size_t mq_size(const MapQueue* mq) {
    return mq->mapQ.size();
}

bool mq_empty(const MapQueue* mq) {
    return mq->mapQ.empty();
}

void* mq_insert(MapQueue* mq, const void* key, void* value) {
    try {
        auto pair = mq->mapQ.insert(key, value);
        return pair.second ? pair.first->second : nullptr;
    }
    catch (const std::exception& ex) {
        return nullptr;
    }
}

bool mq_front(const MapQueue* mq, const void** key, void** value) {
    auto pair = mq->mapQ.front();
    if (pair.second) {
        *key = pair.first->first;
        *value = pair.first->second;
    }
    return pair.second;
}

void mq_pop(MapQueue* mq) {
    mq->mapQ.pop();
}

void mq_free(MapQueue* mq) {
    if (mq)
        delete mq;
}
