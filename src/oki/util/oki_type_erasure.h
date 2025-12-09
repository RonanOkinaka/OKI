#ifndef OKI_TYPE_ERASURE_H
#define OKI_TYPE_ERASURE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

namespace oki {
namespace intl_ {
template <std::size_t Size, std::size_t Align>
class SmallBufStorage
{
public:
    template <typename Type>
    const Type* get_ptr() const
    {
        return std::launder(reinterpret_cast<const Type*>(&buf_));
    }

    template <typename Type>
    Type* get_ptr()
    {
        return const_cast<Type*>(std::as_const(*this).template get_ptr<Type>());
    }

    template <typename Type, typename... Args>
    void init(Args&&... args)
    {
        new (buf_) Type(std::forward<Args>(args)...);
    }

    template <typename Type>
    void destroy()
    {
        std::destroy_at(this->get_ptr<Type>());
    }

private:
    alignas(Align) std::byte buf_[Size];
};

class HeapStorage
{
public:
    template <typename Type>
    const Type* get_ptr() const
    {
        return static_cast<const Type*>(ptr_);
    }

    template <typename Type>
    Type* get_ptr()
    {
        return const_cast<Type*>(std::as_const(*this).template get_ptr<Type>());
    }

    template <typename Type, typename... Args>
    void init(Args&&... args)
    {
        ptr_ = new Type(std::forward<Args>(args)...);
    }

    template <typename Type>
    void destroy()
    {
        delete this->get_ptr<Type>();
    }

private:
    void* ptr_;
};

template <std::size_t Size, std::size_t Align>
class ErasedType
{
public:
    ErasedType() = default;

    ErasedType(const ErasedType<Size, Align>& that)
        : ErasedType()
    {
        this->copy_from(that);
    }

    ErasedType(ErasedType<Size, Align>&& that)
        : ErasedType()
    {
        this->move_from(std::move(that));
    }

    template <typename Type,
        std::enable_if_t<std::negation_v<std::is_same<std::decay_t<Type>, ErasedType<Size, Align>>>,
            int>
        = 0>
    ErasedType(Type&& value)
        : ErasedType()
    {
        using InnerType = std::decay_t<Type>;
        this->emplace<InnerType>(std::forward<Type>(value));
    }

    ~ErasedType() { this->reset(); }

    ErasedType<Size, Align>& operator=(ErasedType<Size, Align> that)
    {
        this->move_from(std::move(that));
        return *this;
    }

    template <typename Type, typename... Args>
    void emplace(Args&&... args)
    {
        this->reset();
        this->reinit_<Type>(std::forward<Args>(args)...);
    }

    void copy_from(const ErasedType<Size, Align>& that)
    {
        if (that.copy_) {
            (this->*that.copy_)(that);
        }
    }

    void move_from(ErasedType<Size, Align>&& that)
    {
        if (that.move_) {
            (this->*that.move_)(std::move(that));
        }
    }

    template <typename Type>
    const Type& get_as() const
    {
        if (!destroy_) {
            throw std::runtime_error("get_as() called on empty ErasedType");
        }

        if constexpr (is_sbo_<Type>()) {
            return *storage_.buf_.template get_ptr<Type>();
        } else {
            return *storage_.ptr_.template get_ptr<Type>();
        }
    }

    template <typename Type>
    Type& get_as()
    {
        return const_cast<Type&>(std::as_const(*this).template get_as<Type>());
    }

    void reset()
    {
        if (destroy_) {
            (this->*destroy_)();
            destroy_ = nullptr;
            copy_ = nullptr;
            move_ = nullptr;
        }
    }

private:
    // Storage data
    using BufferStorage = SmallBufStorage<Size, Align>;

    static_assert(std::is_trivial_v<BufferStorage>);
    static_assert(std::is_trivial_v<HeapStorage>);

    union
    {
        BufferStorage buf_;
        HeapStorage ptr_;
    } storage_;

    template <typename Type>
    constexpr static bool is_sbo_()
    {
        return sizeof(Type) <= Size && alignof(Type) <= Align;
    }

    template <typename Type, typename Function>
    void visit_storage_(Function func)
    {
        if constexpr (is_sbo_<Type>()) {
            func(storage_.buf_);
        } else {
            func(storage_.ptr_);
        }
    }

    // Type-erased destruction
    using DestroyFunc = void (ErasedType::*)();
    DestroyFunc destroy_ = nullptr;

    template <typename Type>
    void destroy_inner_()
    {
        this->visit_storage_<Type>([](auto& data) { data.template destroy<Type>(); });
    }

    // Type-erased construction
    using CopyConstruct = void (ErasedType::*)(const ErasedType&);
    CopyConstruct copy_ = nullptr;

    using MoveConstruct = void (ErasedType::*)(ErasedType&&);
    MoveConstruct move_ = nullptr;

    template <typename Type>
    void copy_inner_(const ErasedType& that)
    {
        if constexpr (std::is_copy_constructible_v<Type>) {
            this->reinit_<Type>(that.get_as<Type>());
        } else {
            throw std::logic_error("Only use copy_from() on copy-constructible types");
        }
    }

    template <typename Type>
    void move_inner_(ErasedType&& that)
    {
        if constexpr (std::is_move_constructible_v<Type>) {
            this->reinit_<Type>(std::move(that.get_as<Type>()));
        } else {
            throw std::logic_error("Only use move_from() on move-constructible types");
        }
    }

    // Initialization
    template <typename Type, typename... Args>
    void reinit_(Args&&... args)
    {
        static_assert(std::is_constructible_v<Type, Args...>);

        // First, destroy whatever we're holding
        this->reset();

        // Then, load the new values
        this->visit_storage_<Type>(
            [&](auto& data) { data.template init<Type>(std::forward<Args>(args)...); });

        destroy_ = &ErasedType<Size, Align>::destroy_inner_<Type>;
        copy_ = &ErasedType<Size, Align>::copy_inner_<Type>;
        move_ = &ErasedType<Size, Align>::move_inner_<Type>;
    }
};

template <typename Type>
using OptimalErasedType = oki::intl_::ErasedType<sizeof(Type), alignof(Type)>;

/*
 * An opaque class representing a type index for an associative map.
 * It is worth using this instead of std::type_index directly because
 * this can easily be replaced with a different mechanism (like some
 * pointer to a static template variable, for instance).
 */
class TypeIndex
{
public:
    std::size_t hash() const { return std::hash<std::type_index> {}(idx_); }

    bool operator==(const TypeIndex& that) const { return idx_ == that.idx_; }

    bool operator<(const TypeIndex& that) const { return idx_ < that.idx_; }

private:
    using IndexType = std::type_index;
    IndexType idx_;

    explicit TypeIndex(IndexType idx)
        : idx_(idx)
    {
    }

    template <typename T>
    friend TypeIndex get_type();
};

template <typename Type>
oki::intl_::TypeIndex get_type()
{
    return oki::intl_::TypeIndex { typeid(std::decay_t<Type>) };
}

template <typename Type>
oki::intl_::TypeIndex get_type(Type&& _)
{
    return oki::intl_::get_type<std::decay_t<Type>>();
}
}
}

namespace std {
template <>
struct hash<oki::intl_::TypeIndex>
{
    std::size_t operator()(const oki::intl_::TypeIndex& type) const { return type.hash(); }
};
}

#endif // OKI_TYPE_ERASURE_H
