#include "oki/oki_component.h"

#include "oki_test_util.h"

#include "catch2/catch_test_macros.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <set>

using Value = test_helper::ObjHelper;
using TestType = oki::ComponentManager;

TEST_CASE("ComponentManager")
{
    oki::ComponentManager compMan;
    auto entity = compMan.create_entity();

    SECTION("can add and retrieve component")
    {
        auto [comp, success] = compMan.bind_component(entity, 0);

        REQUIRE(comp == 0);
        REQUIRE(success);

        REQUIRE(compMan.get_component<int>(entity) == 0);
        REQUIRE(*compMan.get_component_checked<int>(entity) == 0);
        REQUIRE(compMan.has_component<int>(entity));
    }
    SECTION("rejects already present components in bind_component()")
    {
        CHECK(compMan.bind_component(entity, 0).second);

        auto [comp, success] = compMan.bind_component(entity, 1);

        REQUIRE(comp == 0);
        REQUIRE_FALSE(success);
    }
    SECTION("can update component with reference from bind_component()")
    {
        compMan.bind_component(entity, 0);

        auto [comp, success] = compMan.bind_component(entity, 1);
        comp = 2;

        CHECK(compMan.get_component<int>(entity) == 2);
    }
    SECTION("can update component with reference from bind_component_unchecked()")
    {
        compMan.bind_component(entity, 0);

        auto& comp = compMan.bind_component_unchecked(entity, 1);
        comp = 2;

        CHECK(compMan.get_component<int>(entity) == 2);
    }
    SECTION("can update component with get_component()")
    {
        compMan.bind_component(entity, 0);

        auto& intComp = compMan.get_component<int>(entity);
        intComp = 2;

        int* intPtr = compMan.get_component_checked<int>(entity);
        REQUIRE((intPtr && *intPtr == 2));

        *intPtr = 3;
        REQUIRE(intComp == 3);
    }
    SECTION("can update component with bind_or_assign_component()")
    {
        compMan.bind_component(entity, 0);
        compMan.bind_or_assign_component(entity, 1);

        REQUIRE(compMan.get_component<int>(entity) == 1);
    }
    SECTION("can bind multiple components to one entity")
    {
        compMan.bind_component(entity, 0);
        compMan.bind_component(entity, 1.5f);
        compMan.bind_component(entity, std::string{ "wowie" });

        REQUIRE(compMan.get_component<int>(entity) == 0);
        REQUIRE(compMan.get_component<float>(entity) == 1.5);
        REQUIRE(compMan.get_component<std::string>(entity) == "wowie");

        REQUIRE(compMan.has_component<int>(entity));
        REQUIRE(compMan.has_component<float>(entity));
        REQUIRE(compMan.has_component<std::string>(entity));
    }
    SECTION("can bind components to multiple entities")
    {
        auto entity2 = compMan.create_entity();

        compMan.bind_component(entity, 0);
        compMan.bind_component(entity2, 1);

        REQUIRE(compMan.get_component<int>(entity) == 0);
        REQUIRE(compMan.get_component<int>(entity2) == 1);

        REQUIRE(compMan.has_component<int>(entity));
        REQUIRE(compMan.has_component<int>(entity2));
    }
    SECTION("can retrieve and update multiple components at once")
    {
        compMan.bind_component(entity, 0);
        compMan.bind_component(entity, 1.5f);
        compMan.bind_component(entity, std::string{ "wowie" });

        auto [i, f, s] = compMan.get_components<
            int, float, std::string
        >(entity);

        REQUIRE(i == 0);
        REQUIRE(f == 1.5f);
        REQUIRE(s == "wowie");

        i = 2;
        f = 4.f;
        s = "wowza";

        REQUIRE(compMan.get_component<int>(entity) == 2);
        REQUIRE(compMan.get_component<float>(entity) == 4.f);
        REQUIRE(compMan.get_component<std::string>(entity) == "wowza");
    }
    SECTION("can get a mixture of present and not present components")
    {
        auto entity2 = compMan.create_entity();
        compMan.bind_component(entity2, 'z');

        compMan.bind_component(entity, 0);
        compMan.bind_component(entity, 1.5f);

        auto [i, c, f, s] = compMan.get_components_checked<
            int, char, float, std::string
        >(entity);

        REQUIRE((i && *i == 0));
        REQUIRE_FALSE(c);
        REQUIRE((f && *f == 1.5));
        REQUIRE_FALSE(s);
    }
    SECTION("rejects absent component [container is missing]")
    {
        CHECK_FALSE(compMan.get_component_checked<int>(entity));
        CHECK_FALSE(compMan.has_component<int>(entity));
    }
    SECTION("rejects absent component [entity does not have this type]")
    {
        auto entity2 = compMan.create_entity();
        compMan.bind_component(entity2, 1);

        CHECK_FALSE(compMan.get_component_checked<int>(entity));
        CHECK_FALSE(compMan.has_component<int>(entity));
    }
    SECTION("can remove present component")
    {
        compMan.bind_component(entity, 1);

        REQUIRE(compMan.remove_component<int>(entity));

        REQUIRE_FALSE(compMan.get_component_checked<int>(entity));
        REQUIRE_FALSE(compMan.has_component<int>(entity));
    }
    SECTION("does not remove absent component [container is missing]")
    {
        CHECK_FALSE(compMan.remove_component<int>(entity));
    }
    SECTION("does not remove absent component [entity does not have this type]")
    {
        auto entity2 = compMan.create_entity();
        compMan.bind_component(entity2, 1);

        CHECK_FALSE(compMan.remove_component<int>(entity));

        auto comp2 = compMan.get_component_checked<int>(entity2);
        CHECK((comp2 && *comp2 == 1));
    }
    SECTION("can remove all elements of type")
    {
        auto entity2 = compMan.create_entity();
        compMan.bind_component(entity, 1);
        compMan.bind_component(entity2, 2);

        compMan.erase_components<int>();

        REQUIRE_FALSE(compMan.get_component_checked<int>(entity));
        REQUIRE_FALSE(compMan.get_component_checked<int>(entity2));

        REQUIRE_FALSE(compMan.has_component<int>(entity));
        REQUIRE_FALSE(compMan.has_component<int>(entity2));
    }
    SECTION("can iterate over and update one component type")
    {
        constexpr unsigned NUM_VALS = 15;

        int expectedVals[NUM_VALS] = { 0 };
        for (unsigned i = 0; i != NUM_VALS; ++i)
        {
            unsigned value = i * 2;

            compMan.bind_component(compMan.create_entity(), value);
            expectedVals[i] = value;
        }

        std::set<int> values;
        compMan.for_each<unsigned>([&](const oki::Entity ent, unsigned& val) {
            values.insert(val);
            val = 0;
        });

        REQUIRE(std::equal(values.begin(), values.end(), expectedVals));

        compMan.for_each<unsigned>([&](const oki::Entity ent, const unsigned val) {
            REQUIRE(val == 0);
        });
    }
    SECTION("can iterate over several component types")
    {
        auto e1 = compMan.create_entity();
        auto e2 = compMan.create_entity();
        auto e3 = compMan.create_entity();
        auto e4 = compMan.create_entity();

        compMan.bind_component(e1, 1);
        compMan.bind_component(e1, 1.f);
        compMan.bind_component(e1, '1');

        compMan.bind_component(e2, 2);
        compMan.bind_component(e2, '2');

        compMan.bind_component(e3, 3.f);
        compMan.bind_component(e3, '3');
        compMan.bind_component(e3, 3ull);

        compMan.bind_component(e4, 4);
        compMan.bind_component(e4, 4.f);
        compMan.bind_component(e4, '4');

        {
            std::set<int> values;
            compMan.for_each<int, float, char>([&](auto ent, int i, auto...) {
                values.insert(i);
            });

            int expectedVals[2] = { 1, 4 };
            REQUIRE(values.size() == 2);
            REQUIRE(std::equal(values.begin(), values.end(), expectedVals));
        }
        {
            std::set<int> values;
            compMan.for_each<int, char>([&](auto ent, int i, auto...) {
                values.insert(i);
            });

            int expectedVals[4] = { 1, 2, 4 };
            REQUIRE(values.size() == 3);
            REQUIRE(std::equal(values.begin(), values.end(), expectedVals));
        }
        {
            std::set<unsigned long long> values;
            compMan.for_each<unsigned long long>([&](auto ent, unsigned long long i) {
                values.insert(i);
            });

            unsigned long long expectedVals[1] = { 3 };
            REQUIRE(std::equal(values.begin(), values.end(), expectedVals));
        }
    }
    SECTION("can check missing containers in for_each()")
    {
        compMan.for_each<int>([](auto...) {
            // This should never be called (no components)
            REQUIRE(false);
        });
    }
    SECTION("reserve_components() does not increase num_components()")
    {
        compMan.reserve_components<int>(10);

        CHECK_FALSE(compMan.num_components<int>());
    }
    SECTION("reserve_components() does not decrease num_components()")
    {
        compMan.bind_component(entity, 0);
        compMan.reserve_components<int>(0);

        CHECK(compMan.num_components<int>() == 1);
        CHECK(compMan.has_component<int>(entity));
    }
    SECTION("can destroy an entity")
    {
        // Basically does nothing at the moment
        CHECK(compMan.destroy_entity(entity));
    }

    SECTION("(lifetime management)")
    {
        Value::reset();

        auto test_lifetime =
        [](auto memFunc, std::size_t value, auto constr, auto copies, auto moves)
        {
            {
                oki::ComponentManager tempCompMan;
                auto ent = tempCompMan.create_entity();

                memFunc(tempCompMan, ent);

                REQUIRE(tempCompMan.get_component<Value>(ent).value_ == value);
            }

            Value::test(constr, copies, moves);
        };

        SECTION("can insert and retrieve components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                CHECK(manager.bind_component(entity, Value{ 1 }).second);
            }, 1, 2, 0, 1);
        }
        SECTION("can emplace components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                CHECK(manager.emplace_component<Value>(entity, 1u).second);
            }, 1, 1, 0, 0);
        }
        SECTION("can default construct components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                CHECK(manager.emplace_component<Value>(entity).second);
            }, 0, 1, 0, 0);
        }
        SECTION("can move insert components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                Value value{ 1 };
                manager.bind_component(entity, std::move(value));
            }, 1, 2, 0, 1);
        }
        SECTION("can copy insert components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                Value value{ 1 };
                manager.bind_component(entity, value);
            }, 1, 2, 1, 0);
        }
        SECTION("can move assign components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                manager.emplace_component<Value>(entity);
                auto [comp, success] = manager.bind_or_assign_component(
                    entity,
                    Value{ 1 }
                );

                CHECK_FALSE(success);
            }, 1, 2, 0, 1);
        }
        SECTION("can copy assign components")
        {
            test_lifetime([](TestType& manager, oki::Entity entity) {
                manager.emplace_component<Value>(entity);

                Value value{ 1 };
                auto [comp, success] = manager.bind_or_assign_component(
                    entity,
                    value
                );

                CHECK_FALSE(success);
            }, 1, 2, 1, 0);
        }
        SECTION("calls destructor on removed components")
        {
            {
                auto manager = TestType();
                auto entity = manager.create_entity();

                manager.emplace_component<Value>(entity);
                manager.remove_component<Value>(entity);

                CHECK(Value::numConstructs == 1);
                CHECK(Value::numDestructs == 1);
            }

            Value::test();
        }
        SECTION("calls destructor on erased components")
        {
            {
                auto manager = TestType();
                auto entity = manager.create_entity();

                manager.emplace_component<Value>(entity);
                manager.erase_components<Value>();

                CHECK(Value::numConstructs == 1);
                CHECK(Value::numDestructs == 1);
            }

            Value::test();
        }
        SECTION("calls destructor on erased components (type erased)")
        {
            {
                auto manager = TestType();
                auto entity = manager.create_entity();

                manager.emplace_component<Value>(entity);
                manager.erase_components();

                CHECK(Value::numConstructs == 1);
                CHECK(Value::numDestructs == 1);
            }

            Value::test();
        }
    }
}
