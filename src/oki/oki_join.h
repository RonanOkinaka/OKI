#ifndef OKI_JOIN_H
#define OKI_JOIN_H

#include <algorithm>

namespace oki {
namespace intl_ {

enum class SeekKeyStatus
{
    STOP = -1,
    NEW_MAX = 0,
    CALL = 1
};

template <typename Key, typename IteratorPair>
intl_::SeekKeyStatus step_iter_pair(Key& max, IteratorPair& pair)
{
    pair.first = std::find_if_not(
        pair.first, pair.second, [=](const auto& kvPair) { return kvPair.first < max; });

    if (pair.first == pair.second) {
        return SeekKeyStatus::STOP;
    } else if (pair.first->first == max) {
        return SeekKeyStatus::CALL;
    } else /* pair.first->first > max */ {
        max = pair.first->first;
        return SeekKeyStatus::NEW_MAX;
    }
}

} // namespace intl_

template <typename Callback, typename... IteratorPairs>
Callback merge_join(Callback func, IteratorPairs... iterPairs)
{
    if (((iterPairs.first == iterPairs.second) || ...)) {
        return func;
    }

    // Get the first key
    auto max = [](const auto& pair0, const auto&...) { return pair0.first->first; }(iterPairs...);

    while (true) {
        // The following line is not UB
        // From 9.5.5.4 [dlc.init.list]: "a given initializer-clause is sequenced before every
        // [...] initializer-clause that follows it"
        const auto status = std::min({ intl_::step_iter_pair(max, iterPairs)... });

        if (status == intl_::SeekKeyStatus::STOP) {
            break;
        }
        if (status == intl_::SeekKeyStatus::CALL) {
            func(*iterPairs.first...);
            (++iterPairs.first, ...);
        }
    }

    return func;
}

}

#endif // OKI_JOIN_H
