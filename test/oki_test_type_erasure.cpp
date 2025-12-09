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
    (oki::intl_::ErasedType<sizeof(Value), alignof(Value)>), (oki::intl_::OptimalErasedType<Value>),
    (oki::intl_::ErasedType<1, 1>)) // Undersized so won't fit an ObjHelper
{
    Value::reset();

    SECTION("default emplaces when no arguments are provided")
    {
        {
            TestType value;
            value.template emplace<Value>();

            REQUIRE(value.template get_as<Value>().value_ == 0);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("calls constructor on arguments to emplace()")
    {
        {
            TestType value;
            value.template emplace<Value>(1u);

            REQUIRE(value.template get_as<Value>().value_ == 1);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("can copy construct")
    {
        {
            Value init { 1u };
            TestType value = init;

            REQUIRE(value.template get_as<Value>().value_ == init.value_);
        }

        Value::test_max_num_copies(1);
    }
    SECTION("can move construct")
    {
        {
            Value init { 1u };
            TestType value = std::move(init);

            REQUIRE(value.template get_as<Value>().value_ == init.value_);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("can copy emplace")
    {
        {
            Value init { 1u };
            TestType value;
            value.template emplace<Value>(init);

            REQUIRE(value.template get_as<Value>().value_ == init.value_);
        }

        Value::test_max_num_copies(1);
    }
    SECTION("can move emplace")
    {
        {
            Value init { 1u };
            TestType value;
            value.template emplace<Value>(std::move(init));

            REQUIRE(value.template get_as<Value>().value_ == 1u);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("copies and properly destructs after copy assignment")
    {
        {
            TestType value1 = Value(1u);
            TestType value2 = Value(2u);

            value1 = value2;

            REQUIRE(value1.template get_as<Value>().value_ == 2);
            REQUIRE(value2.template get_as<Value>().value_ == 2);
        }

        Value::test_max_num_copies(1);
    }
    SECTION("moves and properly destructs after move assignment")
    {
        {
            TestType value1 = Value(1u);
            TestType value2 = Value(2u);

            value1 = std::move(value2);

            REQUIRE(value2.template get_as<Value>().value_ == 2);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("copies and properly destructs after copy_from()")
    {
        {
            TestType value1 = Value(1u);
            TestType value2 = Value(2u);

            value1.copy_from(value2);

            REQUIRE(value1.template get_as<Value>().value_ == 2);
            REQUIRE(value2.template get_as<Value>().value_ == 2);
        }

        Value::test_max_num_copies(1);
    }
    SECTION("moves and properly destructs after move_from()")
    {
        {
            TestType value1 = Value(1u);
            TestType value2 = Value(2u);

            value1.move_from(std::move(value2));

            REQUIRE(value1.template get_as<Value>().value_ == 2);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("properly destructs after copy self-assignment")
    {
        {
            TestType value = Value(1u);
            value = value;

            CHECK(value.template get_as<Value>().value_ == 1);
        }

        Value::test_max_num_copies(1);
    }
    SECTION("can be copy-constructed")
    {
        {
            TestType value1 = Value(1u);
            auto value2 = value1;

            CHECK(value2.template get_as<Value>().value_ == 1);
        }

        Value::test_max_num_copies(1);
    }
    SECTION("can be move-constructed")
    {
        {
            TestType value1 = Value(1u);
            auto value2 = std::move(value1);

            CHECK(value2.template get_as<Value>().value_ == 1);
        }

        Value::test_max_num_copies(0);
    }
    SECTION("can hold a move-only type")
    {
        using MoveType = std::unique_ptr<Value>;

        {
            TestType value1 = std::make_unique<Value>(1u);
            TestType value2;

            REQUIRE_THROWS(value2.copy_from(value1));

            value2.move_from(std::move(value1));

            auto ptr = value2.template get_as<MoveType>().get();
            CHECK((ptr && ptr->value_ == 1));
        }

        Value::test_max_num_copies(0);
    }
}
