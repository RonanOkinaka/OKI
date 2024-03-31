#include "oki/oki_handle.h"
#include "oki/util/oki_type_erasure.h"

#include "oki_test_util.h"

#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

using Value = test_helper::ObjHelper;

TEMPLATE_TEST_CASE("ErasedType", "[logic][ecs][type]",
    (oki::intl_::OptimalErasedType<test_helper::ObjHelper>),
    (oki::intl_::ErasedType<1, 1>)) // Undersized so won't fit an ObjHelper
{
    Value::reset();

    SECTION("default constructs when no arguments are provided")
    {
        {
            auto value = TestType::template erase_type<Value>();

            REQUIRE(value.template get_as<Value>().value_ == 0);
        }

        Value::test(1, 0, 0);
    }
    SECTION("calls constructor on arguments")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);

            REQUIRE(value.template get_as<Value>().value_ == 1);
        }

        Value::test(1, 0, 0);
    }
    SECTION("move constructs when same type is moved in")
    {
        {
            auto init = Value(1u);
            auto value = TestType::template erase_type<Value>(std::move(init));

            REQUIRE(value.template get_as<Value>().value_ == init.value_);
        }

        Value::test(2, 0, 1);
    }
    SECTION("copy constructs when same type is copied in")
    {
        {
            auto init = Value(1u);
            auto value = TestType::template erase_type<Value>(init);

            REQUIRE(value.template get_as<Value>().value_ == init.value_);
        }

        Value::test(2, 1, 0);
    }
    SECTION("copies and properly destructs after copy assignment")
    {
        {
            auto value1 = TestType::template erase_type<Value>(1u);
            auto value2 = TestType::template erase_type<Value>(2u);

            value1 = value2;

            REQUIRE(value1.template get_as<Value>().value_ == 2);
            REQUIRE(value2.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 1, 0);
    }
    SECTION("moves and properly destructs after move assignment")
    {
        {
            auto value1 = TestType::template erase_type<Value>(1u);
            auto value2 = TestType::template erase_type<Value>(2u);

            value1 = std::move(value2);

            REQUIRE(value1.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 0, std::nullopt);
        CHECK(Value::numMoves <= 1); // Unbuffered simply swaps pointers
    }
    SECTION("moves and properly destructs after copy_from()")
    {
        {
            auto value1 = TestType::template erase_type<Value>(1u);
            auto value2 = TestType::template erase_type<Value>(2u);

            value1.template copy_from<Value>(value2);

            REQUIRE(value1.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 1, 0);
    }
    SECTION("moves and properly destructs after move_from()")
    {
        {
            auto value1 = TestType::template erase_type<Value>(1u);
            auto value2 = TestType::template erase_type<Value>(2u);

            value1.template move_from<Value>(std::move(value2));

            REQUIRE(value1.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 0, std::nullopt);
        CHECK(Value::numMoves <= 1);
    }
    SECTION("properly destructs after copy self-assignment")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);
            value = value;

            CHECK(value.template get_as<Value>().value_ == 1);
        }

        Value::test(1, 1, 0);
    }
    SECTION("can assign rvalues with hold()")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);
            value.template hold<Value>(Value { 2 });

            CHECK(value.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 0, 1);
    }
    SECTION("can move values with hold()")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);
            auto other = Value { 2 };

            value.template hold<Value>(std::move(other));

            CHECK(value.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 0, 1);
    }
    SECTION("can copy values with hold()")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);
            Value other = Value { 2 };
            value.template hold<Value>(other);

            CHECK(value.template get_as<Value>().value_ == 2);
        }

        Value::test(2, 1, 0);
    }
    SECTION("can be copy-constructed")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);
            auto value2 = value;

            CHECK(value2.template get_as<Value>().value_ == 1);
        }

        Value::test(2, 1, 0);
    }
    SECTION("can be move-constructed")
    {
        {
            auto value = TestType::template erase_type<Value>(1u);
            auto value2 = std::move(value);

            CHECK(value2.template get_as<Value>().value_ == 1);
        }

        Value::test(2, 0, 1);
    }
    SECTION("can hold a move-only type")
    {
        // This basically just needs to compile
        using MoveType = std::unique_ptr<Value>;
        using TestType2 = oki::intl_::OptimalErasedType<MoveType>;

        {
            auto value
                = TestType2::template erase_type<MoveType>(new Value { 1u });

            auto ptr = value.template get_as<MoveType>().get();
            CHECK((ptr && ptr->value_ == 1));
        }

        Value::test(1, 0, 0);
    }
}
