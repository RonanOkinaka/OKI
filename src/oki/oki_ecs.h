#ifndef OKI_ECS_H
#define OKI_ECS_H

#include "oki/oki_component.h"
#include "oki/oki_observer.h"
#include "oki/oki_system.h"

#include <type_traits>

namespace oki
{
    class Engine
        : public oki::ComponentManager
        , public oki::SignalManager
        , public oki::SystemManager { };

    template <typename ChildClass = void>
    class EngineSystem
        : public oki::System
    {
    public:
        virtual ~EngineSystem() = default;

        virtual void step(oki::Engine&, oki::SystemOptions&) = 0;

    private:
        void step(oki::SystemManager& manager, oki::SystemOptions& opts)
            override
        {
            // Optional CRTP
            if constexpr (std::is_base_of_v<EngineSystem, ChildClass>)
            {
                static_cast<ChildClass*>(this)->step(
                    static_cast<oki::Engine&>(manager),
                    opts
                );
            }
            else
            {
                this->step(static_cast<oki::Engine&>(manager), opts);
            }
        }
    };

    using SimpleEngineSystem = oki::EngineSystem<>;
}

#endif // OKI_ECS_H
