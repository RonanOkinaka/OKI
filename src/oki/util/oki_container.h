#ifndef OKI_CONTAINER_H
#define OKI_CONTAINER_H

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace oki
{
    namespace intl_
    {
        /*
         * This is an implementation of one the simplest associative containers:
         * the sorted array.
         * One of the largest performance benefits of ECS is the cache-locality
         * gains reaped from iterating primarily over contiguous data, making the
         * vector a natural choice.
         *
         * Its performance characteristics are as follows:
         *   - Fast, cache-local iteration
         *   - Slow O(n) insertion [but amortized O(1) for keys that only increase!]
         *   - Moderate O(log n) retrieval [though this can be optimized with a
         *       reverse index as is done in entt's sparse_set]
         *
         * Overall, this is a very simple associative container that should work well
         * with this particular library's setup.
         */
        template <typename Key, typename Type>
        class AssocSortedVector
        {
        public:
            AssocSortedVector() noexcept = default;
            AssocSortedVector(const AssocSortedVector&) = default;
            AssocSortedVector(AssocSortedVector&&) noexcept = default;
            ~AssocSortedVector() noexcept = default;

            AssocSortedVector& operator=(const AssocSortedVector&) = default;
            AssocSortedVector& operator=(AssocSortedVector&&) noexcept = default;

            using DataType = std::vector<std::pair<Key, Type>>;
            using key_type = Key;
            using mapped_type = Type;
            using value_type = typename DataType::value_type;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using allocator_type = typename DataType::allocator_type;
            using reference = value_type&;
            using const_reference = const value_type&;
            using iterator = typename DataType::iterator;
            using const_iterator = typename DataType::const_iterator;

            /*
             * Inserts a new key-value pair into the container.
             *
             * Takes a key and variadic arguments to some type(s) that must be able
             * to construct a mapped_type.
             *
             * Either inserts and returns an iterator to the newly inserted pair + a
             * 'true' value, or returns an iterator the old pair and a 'false' value.
             */
            template <typename... Args>
            std::pair<iterator, bool> emplace(Key key, Args&&... args)
            {
                // Equivalent behavior to find_key_maybe_max_() -> try to skip
                // the binary search by checking the highly likely case that
                // the key is maximal, but skips one extra branch this way
                auto begin = data_.begin(), end = data_.end();
                if (begin == end || data_.back().first < key)
                {
                    return {
                        data_.emplace(data_.end(),
                            std::piecewise_construct,
                            std::tuple{ key },
                            std::forward_as_tuple(std::forward<Args>(args)...)
                        ),
                        true
                    };
                }

                return this->try_insert_impl_<false>(
                    key,
                    std::forward_as_tuple(std::forward<Args>(args)...)
                );
            }

            /*
             * Inserts a new key-value pair into the container.
             *
             * Takes a key and a universal reference to some type that must be able
             * to construct a mapped_type.
             *
             * Either inserts and returns an iterator to the newly inserted pair + a
             * 'true' value, or returns an iterator the old pair and a 'false' value.
             */
            template <typename InsertType>
            std::pair<iterator, bool> insert(Key key, InsertType&& value)
            {
                return this->emplace(key, std::forward<InsertType>(value));
            }

            /*
             * Guarantees that a pair with key value <key> holds the value <value>.
             *
             * Takes a key and a universal reference to some type that must be able
             * to both construct and assign to a value_type.
             *
             * Either inserts and returns an iterator to the newly inserted pair + a
             * 'true' value, or returns an iterator the old pair and a 'false' value.
             */
            template <typename InsertType>
            std::pair<iterator, bool> insert_or_assign(Key key, InsertType&& value)
            {
                return this->try_insert_impl_<true>(
                    key,
                    std::forward_as_tuple(std::forward<InsertType>(value))
                );
            }

            /*
             * Emplaces a key-value pair under the assumption that no item with
             * that <key> already exists in the container. Does not check.
             *
             * Returns an iterator to the newly inserted pair.
             */
            template <typename... Args>
            iterator emplace_unchecked(Key key, Args&&... args)
            {
                static_assert(std::is_constructible_v<Type, Args...>);

                return data_.emplace(this->find_key_maybe_max_(key),
                    key,
                    std::forward<Args>(args)...
                );
            }

            /*
             * Inserts a key-value pair under the assumption that no item with
             * that <key> already exists in the container. Does not check.
             *
             * Returns an iterator to the newly inserted pair.
             */
            template <typename InsertType>
            iterator insert_unchecked(Key key, InsertType&& value)
            {
                return this->emplace_unchecked(key, std::forward<InsertType>(value));
            }

            /*
             * Attempts to erase a pair with key <key>. Does nothing if <key> is not present.
             */
            bool erase(Key key)
            {
                auto iter = this->find_key_(key);

                if (!this->check_key_iter_(key, iter))
                {
                    return false;
                }

                data_.erase(iter);
                return true;
            }

            /*
             * Attempts to locate a const_iterator to a pair with key <key>.
             *
             * Returns this->cend() if the key is not present.
             */
            const_iterator find(Key key) const noexcept
            {
                auto iter = this->find_key_(key);
                return this->check_key_iter_(key, iter) ? iter : this->cend();
            }

            /*
             * Attempts to locate an iterator to a pair with key <key>.
             *
             * Returns this->end() if the key is not present.
             */
            iterator find(Key key) noexcept
            {
                auto iter = std::as_const(*this).find(key);
                auto ret = data_.begin();
                std::advance(ret, std::distance<const_iterator>(ret, iter));

                return ret;
            }

            /*
             * Attempts to locate a pair with key <key>.
             *
             * Returns a boolean indicating whether the key was present.
             */
            bool contains(Key key) const noexcept
            {
                return this->check_key_iter_(key, this->find_key_(key));
            }

            auto begin() { return data_.begin(); }
            auto cbegin() const { return data_.cbegin(); }
            auto end() { return data_.end(); }
            auto cend() const { return data_.cend(); }

            std::size_t size() const noexcept { return data_.size(); }
            void clear() noexcept { data_.clear(); }
            void reserve(std::size_t n) { data_.reserve(n); }

        private:
            DataType data_;

            const_iterator find_key_(Key key, const_iterator begin, const_iterator end) const
            {
                return std::lower_bound(begin, end, key,
                [](const auto& kvPair, auto key) {
                    return kvPair.first < key;
                });
            }

            iterator find_key_(Key key, iterator begin, iterator end)
            {
                auto iter = std::as_const(*this).find_key_(key, begin, end);
                std::advance(begin, std::distance<const_iterator>(begin, iter));

                return begin;
            }

            // Attempts to locate a key with a high chance that the key in
            // question is maximal
            iterator find_key_maybe_max_(Key key)
            {
                auto begin = data_.begin(), end = data_.end();

                // Special case (often an optimization): try to skip
                // binary search if the key is known to be maximal
                if (begin == end || data_.back().first < key)
                {
                    return end;
                }

                return this->find_key_(key, begin, end);
            }

            const_iterator find_key_(Key key) const
            {
                return this->find_key_(key, data_.begin(), data_.end());
            }

            iterator find_key_(Key key)
            {
                return this->find_key_(key, data_.begin(), data_.end());
            }

            bool check_key_iter_(Key key, const_iterator iter) const
            {
                return iter != data_.end()
                    && iter->first == key;
            }

            template <bool ASSIGN, typename... Args>
            std::pair<iterator, bool> try_insert_impl_(
                Key key,
                std::tuple<Args...>&& args)
            {
                static_assert(std::is_constructible_v<Type, Args...>);

                auto iter = this->find_key_(key);
                if (this->check_key_iter_(key, iter))
                {
                    // If we already have the key, DO NOT insert
                    if constexpr (ASSIGN)
                    {
                        static_assert(sizeof...(Args) == 1);
                        static_assert(std::is_assignable_v<
                            Type&,
                            std::tuple_element_t<0, std::decay_t<decltype(args)>>
                        >);

                        iter->second = std::get<0>(std::move(args));
                    }

                    return { iter, false };
                }

                // Otherwise, DO insert
                return {
                    data_.emplace(iter,
                        std::piecewise_construct,
                        std::tuple{ key },
                        std::move(args)
                    ),
                    true
                };
            }
        };

        namespace helper_
        {
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
                pair.first = std::find_if_not(pair.first, pair.second,
                    [=](const auto& kvPair) {
                        return kvPair.first < max;
                    }
                );

                if (pair.first == pair.second)
                {
                    return Status::STOP;
                }
                else if (pair.first->first == max)
                {
                    return Status::CALL;
                }
                else // pair.first > max
                {
                    max = pair.first->first;
                    return Status::NEW_MAX;
                }
            }
        }

        template <typename Callback, typename... IteratorPairs>
        Callback variadic_set_intersection(Callback func, IteratorPairs... iterPairs)
        {
            namespace helper = oki::intl_::helper_;

            // This is essentially the merge join algorithm, optimized for cache-coherence
            auto max = helper::get_first_key(iterPairs...);
            while (true)
            {
                helper::Status status = std::min({ helper::step_iter_pair(max, iterPairs)... });

                if (status == helper::Status::STOP)
                {
                    return func;
                }
                if (status == helper::Status::CALL)
                {
                    func(*iterPairs.first...);
                    (++iterPairs.first, ...);
                }
            }

            return func;
        }
    }
}

#endif // OKI_CONTAINER_H
