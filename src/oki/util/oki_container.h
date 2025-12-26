#ifndef OKI_CONTAINER_H
#define OKI_CONTAINER_H

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace oki {
namespace intl_ {
namespace helper_ {

template <typename IteratorPair, typename... IteratorPairs>
auto get_first_key(IteratorPair pair, IteratorPairs... pairs)
{
    return pair.first->first;
}

enum class Status
{
    STOP = -1,
    NEW_MAX = 0,
    CALL = 1
};

template <typename Key, typename IteratorPair>
oki::intl_::helper_::Status step_iter_pair(Key& max, IteratorPair& pair)
{
    pair.first = std::find_if_not(
        pair.first, pair.second, [=](const auto& kvPair) { return kvPair.first < max; });

    if (pair.first == pair.second) {
        return Status::STOP;
    } else if (pair.first->first == max) {
        return Status::CALL;
    } else // pair.first > max
    {
        max = pair.first->first;
        return Status::NEW_MAX;
    }
}

} // namespace helper_

template <typename Callback, typename... IteratorPairs>
Callback variadic_set_intersection(Callback func, IteratorPairs... iterPairs)
{
    namespace helper = oki::intl_::helper_;

    if (((iterPairs.first == iterPairs.second) || ...)) {
        return func;
    }

    // This is essentially the merge join algorithm, optimized for
    // cache-coherence
    auto max = helper::get_first_key(iterPairs...);
    while (true) {
        helper::Status status = std::min({ helper::step_iter_pair(max, iterPairs)... });

        if (status == helper::Status::STOP) {
            return func;
        }
        if (status == helper::Status::CALL) {
            func(*iterPairs.first...);
            (++iterPairs.first, ...);
        }
    }

    return func;
}

}
}

#endif // OKI_CONTAINER_H
