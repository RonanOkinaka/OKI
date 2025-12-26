#ifndef OKI_FLAT_MAP_H
#define OKI_FLAT_MAP_H

#include <cstdint>
#include <flat_map>
#include <utility>

namespace oki {
namespace container {

template <typename Key, typename Type>
class FlatMap
{
private:
    std::flat_map<Key, Type> data_;

public:
    using key_type = Key;
    using mapped_type = Type;
    using iterator = decltype(data_)::iterator;
    using const_iterator = decltype(data_)::const_iterator;

    template <typename... Args>
    std::pair<iterator, bool> emplace(const Key key, Args&&... args)
    {
        return data_.try_emplace(key, std::forward<Args>(args)...);
    }

    template <typename InsertType>
    std::pair<iterator, bool> insert_or_assign(const Key key, InsertType&& value)
    {
        return data_.insert_or_assign(key, std::forward<InsertType>(value));
    }

    bool erase(const Key key) { return data_.erase(key); }

    const_iterator find(const Key key) const noexcept { return data_.find(key); }

    iterator find(const Key key) noexcept { return data_.find(key); }

    bool contains(const Key key) const noexcept { return data_.contains(key); }

    auto begin() { return data_.begin(); }
    auto cbegin() const { return data_.cbegin(); }
    auto end() { return data_.end(); }
    auto cend() const { return data_.cend(); }

    std::size_t size() const noexcept { return data_.size(); }
    void clear() noexcept { data_.clear(); }

    void reserve(const std::size_t n)
    {
        auto underlying = std::move(data_).extract();
        underlying.keys.reserve(n);
        underlying.values.reserve(n);
        data_.replace(std::move(underlying.keys), std::move(underlying.values));
    }
};

}
}

#endif // OKI_FLAT_MAP_H
