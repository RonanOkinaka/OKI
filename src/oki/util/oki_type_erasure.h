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
         *
         * This version in particular stores move-only types.
         */
        template <std::size_t Size, std::size_t Align>
        class MovableErasedType
        {
        public:
            MovableErasedType(MovableErasedType<Size, Align>&& that)
                : budgetVtable_(that.budgetVtable_)
            {
                this->vtable_dispatch_(&that, Operation::MOVE_CONSTR);
            }

            ~MovableErasedType()
            {
                this->vtable_dispatch_(nullptr, Operation::DESTROY);
            }

            /*
             * As an example of the above, there is no guarantee that the other
             * MovableErasedType<> holds a type that is equivalent to this one.
             * It falls on the caller to be sure they're correct.
             */
            MovableErasedType<Size, Align>& operator=(
                MovableErasedType<Size, Align>&& that)
            {
                this->vtable_dispatch_(&that, Operation::MOVE_ASSIGN);
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
             * This should be preferred over assigning the MovableErasedType<>.
             */
            template <typename Type, typename InsertType>
            void hold(InsertType&& value)
            {
                this->get_as<Type>() = std::forward<InsertType>(value);
            }

            /*
             * Move-assigns the value in the provided MovableErasedType<> to
             * the one in this instance.
             *
             * Should be preferred over assignment.
             */
            template <typename Type>
            void move_from(MovableErasedType<Size, Align>&& that)
            {
                if constexpr (is_small_buffered<Type>::value)
                {
                    this->hold<Type>(std::move(that.get_as<Type>()));
                }
                else
                {
                    std::swap(storage_.ptr_, that.storage_.ptr_);
                }
            }

            /*
             * Creates a MovableErasedType<> instace holding a value of type
             * Type, constructing one in-place by perfectly forwarding the
             * variadic arguments.
             */
            template <typename Type, typename... Args>
            static MovableErasedType<Size, Align> erase_type(Args&&... args)
            {
                MovableErasedType<Size, Align> ret;
                initialize_erased_<Type>(
                    ret,
                    erased_ops_no_copy_<Type>,
                    std::forward<Args>(args)...
                );

                return ret;
            }

        protected:
            using Operation = oki::intl_::helper_::AnyErasedOperations;

            template <typename Type>
            using is_small_buffered =
                std::conditional_t<
                    alignof(Type) <= Align && sizeof(Type) <= Size,
                    std::true_type,
                    std::false_type
                >;

            MovableErasedType() = default;

            template <typename Type, typename Erased, typename Func, typename... Args>
            static void initialize_erased_(Erased& erased, Func vtable, Args&&... args)
            {
                erased.template initalize_data_<Type>(std::forward<Args>(args)...);
                erased.budgetVtable_ = vtable;
            }

            void copy_vtable_(const MovableErasedType<Size, Align>& that)
            {
                budgetVtable_ = that.budgetVtable_;
            }

            void vtable_dispatch_(MovableErasedType<Size, Align>* that, Operation op)
            {
                (*this->budgetVtable_)(*this, that, op);
            }

            template <typename Type, typename... Args>
            void initalize_data_(Args&&... args)
            {
                if constexpr (is_small_buffered<Type>::value)
                {
                    new (storage_.buf_) Type{ std::forward<Args>(args)... };
                }
                else
                {
                    storage_.ptr_ = new Type{ std::forward<Args>(args)... };
                }
            }

            template <typename Type>
            void destroy_data_()
            {
                if constexpr (is_small_buffered<Type>::value)
                {
                    std::destroy_at(this->get_data_<Type>());
                }
                else
                {
                    delete this->get_data_<Type>();
                }
            }

        private:
            union Storage
            {
                alignas(Align) std::byte buf_[Size];
                void* ptr_;
            } storage_;

            // The name is a bit tongue-in-cheek but it gets the point across
            void (*budgetVtable_)(
                MovableErasedType<Size, Align>&,
                MovableErasedType<Size, Align>*,
                Operation
            );

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
            static void erased_ops_no_copy_(
                MovableErasedType<Size, Align>& self,
                MovableErasedType<Size, Align>* other,
                Operation op)
            {
                using ThisType = MovableErasedType<Size, Align>;

                switch (op)
                {
                case Operation::MOVE_CONSTR:
                    self.initalize_data_<Type>(
                        std::move(other->get_as<Type>())
                    );
                    break;
                case Operation::MOVE_ASSIGN:
                    self.move_from<Type>(
                        std::move(*other)
                    );
                    break;
                case Operation::DESTROY:
                    self.destroy_data_<Type>();
                    break;
                }
            }
        };

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
         *
         * This version stores copyable types.
         */
        template <std::size_t Size, std::size_t Align>
        class ErasedType
            : public MovableErasedType<Size, Align>
        {
        public:
            ErasedType(const ErasedType<Size, Align>& that)
            {
                this->copy_vtable_(that);
                this->vtable_dispatch_(
                    // We can cast the constness away as long as
                    // no data is written
                    const_cast<ErasedType*>(&that),
                    Operation::COPY_CONSTR
                );
            }

            ErasedType(ErasedType<Size, Align>&& that)
                : Base(std::move(that)) { }

            /*
             * As an example of the above, there is no guarantee that the other
             * ErasedType<> holds a type that is equivalent to this one. It falls
             * on the caller to be sure they're correct.
             */
            ErasedType<Size, Align>& operator=(const ErasedType<Size, Align>& that)
            {
                this->vtable_dispatch_(
                    const_cast<ErasedType*>(&that),
                    Operation::COPY_ASSIGN
                );
                return *this;
            }

            /*
             * As an example of the above, there is no guarantee that the other
             * ErasedType<> holds a type that is equivalent to this one. It falls
             * on the caller to be sure they're correct.
             */
            ErasedType<Size, Align>& operator=(
                ErasedType<Size, Align>&& that)
            {
                this->vtable_dispatch_(&that, Operation::MOVE_ASSIGN);
                return *this;
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
                this->template hold<Type>(that.template get_as<Type>());
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
                Base::template initialize_erased_<Type>(
                    ret,
                    erased_ops_<Type>,
                    std::forward<Args>(args)...
                );

                return ret;
            }

        private:
            using Base = MovableErasedType<Size, Align>;

            using Operation = typename Base::Operation;

            template <typename Type>
            static void erased_ops_(
                Base& baseSelf,
                Base* baseOther,
                Operation op)
            {
                // Downcast to self
                using ThisType = ErasedType<Size, Align>;
                auto& self = static_cast<ThisType&>(baseSelf);
                auto other = static_cast<ThisType*>(baseOther);

                switch (op)
                {
                case Operation::MOVE_CONSTR:
                    self.template initalize_data_<Type>(
                        std::move(other->template get_as<Type>())
                    );
                    break;
                case Operation::MOVE_ASSIGN:
                    self.template move_from<Type>(
                        std::move(*other)
                    );
                    break;
                case Operation::COPY_CONSTR:
                    self.template initalize_data_<Type>(
                        std::as_const(*other).template get_as<Type>()
                    );
                    break;
                case Operation::COPY_ASSIGN:
                    self.template copy_from<Type>(
                        std::as_const(*other)
                    );
                    break;
                case Operation::DESTROY:
                    self.template destroy_data_<Type>();
                    break;
                }
            }

            ErasedType() = default;
        };

        template <typename Type>
        using OptimalErasedType = std::conditional_t<
            std::is_copy_assignable_v<Type> && std::is_copy_constructible_v<Type>,
            oki::intl_::ErasedType<sizeof(Type), alignof(Type)>,
            oki::intl_::MovableErasedType<sizeof(Type), alignof(Type)>
        >;

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
