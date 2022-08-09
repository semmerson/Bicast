/**
 * This file declares the API for an object that's a combination of sorted-map
 * and queue.
 */
#ifndef MAP_QUEUE_H_
#define MAP_QUEUE_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct map_queue MapQueue;

/**
 * Allocates a map-queue.
 *
 * @param[in] compare  Key comparison function. Returns a integer that is less
 *                     than, equal to, or greater than zero as the first key is
 *                     considered less than, equal to, or greater than the
 *                     second key, respectively.
 * @return             Pointer to newly-allocated instance
 * @retval    NULL     Out-of-memory.
 * @see                `mq_free()`
 */
MapQueue* sq_alloc(int (*compare)(const void* key1, const void* key2));

/**
 * Returns the number of entries in a map-queue.e., has no entries).
 *
 * @param[in] mq       Pointer to the map-queue
 * @return             The number of entries in the map-queue
 */
size_t mq_size(const MapQueue* mq);

/**
 * Indicates if a map-queue is empty (i.e., has no entries).
 *
 * @param[in] mq       Pointer to the map-queue
 * @retval    `true`   The map-queue is empty
 * @retval    `false`  The map-queue is not empty
 */
bool mq_empty(const MapQueue* mq);

/**
 * Inserts an entry into a map-queue if it doesn't already exist. This is
 * consonant with the map aspect of a map-queue.
 *
 * @param[in,out] mq     Pointer to the map-queue
 * @param[in]     key    Pointer to the key. It is the caller's responsibility
 *                       to manage storage for the key.
 * @param[in]     value  Pointer to the value.  It is the caller's
 *                       responsibility to manage storage for the value.
 * @return               Pointer to the value associated with the key. The
 *                       returned pointer be equal to `value` if and only if a
 *                       new entry was created.
 * @retval        NULL   Out of memory
 */
void* mq_insert(MapQueue* mq, const void* key, void* value);

/**
 * Returns, but doesn't remove, the entry in a map-queue whose key compares the
 * least. This is consonant with the queue aspect of a map-queue.
 *
 * @param[in]  mq       Pointer to the map-queue
 * @param[out] key      Pointer to a pointer to the key of the entry that
 *                      compares the least
 * @param[out] value    Pointer to a pointer to the value associated with the key
 * @retval     `true`   Success. `*key` and `*value` are set.
 * @retval     `false`  Failure. The map-queue is empty.
 * @see                 `mq_pop()`
 */
bool mq_front(const MapQueue* mq, const void** key, void** value);

/**
 * Removes the entry in a map-queue whose key compares the least. Does nothing
 * if the map-queue is empty. This is consonant with the queue aspect of a
 * map-queue.
 *
 * @param[in,out] mq  Pointer to the map-queue
 * @see               `mq_front()`
 */
void mq_pop(MapQueue* mq);

/**
 * Releases all resources associated with a map-queue.
 *
 * @param[in] mq  Pointer to the map-queue. If `NULL`, then nothing happens.
 * @see           `mq_alloc()`
 */
void mq_free(MapQueue* mq);

#ifdef __cplusplus
}

#include <map>

/**
 * C++ class that's a combination of sorted-map and queue.
 *
 * @tparam KEY    Key type. Must have default and copy constructors, and copy
 *                assignment.
 * @tparam VALUE  Value type. Must have default and copy constructors, and copy
 *                assignment.
 * @tparam COMP   Comparison function for keys. Keys increase from the front of
 *                the queue to the back.
 */
template<class KEY, class VALUE, class COMP = std::less<KEY>>
class MapQ
{
    using Map      = std::map<const KEY, VALUE, COMP>;

    Map  map;

public:
    /**
     * Constructs.
     *
     * @param[in] compare         Key comparison function
     * @throws std::system_error  Out of memory
     */
    MapQ(COMP& comp)
        : map(comp)
    {}

    /**
     * Returns the number of entries.
     *
     * @return  The number of entries
     */
    size_t size() const {
        return map.size();
    }

    /**
     * Indicates if this instance is empty (i.e., has no entries).
     *
     * @retval    `true`   This instance is empty
     * @retval    `false`  The instance is not empty
     */
    bool empty() const {
        return map.empty();
    }

    /**
     * Inserts an entry if it doesn't already exist. This is consonant with the
     * map aspect.
     *
     * @param[in] key      The key.
     * @param[in] value    The value.
     * @return             A pair whose first element is an iterator to the
     *                     possibly inserted key->value entry and whose second
     *                     element is a boolean that's true if and only if the
     *                     entry was actually created.
     */
    std::pair<typename Map::iterator, bool> insert(const KEY& key, VALUE& value) {
        return map.emplace(key, value);
    }

    /**
     * Returns, but doesn't remove, the entry whose key compares the least. This
     * is consonant with the queue aspect of a map-queue.
     *
     * @return  A pair whose first element is an iterator pointing to the first
     *          key->value entry if it exists and whose second element is a
     *          boolean whose value is true if the entry exists and false
     *          otherwise.
     * @see     `pop()`
     */
    std::pair<typename Map::const_iterator, bool> front() const {
        return {map.begin(), !map.empty()};
    }

    /**
     * Removes the entry whose key compares the least. Does nothing if the
     * map-queue is empty. This is consonant with the queue aspect of a
     * map-queue.
     *
     * @param[in,out] mq  Pointer to the map-queue
     * @see               `mq_front()`
     */
    void pop() {
        if (!map.empty())
            map.erase(map.begin());
    }
};
#endif

#endif /* MAP_QUEUE_H_ */
