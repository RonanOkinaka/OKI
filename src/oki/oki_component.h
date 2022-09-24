#include "oki/oki_handle.h"
#include "oki/util/oki_container.h"
#include "oki/util/oki_handle_gen.h"
#include "oki/util/oki_type_erasure.h"

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <unordered_map>

namespace oki
{
    /*
     * Opaque class representing an entity (the 'E' in ECS). This object is
     * provided by and used in conjunction with the ComponentManager to relate
     * components to each other, allowing composition.
     */
    class Entity
    {
    public:
        using HandleType = oki::Handle;

    private:
        HandleType handle_ = oki::intl_::get_invalid_handle_constant();

        friend class ComponentManager;
    };

    /*
     * Class responsible for storing components and relating them to entities
     * and, by extension, each other.
     *
     * This is the 'core' ECS behavior (it accounts for the data: 'E' and 'C',
     * and the 'S' is mostly handled by the caller because it is code).
     */
    class ComponentManager
    {
        using HandleType = oki::Entity::HandleType;

        template <typename Type>
        using Container = oki::intl_::AssocSortedVector<
            HandleType,
            std::decay_t<Type>
        >;

        using ErasedContainer = oki::intl_::OptimalErasedType<Container<long>>;

    public:
        ComponentManager() = default;
        ComponentManager(const ComponentManager&) = default;
        ComponentManager(ComponentManager&&) = default;
        ~ComponentManager() = default;

        /*
         * Creates and returns an entity with which one can add, remove
         * and retrieve components.
         *
         * This is how composition is achieved: doing so tells the
         * ComponentManager how different components relate to each other.
         */
        oki::Entity create_entity()
        {
            oki::Entity entity;
            entity.handle_ = handGen_.create_handle();

            return entity;
        }

        /*
         * Deletes the entity handle (potentially allowing reuse) but DOES NOT
         * erase the entities that were associated with this entity.
         *
         * (The ability to do so comes with significant cost due to the type
         * erasure and may be added in the future, but not now.)
         *
         * Returns whether the deletion was successful (which is a no-op
         * by default).
         */
        bool destroy_entity(oki::Entity entity)
        {
            return handGen_.destroy_handle(entity.handle_);
        }

        /*
         * Adds a component of type Type if one was NOT already bound to this
         * entity and returns std::pair, where .first is a reference to the new
         * component and .second indicates whether the insertion took place.
         *
         * Constructs the component in-place by forwarding the 0 or more
         * supplied arguments to the constructor of Type.
         */
        template <typename Type, typename... Args>
        std::pair<Type&, bool> emplace_component(oki::Entity entity, Args&&... args)
        {
            auto& cont = this->get_or_create_cont_<Type>();

            auto [valIter, success] = cont.emplace(
                entity.handle_,
                std::forward<Args>(args)...
            );

            return { valIter->second, success };
        }

        /*
         * Adds a component if one with same type was NOT already bound to this
         * entity, and returns std::pair where .first is a reference to the new
         * component and .second indicates whether the insertion took place.
         *
         * Deduces the component type and forwards the incoming value.
         */
        template <typename InsertType>
        auto bind_component(oki::Entity entity, InsertType&& value)
        {
            return this->emplace_component<std::decay_t<InsertType>>(
                entity,
                std::forward<InsertType>(value)
            );
        }

        /*
         * Guarantees that the entity has a component matching the incoming value
         * and type by either creating a new one or assigning the existing value.
         *
         * Returns a std::pair where .first is a reference to the component in
         * question and .second indicates whether the component is new (true)
         * or old (false).
         *
         * Deduces the component type and forwards the incoming value.
         */
        template <typename InsertType>
        auto bind_or_assign_component(oki::Entity entity, InsertType&& value)
        {
            using Type = std::decay_t<InsertType>;
            auto& cont = this->get_or_create_cont_<Type>();

            auto [valIter, success] = cont.insert_or_assign(
                entity.handle_,
                std::forward<InsertType>(value)
            );

            return std::pair<Type&, bool>{ valIter->second, success };
        }

        /*
         * In-place constructs a component of type Type bound to the provided
         * entity, assuming (without checking) that there is not already a
         * component of the same type bound.
         *
         * OKI does not support adding multiple components of the same
         * type to one entity and its behavior is not formally supported in
         * this state.
         *
         * Forwards the supplied arguments to the constructor of Type.
         */
        template <typename Type, typename... Args>
        Type& emplace_component_unchecked(oki::Entity entity, Args&&... args)
        {
            auto& cont = this->get_or_create_cont_<Type>();
            auto iter = cont.emplace_unchecked(
                entity.handle_,
                std::forward<Args>(args)...
            );

            return iter->second;
        }

        /*
         * Binds a component to an entity, assuming (without checking) that
         * there is not already a component of the same type bound.
         *
         * OKI does not support adding multiple components of the same
         * type to one entity and its behavior is not formally supported in
         * this state.
         *
         * Deduces the type and forwards the incoming value to the constructor.
         */
        template <typename InsertType>
        auto& bind_component_unchecked(oki::Entity entity, InsertType&& value)
        {
            return this->emplace_component_unchecked<std::decay_t<InsertType>>(
                entity,
                std::forward<InsertType>(value)
            );
        }

        /*
         * Attempts to unbind a component from the provided entity and
         * call its destructor, then returns whether or not a component
         * existed and was deleted.
         */
        template <typename Type>
        bool remove_component(oki::Entity entity)
        {
            return this->call_on_cont_checked_<Type, bool>([=](auto& container) {
                return container.erase(entity.handle_);
            }, false);
        }

        /*
         * Erases all components of a given type.
         */
        template <typename Type>
        void erase_components()
        {
            this->call_on_cont_checked_<Type>([](auto& container) {
                container.clear();
            });
        }

        /*
         * Erases all components.
         *
         * Invalidates any views received from get_component_view().
         */
        void erase_components()
        {
            data_.clear();
        }

        /*
         * Retrieves a reference to component of type Type from the provided
         * entity, assuming (without checking) that this entity has a component
         * of that type.
         *
         * Components are owned by the ComponentManager and this reference is
         * valid only until a new component of this type is added (very likely
         * longer but OKI does not formally support this).
         */
        template <typename Type>
        Type& get_component(oki::Entity entity)
        {
            return this->get_cont_<Type>().find(entity.handle_)->second;
        }

        /*
         * If the entity has a component of this type, returns a pointer thereto;
         * otherwise, returns nullptr.
         *
         * Components are owned by the ComponentManager and this pointer is
         * valid only until a new component of this type is added (very likely
         * longer but OKI does not formally support this).
         */
        template <typename Type>
        Type* get_component_checked(oki::Entity entity)
        {
            return this->call_on_cont_checked_<Type, Type*>([=](auto& container) {
                auto compIter = container.find(entity.handle_);

                return (compIter != container.end())
                     ? std::addressof(compIter->second)
                     : nullptr;
            }, nullptr);
        }

        /*
         * Syntactic sugar for getting multiple components from an entity.
         * (Looks good with structured bindings!)
         */
        template <typename... Types>
        std::tuple<Types&...> get_components(oki::Entity entity)
        {
            return std::tie(this->get_component<Types>(entity)...);
        }

        /*
         * Syntactic sugar for getting multiple components (checked) from
         * an entity.
         */
        template <typename... Types>
        std::tuple<Types*...> get_components_checked(oki::Entity entity)
        {
            return { this->get_component_checked<Types>(entity)... };
        }

        /*
         * Seeks a component of this type that is bound to provided entity
         * and returns whether one was found.
         */
        template <typename Type>
        bool has_component(oki::Entity entity) const noexcept
        {
            return this->call_on_cont_checked_<Type, bool>([=](auto& container) {
                return container.contains(entity.handle_);
            }, false);
        }

        /*
         * The primary reason to use an ECS architecture, this provides
         * the ability to iterate over entities' components.
         *
         * The input functor func() will be called with the following
         * parameters for each matching entity:
         *   - An oki::Entity representing the components' owner
         *   - A reference to each of the bound components whose types
         *      are specified in Types..., in the order provided
         */
        template <typename... Types, typename Callback>
        Callback for_each(Callback func)
        {
            /*
             * Design note: we could just use this->get_or_create_cont_()
             * instead but this is more efficient and has no suprise
             * allocations. Failing out of this function is "free".
             */
            [&](auto... contPtrs) {
                // If any containers are missing, exit early
                if ((!contPtrs || ...))
                {
                    return;
                }

                // Otherwise, proceed as usual
                this->component_intersection_(func, *contPtrs...);
            }(this->try_get_cont_<Types>()...);

            return func;
        }

        /*
         * Allocates enough space for n components of type Type.
         *
         * (Internal tip: creates the relevant container if it does not
         * already exist, which can optimize the first component bind of
         * type Type.)
         */
        template <typename Type>
        void reserve_components(std::size_t n)
        {
            auto& container = this->get_or_create_cont_<Type>();
            container.reserve(n);
        }

        /*
         * Returns the number of components of a given type.
         */
        template <typename Type>
        std::size_t num_components() const
        {
            return this->call_on_cont_checked_<Type, std::size_t>(
            [](auto& container) {
                return container.size();
            }, 0);
        }

        template <typename... Types>
        class ComponentView
        {
        public:
            ComponentView(const ComponentView&) noexcept = default;
            ComponentView(ComponentView&&) noexcept = default;
            ~ComponentView() noexcept = default;

            template <typename Callback>
            Callback for_each(Callback func)
            {
                std::apply([&](auto& cont, auto&... containers) {
                    component_intersection_(func, cont, containers...);
                }, containers_);

                return func;
            }

        private:
            std::tuple<Container<Types>&...> containers_;

            ComponentView(std::tuple<Container<Types>&...> containers)
                : containers_(containers) { }

            friend class oki::ComponentManager;
        };

        /*
         * Get a reusable way to iterate over a set of components. It is
         * designed to amortize the cost, so allocates and locates all
         * relevant containers upfront, and should perform better than
         * for_each() for each subsequent use.
         *
         * The returned object is unchecked and is only valid if:
         *  - The object it came from is still alive and in the same location
         *  - erase_components() [the non-template version] has not been called
         */
        template <typename... Types>
        ComponentView<Types...> get_component_view()
        {
            auto& c = this->get_or_create_cont_<int>();

            // std::unordered_map does not invalidate references so this is ok
            return ComponentView<Types...>(
                std::tie(this->get_or_create_cont_<Types>()...)
            );
        }

    private:
        std::unordered_map<
            oki::intl_::TypeIndex,
            ErasedContainer
        > data_;

        oki::intl_::DefaultHandleGenerator<oki::Entity::HandleType> handGen_;

        template <typename Type>
        Container<Type>& create_cont_()
        {
            auto [iter, _] = data_.emplace(
                oki::intl_::get_type<Type>(),
                ErasedContainer::erase_type<Container<Type>>()
            );

            return iter->second;
        }

        template <typename Type>
        Container<Type>& get_or_create_cont_()
        {
            auto iter = data_.find(oki::intl_::get_type<Type>());

            if (iter == data_.end())
            {
                iter = data_.emplace_hint(
                    iter,
                    oki::intl_::get_type<Type>(),
                    ErasedContainer::erase_type<Container<Type>>()
                );
            }

            return iter->second.template get_as<Container<Type>>();
        }

        template <typename Type>
        Container<Type>& get_cont_()
        {
            auto iter = data_.find(oki::intl_::get_type<Type>());

            return iter->second.template get_as<Container<Type>>();
        }

        template <typename Type>
        Container<Type>* try_get_cont_()
        {
            auto iter = data_.find(oki::intl_::get_type<Type>());

            return iter != data_.end()
                 ? &iter->second.template get_as<Container<Type>>()
                 : nullptr;
        }

        template <typename Type, typename ReturnType,
                  typename Callback, typename DefaultRet>
        ReturnType call_on_cont_checked_(Callback func, DefaultRet defaultValue) const
        {
            static_assert(std::is_convertible_v<DefaultRet, ReturnType>);

            auto contIter = data_.find(oki::intl_::get_type<Type>());
            if (contIter != data_.cend())
            {
                return func(contIter->second.template get_as<Container<Type>>());
            }

            return defaultValue;
        }

        template <typename Type, typename ReturnType,
                  typename Callback, typename DefaultRet>
        ReturnType call_on_cont_checked_(Callback func, DefaultRet defaultValue)
        {
            // Trying to do this with two const_casts and a std::as_const was worse
            // than just duplicating this code
            static_assert(std::is_convertible_v<DefaultRet, ReturnType>);

            auto contIter = data_.find(oki::intl_::get_type<Type>());
            if (contIter != data_.end())
            {
                return func(contIter->second.template get_as<Container<Type>>());
            }

            return defaultValue;
        }

        template <typename Type, typename Callback>
        void call_on_cont_checked_(Callback func)
        {
            this->call_on_cont_checked_<Type, int>([&](auto& container) -> int {
                func(container);
                return 0;
            }, 0);
        }

        template <typename Callback, typename... Containers>
        static void component_intersection_(Callback& func, Containers&... conts)
        {
            oki::intl_::variadic_set_intersection(
                [&](auto& val, auto&... vals) {
                    // Unfortunate oversight on my part
                    oki::Entity entity;
                    entity.handle_ = val.first;

                    func(entity, val.second, vals.second...);
                },
                std::make_pair(conts.begin(), conts.end())...
            );
        }
    };
}
