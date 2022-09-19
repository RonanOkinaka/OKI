#include "oki/oki_handle.h"
#include "oki/util/oki_container.h"

#include "oki_test_util.h"

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <type_traits>
#include <map>
#include <utility>
#include <vector>

TEST_CASE("AssocSortedVector", "[logic][ecs][container]")
{
    oki::intl_::AssocSortedVector<oki::Handle, std::string> map;
    map.insert(2, "2");

    auto test_insertion = [&map](auto memFunc, const std::string& funcName)
    {
        SECTION("allows " + funcName + "() at front")
        {
            auto [iter, success] = memFunc(map, 1, "1");

            REQUIRE(success);
            REQUIRE(iter->first == 1);
            REQUIRE(iter->second == "1");
        }
        SECTION("allows " + funcName + "() at end")
        {
            auto [iter, success] = memFunc(map, 3, "3");

            REQUIRE(success);
            REQUIRE(iter->first == 3);
            REQUIRE(iter->second == "3");
        }
        SECTION("allows " + funcName + "() at center")
        {
            map.insert(1, "1");
            map.insert(4, "4");
            auto [iter, success] = memFunc(map, 3, "3");

            REQUIRE(success);
            REQUIRE(iter->first == 3);
            REQUIRE(iter->second == "3");
        }
    };

    test_insertion(std::mem_fn(&decltype(map)::emplace<const char*>), "emplace");
    test_insertion(std::mem_fn(&decltype(map)::insert<const char*>), "insert");
    test_insertion(std::mem_fn(&decltype(map)::insert_or_assign<const char*>), "insert_or_assign");
    test_insertion([](auto& map, auto key, auto& value) {
        return std::make_pair(map.emplace_unchecked(key, value), true);
    }, "emplace_unchecked");
    test_insertion([](auto& map, auto key, auto& value) {
        return std::make_pair(map.insert_unchecked(key, value), true);
    }, "insert_unchecked");

    SECTION("does not change values via insert(), returns current value instead")
    {
        auto [iter, success] = map.insert(2, "0");

        REQUIRE_FALSE(success);
        REQUIRE(iter->first == 2);
        REQUIRE(iter->second == "2");
    }
    SECTION("does change values via insert_or_assign()")
    {
        auto [iter, success] = map.insert_or_assign(2, "0");

        REQUIRE_FALSE(success);
        REQUIRE(iter->first == 2);
        REQUIRE(iter->second == "0");
    }
    SECTION("returns accurate size()")
    {
        REQUIRE(map.size() == 1);

        map.erase(2);
        REQUIRE(map.size() == 0);

        map.insert(1, "1");
        map.insert(2, "2");
        REQUIRE(map.size() == 2);
    }
    SECTION("retrieves unconst valid values")
    {
        auto iter = map.find(2);
        REQUIRE(iter->second == "2");
        CHECK(map.contains(2));

        iter->second = "0";
        REQUIRE(map.find(2)->second == "0");
    }
    SECTION("retrieves const valid values")
    {
        auto& cMap = std::as_const(map);
        auto iter = cMap.find(2);

        REQUIRE(iter->second == "2");
        CHECK(cMap.contains(2));
    }
    SECTION("does not retrieve invalid values")
    {
        REQUIRE(map.find(0) == map.end());
    }
    SECTION("iterates over keys in sorted order")
    {
        map.insert(1, "1");
        map.insert(4, "4");
        map.insert(3, "3");

        decltype(map)::key_type i = 1;
        for (auto iter = map.begin(); iter != map.end(); ++iter, ++i)
        {
            CHECK(iter->first == i);
            CHECK(iter->second == std::to_string(i));
        }
    }
    SECTION("erases valid values")
    {
        CHECK(map.erase(2));
        CHECK(map.size() == 0);
    }
    SECTION("does not erase invalid values")
    {
        CHECK_FALSE(map.erase(0));
        CHECK(map.size() == 1);
    }
    SECTION("can clear() an entire container")
    {
        map.clear();
        CHECK(map.size() == 0);
    }

    SECTION("(lifetime management)")
    {
        using Value = test_helper::ObjHelper;
        Value::reset();

        using TestType = oki::intl_::AssocSortedVector<oki::Handle, Value>;

        // I'm only testing emplace() here because I am lazy and happen to know
        // that this actually tests all of the relevant behavior
        SECTION("calls constructor on arguments to emplace()")
        {
            {
                auto lifetimeMap = TestType();
                auto [iter, success] = lifetimeMap.emplace(1, 1);

                REQUIRE(iter->second.value_ == 1);
            }

            Value::test(1, 0, 0);
        }
        SECTION("can default-construct in emplace()")
        {
            {
                auto lifetimeMap = TestType();
                auto [iter, success] = lifetimeMap.emplace(1);

                REQUIRE(iter->second.value_ == 0);
            }

            Value::test(1, 0, 0);
        }
        SECTION("can move-construct in emplace()")
        {
            {
                auto lifetimeMap = TestType();
                auto value = Value{ 1 };
                auto [iter, success] = lifetimeMap.emplace(1, std::move(value));

                REQUIRE(iter->second.value_ == 1);
            }

            Value::test(2, 0, 1);
        }
        SECTION("can copy-construct in emplace()")
        {
            {
                auto lifetimeMap = TestType();
                auto value = Value{ 1 };
                auto [iter, success] = lifetimeMap.emplace(1, value);

                REQUIRE(iter->second.value_ == 1);
            }

            Value::test(2, 1, 0);
        }
        SECTION("can copy-assign in insert_or_assign()")
        {
            {
                auto lifetimeMap = TestType();
                auto value = Value{ 1 };
                lifetimeMap.emplace(1);

                auto [iter, success] = lifetimeMap.insert_or_assign(1, value);

                REQUIRE(iter->second.value_ == 1);
            }

            Value::test(2, 1, 0);
        }
        SECTION("can move-assign in insert_or_assign()")
        {
            {
                auto lifetimeMap = TestType();
                auto value = Value{ 1 };
                lifetimeMap.emplace(1);

                auto [iter, success] = lifetimeMap.insert_or_assign(1, std::move(value));

                REQUIRE(iter->second.value_ == 1);
            }

            Value::test(2, 0, 1);
        }
    }
}

namespace test_helper
{
    template <typename Type>
    class IntersectionHelper
    {
    public:
        template <typename... Pairs>
        void operator()(const std::pair<oki::Handle, Type>& pair, const Pairs&... pairs)
        {
            CHECK(pair.first == pair.second);
            CHECK(((pairs.first == pairs.second) && ...));

            values_.push_back(pair.second);
        }

        template <typename... Maps>
        void do_test(const std::vector<Type>& expected, const Maps&... maps)
        {
            auto me = oki::intl_::variadic_set_intersection(
                *this,
                std::make_pair(maps.cbegin(), maps.cend())...
            );

            CHECK(expected == me.values_);
        }

        static auto create_map(std::initializer_list<Type> values)
        {
            static_assert(std::is_integral_v<Type>);
            oki::intl_::AssocSortedVector<oki::Handle, Type> map;

            for (auto value : values)
            {
                map.insert(value, value);
            }

            return map;
        }

    private:
        std::vector<Type> values_;
    };
}

TEST_CASE("variadic_set_intersection()", "[logic][ecs][algorithm]")
{
    test_helper::IntersectionHelper<unsigned int> helper;

    SECTION("iterates over a lone map")
    {
        helper.do_test({ 1, 2, 3 }, helper.create_map({ 1, 2, 3 }));
    }
    SECTION("intersects two maps")
    {
        auto map1 = helper.create_map({ 1, 3, 4, 5, 8, 9, 10 });
        auto map2 = helper.create_map({ 2, 3, 4, 7, 8, 9 });

        helper.do_test({ 3, 4, 8, 9 }, map1, map2);
    }
    SECTION("intersects three maps")
    {
        auto map1 = helper.create_map({ 1, 2, 3, 4, 6, 7, 8, 9 });
        auto map2 = helper.create_map({ 0, 2, 3, 5,    7,    9 });
        auto map3 = helper.create_map({ 0, 2, 3,    6, 7, 8, 9 });

        helper.do_test({ 2, 3, 7, 9 }, map1, map2, map3);
    }
    SECTION("intersects empty maps")
    {
        auto map1 = helper.create_map({ 1, 2, 3 });
        auto map2 = helper.create_map({ });

        helper.do_test({ }, map1, map2);
    }
    SECTION("can intersect any ordered map of pairs")
    {
        std::map<oki::Handle, unsigned int> map1;
        for (auto value : { 1, 2, 4, 6, 7, 8 })
        {
            map1.emplace(value, value);
        }
        auto map2 = helper.create_map({ 1, 2, 5, 8 });

        helper.do_test({ 1, 2, 8 }, map1, map2);
    }
}
