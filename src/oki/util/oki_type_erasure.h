#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <utility>

namespace oki
{
    namespace intl_
    {
        namespace helper_
        {
            enum class AnyErasedOperations
            {
                COPY_CONSTR,
                COPY_ASSIGN,
                MOVE_CONSTR,
                MOVE_ASSIGN,
                DESTROY
            };
        }

        /*
         * This is a class designed for a very specialized purpose: obscure
         * the type of an object whose type is known by the caller.
         * It is capable of small-buffer optimizing objects of a particular
         * size and alignment (making it perhaps useful for a heterogeneous
         * container of a template class with different instantiations?)
         *
         * Per the assumption that the caller *knows the type*, there is NO
         * TYPE CHECKING for ANY operation. It is optimized for performance and
         * is able to function without RTTI. Such is the benefit over std::any.
         */
        template <std::size_t Size, std::size_t Align>
        class ErasedType
        {
        public:
            ErasedType(const ErasedType<Size, Align>& that)
                : budgetVtable_(that.budgetVtable_)
            {
                this->budgetVtable_(*this, &that, Operation::COPY_CONSTR);
            }

            ErasedType(ErasedType<Size, Align>&& that)
                : budgetVtable_(that.budgetVtable_)
            {
                this->budgetVtable_(*this, &that, Operation::MOVE_CONSTR);
            }

            ~ErasedType()
            {
                this->budgetVtable_(*this, nullptr, Operation::DESTROY);
            }

            /*
             * As an example of the above, there is no guarantee that the other
             * ErasedType<> holds a type that is equivalent to this one. It falls
             * on the caller to be sure they're correct.
             */
            ErasedType<Size, Align>& operator=(const ErasedType<Size, Align>& that)
            {
                this->budgetVtable_(*this, &that, Operation::COPY_ASSIGN);
                return *this;
            }

            ErasedType<Size, Align>& operator=(ErasedType<Size, Align>&& that)
            {
                this->budgetVtable_(*this, &that, Operation::MOVE_ASSIGN);
                return *this;
            }

            /*
             * Returns a reference to the stored value, assuming (and not
             * checking whether) the caller is correct about the type.
             */
            template <typename Type>
            const Type& get_as() const
            {
                return *this->get_data_<Type>();
            }

            template <typename Type>
            Type& get_as()
            {
                return const_cast<Type&>(
                    std::as_const(*this).template get_as<Type>()
                );
            }

            /*
             * Takes a value matching the underlying type and assigns
             * it to the internal object with perfect forwarding.
             *
             * This should be preferred over assigning the ErasedType<>.
             */
            template <typename Type, typename InsertType>
            void hold(InsertType&& value)
            {
                this->get_as<Type>() = std::forward<InsertType>(value);
            }

            /*
             * Copy-assigns the value in the provided ErasedType<> to the
             * one in this instance.
             *
             * Should be preferred over assignment.
             */
            template <typename Type>
            void copy_from(const ErasedType<Size, Align>& that)
            {
                this->hold<Type>(that.get_as<Type>());
            }

            /*
             * Move-assigns the value in the provided ErasedType<> to the
             * one in this instance.
             *
             * Should be preferred over assignment.
             */
            template <typename Type>
            void move_from(ErasedType<Size, Align>&& that)
            {
                this->hold<Type>(std::move(that.get_as<Type>()));
            }

            /*
             * Creates an ErasedType<> instace holding a value of type Type,
             * constructing one in-place by perfectly forwarding the variadic
             * arguments.
             */
            template <typename Type, typename... Args>
            static ErasedType<Size, Align> erase_type(Args&&... args)
            {
                ErasedType<Size, Align> ret;

                if constexpr (is_small_buffered<Type>::value)
                {
                    new (ret.storage_.buf_) Type{ std::forward<Args>(args)... };
                }
                else
                {
                    ret.storage_.ptr_ = new Type{ std::forward<Args>(args)... };
                }
                ret.budgetVtable_ = erased_ops_<Type>;

                return ret;
            }

        private:
            union Storage
            {
                alignas(Align) std::byte buf_[Size];
                void* ptr_;
            } storage_;

            using Operation = oki::intl_::helper_::AnyErasedOperations;

            // The name is a bit tongue-in-cheek but it gets the point across
            void (*budgetVtable_)(
                ErasedType<Size, Align>&,
                const ErasedType<Size, Align>*,
                Operation
            );

            template <typename Type>
            using is_small_buffered =
                std::conditional_t<
                    alignof(Type) <= Align && sizeof(Type) <= Size,
                    std::true_type,
                    std::false_type
                >;

            template <typename Type>
            const Type* get_data_() const
            {
                if constexpr (is_small_buffered<Type>::value)
                {
                    return std::launder(
                        reinterpret_cast<const Type*>(
                            &storage_.buf_
                        )
                    );
                }
                else
                {
                    return static_cast<const Type*>(storage_.ptr_);
                }
            }

            template <typename Type>
            Type* get_data_()
            {
                return const_cast<Type*>(
                    std::as_const(*this).template get_data_<Type>()
                );
            }

            template <typename Type>
            static void erased_ops_(
                ErasedType<Size, Align>& self,
                const ErasedType<Size, Align>* other,
                Operation op)
            {
                if constexpr (is_small_buffered<Type>::value)
                {
                    switch (op)
                    {
                    case Operation::MOVE_CONSTR:
                        new (self.storage_.buf_) Type{
                            std::move(const_cast<Type&>(other->get_as<Type>()))
                        };
                        break;
                    case Operation::MOVE_ASSIGN:
                        self.move_from<Type>(
                            std::move(*const_cast<ErasedType<Size, Align>*>(other))
                        );
                        break;
                    case Operation::COPY_CONSTR:
                        new (self.storage_.buf_) Type{
                            other->get_as<Type>()
                        };
                        break;
                    case Operation::COPY_ASSIGN:
                        self.copy_from<Type>(*other);
                        break;
                    case Operation::DESTROY:
                        std::destroy_at(self.get_data_<Type>());
                        break;
                    }
                }
                else
                {
                    switch (op)
                    {
                    case Operation::MOVE_CONSTR:
                    case Operation::MOVE_ASSIGN:
                        std::swap(
                            self.storage_.ptr_,
                            const_cast<ErasedType<Size, Align>*>(other)->storage_.ptr_
                        );
                        break;
                    case Operation::COPY_CONSTR:
                        self.storage_.ptr_ = new Type{
                            other->get_as<Type>()
                        };
                        break;
                    case Operation::COPY_ASSIGN:
                        self.copy_from<Type>(*other);
                        break;
                    case Operation::DESTROY:
                        delete self.get_data_<Type>();
                        break;
                    }
                }
            }

            ErasedType() = default;
        };

        template <typename Type>
        using OptimalErasedType = ErasedType<sizeof(Type), alignof(Type)>;

        /*
         * An opaque class representing a type index for an associative map.
         * It is worth using this instead of std::type_index directly because
         * this can easily be replaced with a different mechanism (like some
         * pointer to a static template variable, for instance).
         */
        class TypeIndex
        {
        public:
            std::size_t hash() const
            {
                return std::hash<std::type_index>{ }(idx_);
            }

            bool operator==(const TypeIndex& that) const
            {
                return idx_ == that.idx_;
            }

            bool operator<(const TypeIndex& that) const
            {
                return idx_ < that.idx_;
            }

        private:
            using IndexType = std::type_index;
            IndexType idx_;

            explicit TypeIndex(IndexType idx)
                : idx_(idx) { }

            template <typename T>
            friend TypeIndex get_type();
        };

        template <typename Type>
        oki::intl_::TypeIndex get_type()
        {
            return oki::intl_::TypeIndex{ typeid(std::decay_t<Type>) };
        }

        template <typename Type>
        oki::intl_::TypeIndex get_type(Type&& _)
        {
            return oki::intl_::get_type<std::decay_t<Type>>();
        }
    }
}

namespace std
{
    template <>
    struct hash<oki::intl_::TypeIndex>
    {
        std::size_t operator()(const oki::intl_::TypeIndex& type) const
        {
            return type.hash();
        }
    };
}
