#ifndef OKI_CONTAINER_CONCEPT_H
#define OKI_CONTAINER_CONCEPT_H

#include <concepts>
#include <cstdint>
#include <utility>

namespace oki {
namespace container {

template <typename Container>
concept ComponentStorageConcept = requires(
    Container container, typename Container::key_type key, typename Container::mapped_type value) {
    // EMPLACE: Take a key and arguments that can be forwarded to construct mapped_type
    {
        container.emplace(key, std::declval<typename Container::mapped_type>())
    } -> std::same_as<std::pair<typename Container::iterator, bool>>;

    // INSERT_OR_ASSIGN: Take a key and a value to either assign to an existing record or to create
    // a new one
    {
        container.insert_or_assign(key, value)
    } -> std::same_as<std::pair<typename Container::iterator, bool>>;

    // ERASE: Delete record with matching key, if one exists, and indicate whether the deletion
    // occurred
    { container.erase(key) } -> std::same_as<bool>;

    // FIND: Look for a key and returns either an iterator or const_iterator
    { container.find(key) } -> std::same_as<typename Container::iterator>;
    { std::as_const(container).find(key) } -> std::same_as<typename Container::const_iterator>;

    // CONTAINS: Return whether the key exists
    { container.contains(key) } -> std::same_as<bool>;

    // ITERATORS / BEGIN / END: Return an iterator the beginning or end of the record set
    { container.begin() } -> std::same_as<typename Container::iterator>;
    { container.cbegin() } -> std::same_as<typename Container::const_iterator>;
    { container.end() } -> std::same_as<typename Container::iterator>;
    { container.cend() } -> std::same_as<typename Container::const_iterator>;

    // SIZE: Returns the size of the container
    { container.size() } -> std::same_as<std::size_t>;

    // CLEAR: Clears the container
    { container.clear() } -> std::same_as<void>;

    // RESERVE: Reserves space in the container
    { container.reserve(std::declval<std::size_t>()) } -> std::same_as<void>;
};

}
}

#endif // OKI_CONTAINER_CONCEPT_H
