#include "oki/oki_handle.h"
#include "oki/oki_system.h"

#include "catch2/catch_test_macros.hpp"

#include <vector>

struct TestSystem : public oki::System
{
    void step(oki::SystemManager& manager, oki::SystemOptions& opts) override
    {
        ++numCalls;
    }

    unsigned int numCalls = 0;
};

TEST_CASE("SystemManager")
{
    oki::SystemManager sysMan;
    TestSystem system;

    auto handle = sysMan.add_priority_system(10, system);

    SECTION("can add a system")
    {
        REQUIRE_FALSE(oki::intl_::is_bad_handle(handle));
    }
    SECTION("can call a system's step()")
    {
        auto [exit, _] = sysMan.step();

        REQUIRE_FALSE(exit);
        REQUIRE(system.numCalls == 1);
    }
    SECTION("can add a functional system")
    {
        bool called = false;
        auto funcSys
            = oki::create_functional_system([&](auto&...) { called = true; });

        sysMan.add_system(*funcSys);
        sysMan.step();

        REQUIRE(called);
    }
    SECTION("runs systems in priority-order")
    {
        std::vector<int> callOrder;
        auto titled_func_sys = [&callOrder](int title) {
            return oki::create_functional_system(
                [=, &callOrder](auto&...) { callOrder.push_back(title); });
        };

        std::unique_ptr<oki::System> systems[] = {
            titled_func_sys(0),
            titled_func_sys(1),
            titled_func_sys(2),
            titled_func_sys(3),
            titled_func_sys(4),
            titled_func_sys(5),
            titled_func_sys(6),
        };

        sysMan.add_priority_system(10, *systems[0]);
        sysMan.add_priority_system(5, *systems[1]);
        sysMan.add_priority_system(15, *systems[2]);
        sysMan.add_priority_system(10, *systems[3]);
        sysMan.add_priority_system(10, *systems[4]);
        sysMan.add_priority_system(1, *systems[5]);
        sysMan.add_priority_system(20, *systems[6]);

        sysMan.step();
        REQUIRE(callOrder == std::vector<int> { 6, 2, 0, 3, 4, 1, 5 });
    }
    SECTION("can remove a system")
    {
        sysMan.step();
        REQUIRE(system.numCalls == 1);

        REQUIRE(sysMan.remove_system(handle));

        sysMan.step();
        REQUIRE(system.numCalls == 1);

        CHECK_FALSE(sysMan.remove_system(handle));
    }
    SECTION("halts when there are no systems")
    {
        REQUIRE(sysMan.remove_system(handle));

        // TODO: Find a timeout option for Catch2
        // (if this fails, we just stall indefinitely)
        sysMan.run();
        REQUIRE(true);
    }
    SECTION("can remove systems while running")
    {
        unsigned int numCalls = 0;
        auto funcSys = oki::create_functional_system(
            [&](oki::SystemManager& manager, oki::SystemOptions& opts) {
                ++numCalls;
                opts.remove_me();
                manager.remove_system(handle);
            });

        sysMan.add_priority_system(20, *funcSys);
        sysMan.run();

        REQUIRE(numCalls == 1);
        REQUIRE(system.numCalls == 0);
    }
    SECTION("can exit from run()")
    {
        auto funcSys = oki::create_functional_system(
            [](auto&, oki::SystemOptions& opts) { opts.exit(1); });

        sysMan.add_priority_system(20, *funcSys);

        REQUIRE(sysMan.run() == 1);
        CHECK(system.numCalls == 0);
    }
    SECTION("can skip other systems")
    {
        unsigned int counter = 0;

        auto funcSys = oki::create_functional_system(
            [&](auto&, oki::SystemOptions& opts) {
                if (counter == 5) {
                    opts.exit();
                    return;
                }

                ++counter;
                opts.skip_rest();
            });

        sysMan.add_priority_system(20, *funcSys);

        sysMan.run();

        CHECK(system.numCalls == 0);
        CHECK(counter == 5);
    }
    SECTION("can get inserted systems")
    {
        CHECK(&system == sysMan.get_system(handle));
    }
    SECTION("cannot get nonexistent systems")
    {
        sysMan.remove_system(handle);
        CHECK_FALSE(sysMan.get_system(handle));
    }
}
