#ifndef OKI_SYSTEM_H
#define OKI_SYSTEM_H

#include "oki/oki_handle.h"
#include "oki/util/oki_handle_gen.h"
#include "oki/util/oki_type_erasure.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <type_traits>
#include <utility>

namespace oki {
using SystemPriority = std::uint16_t;

class SystemOptions;
class SystemManager;

class System
{
public:
    virtual ~System() = default;

    virtual void step(oki::SystemManager&, oki::SystemOptions&) = 0;
};

/*
 * Heap-allocates a functional system given a provided step() function.
 *
 * The caller is still responsible for owning this object (although it
 * can be leaked if it will last the entire lifetime of the program).
 */
template <typename StepFunction>
std::unique_ptr<System> create_functional_system(StepFunction&& callback)
{
    class FunctionalSystem : public oki::System
    {
    public:
        FunctionalSystem(StepFunction callback)
            : callback_(callback)
        {
        }

        void step(oki::SystemManager& man, oki::SystemOptions& opts) override
        {
            callback_(man, opts);
        }

    private:
        StepFunction callback_;
    };

    return std::unique_ptr<oki::System>(
        new FunctionalSystem { std::forward<StepFunction>(callback) });
}

class SystemOptions
{
public:
    bool will_skip() const noexcept { return loopChoice_ == SKIP; }

    void skip_rest() noexcept { loopChoice_ = SKIP; }

    std::pair<bool, int> exit_info() const noexcept
    {
        return { loopChoice_ == EXIT, exitCode_ };
    }

    void exit(int code = 0) noexcept
    {
        exitCode_ = code;
        loopChoice_ = EXIT;
    }

    bool will_remove() const noexcept { return shouldRemove_; }

    void remove_me() noexcept { shouldRemove_ = true; }

private:
    int exitCode_;

    enum
    {
        EXIT,
        SKIP,
        CONT
    } loopChoice_;

    bool shouldRemove_;

    SystemOptions()
        : exitCode_(0)
        , loopChoice_(CONT)
        , shouldRemove_(false)
    {
    }

    friend class SystemManager;
};

class SystemManager
{
public:
    SystemManager() = default;
    SystemManager(const SystemManager&) = delete;
    SystemManager(SystemManager&&) = default;
    ~SystemManager() = default;

    SystemManager& operator=(const SystemManager&) = delete;
    SystemManager& operator=(SystemManager&&) = default;

    /*
     * Adds a system to be run by the SystemManager with a given priority.
     *
     * Every time the SystemManager calls step(), systems with higher
     * priority will run before those with lower priority. For matching
     * values, a system that was added later will run last.
     *
     * Systems CAN be added during a step() or run(), and whether they will
     * be called "this" frame is dependent on their priority.
     *
     * The SystemManager DOES NOT own systems. References to added systems
     * must remain valid for their entire tenure with the SystemManager.
     *
     * Returns a handle which can be used to refer to this system for
     * deletion/access/etc.
     */
    template <typename SystemType>
    oki::Handle add_priority_system(
        oki::SystemPriority priority, SystemType& system)
    {
        static_assert(std::is_base_of_v<oki::System, SystemType>);

        SystemData sysData;
        sysData.system_ = std::addressof(system);
        sysData.handle_ = handleGen_.create_handle();
        sysData.priority_ = priority;

        // We want to insert this system after higher-priority sytems
        // and, if they match priorities, after its peers
        auto sysIter = std::find_if(systems_.begin(), systems_.end(),
            [=](const auto& elem) { return elem.priority_ < priority; });

        systems_.insert(sysIter, sysData);
        return sysData.handle_;
    }

    /*
     * Adds a new system with priority 0.
     */
    template <typename SystemType>
    oki::Handle add_system(SystemType& system)
    {
        return this->add_priority_system<SystemType>(0, system);
    }

    /*
     * Removes a system given its handle.
     *
     * This CAN occur during a run() a step().
     */
    bool remove_system(oki::Handle handle)
    {
        auto sysIter = this->seek_handle_(handle);

        if (sysIter != systems_.end()) {
            // Does not hard erase (could be occuring during iteration)
            sysIter->handle_ = oki::intl_::get_invalid_handle_constant();
            sysIter->system_ = nullptr;

            return true;
        }

        return false;
    }

    /*
     * Returns a pointer to the system referred to by provided handle.
     *
     * The SystemManager is not designed for this and it is not
     * recommended to call this often.
     */
    oki::System* get_system(oki::Handle handle)
    {
        auto sysIter = this->seek_handle_(handle);
        return (sysIter != systems_.end()) ? sysIter->system_ : nullptr;
    }

    /*
     * Runs a single step, calling the step() function of each associated
     * system exactly once. Respects priority.
     *
     * Returns two values indicating whether one of the systems has
     * requested to exit and with which code.
     */
    std::pair<bool, int> step()
    {
        for (auto sysIter = systems_.begin(); sysIter != systems_.end();) {
            // If this system was removed in the last pass,
            // hard erase it
            if (!sysIter->system_) {
                sysIter = systems_.erase(sysIter);
                continue;
            }

            oki::SystemOptions options;
            sysIter->system_->step(*this, options);

            if (options.will_remove()) {
                sysIter = systems_.erase(sysIter);
                continue;
            }

            if (options.will_skip()) {
                break;
            }

            auto exitPair = options.exit_info();
            if (exitPair.first) {
                return exitPair;
            }

            ++sysIter;
        }

        return { false, 0 };
    }

    /*
     * Calls step() repeatedly until all systems have been removed or
     * one of the systems has requested to exit.
     */
    int run()
    {
        // If all systems have been removed, exit
        while (!systems_.empty()) {
            auto [exit, code] = this->step();

            if (exit) {
                return code;
            }
        }

        return 0;
    }

private:
    struct SystemData
    {
        oki::System* system_;
        oki::Handle handle_;
        oki::SystemPriority priority_;
    };

    // This choice of data structure makes the implementation easier and
    // its cache misses should be acceptable because the virtual function
    // associated with an oki::System will miss anyway (+ quantity is low)
    std::list<SystemData> systems_;

    oki::intl_::DefaultHandleGenerator<oki::Handle> handleGen_;

    auto seek_handle_(oki::Handle handle) noexcept ->
        typename decltype(systems_)::iterator
    {
        // A linear search is fine, because in general there should
        // be very few systems
        return std::find_if(systems_.begin(), systems_.end(),
            [=](const auto& elem) { return elem.handle_ == handle; });
    }
};
}

#endif // OKI_SYSTEM_H
