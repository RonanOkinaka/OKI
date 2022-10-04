#include "oki/util/oki_handle_gen.h"
#include "oki/oki_handle.h"

#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_template_test_macros.hpp"

#include <array>
#include <algorithm>
#include <unordered_set>

/*
 * We know that (for this implementation) the handle generators will create the
 * same sequence std::iota() would, but only formally guarantee that the handles 
 * will be distinct and not compare equal to the invalid handle constant.
 *
 * Generalized guarantees go in this test case.
 */
TEMPLATE_TEST_CASE("All handle generators", "[logic][ecs][handle]",
    (oki::intl_::LinearHandleGenerator<>),
    (oki::intl_::ReuseHandleGenerator<>),
    (oki::intl_::DebugHandleGenerator<>))
{
    // Generate 15 handles to check against our requirements
    TestType handleGen;
    std::array<oki::Handle, 15> handles;

    std::generate(handles.begin(), handles.end(), [&]()
    {
        return handleGen.create_handle();
    });
    std::unordered_set<oki::Handle> distinctHandles(handles.begin(), handles.end());

    SECTION("generates valid handles")
    {
        REQUIRE(std::none_of(
            handles.begin(), handles.end(),
            oki::intl_::is_bad_handle<>
        ));
    }
    SECTION("generates distinct handles")
    {
        REQUIRE(distinctHandles.size() == handles.size());
    }
    SECTION("correctly verifies the invalid handle constant")
    {
        REQUIRE_FALSE(
            handleGen.verify_handle(oki::intl_::get_invalid_handle_constant<>())
        );
    }
}

// Guarantees specific to the OKI handle generators go in this test case
TEMPLATE_TEST_CASE("OKI handle generators", "[logic][ecs][handle]",
    (oki::intl_::LinearHandleGenerator<>),
    (oki::intl_::ReuseHandleGenerator<>),
    (oki::intl_::DebugHandleGenerator<>))
{
    // Generate 15 handle to check against our requirements (just as before)
    TestType handleGen;
    std::array<oki::Handle, 15> handles;

    std::generate(handles.begin(), handles.end(), [&]()
    {
        return handleGen.create_handle();
    });
    std::unordered_set<oki::Handle> distinctHandles(handles.begin(), handles.end());

    SECTION("correctly verifies given handles")
    {
        for (auto handle : handles)
        {
            REQUIRE(handleGen.verify_handle(handle));
        }
    }
    SECTION("correctly verifies handles which could not have been given")
    {
        REQUIRE_FALSE(handleGen.verify_handle(handles.back() + 1));
    }
    SECTION("successfully destroys valid handles")
    {
        CHECK(handleGen.destroy_handle(handles.front()));
        CHECK(handleGen.destroy_handle(handles.back()));
    }
    SECTION("returns generation state to default on reset")
    {
        handleGen.reset();
        REQUIRE(handleGen.create_handle() == oki::intl_::get_first_valid_handle<>());
    }
    SECTION("returns verification state to default on reset")
    {
        handleGen.reset();
        CHECK_FALSE(handleGen.verify_handle(handles.front()));
    }
}

// Extra verification guarantees provided by the two tracking generators
TEMPLATE_TEST_CASE("OKI tracking handle generators", "[logic][ecs][handle]",
    (oki::intl_::ReuseHandleGenerator<>),
    (oki::intl_::DebugHandleGenerator<>))
{
    TestType handleGen;
    auto handle = handleGen.create_handle();

    SECTION("successfully deletes a valid handle")
    {
        REQUIRE(handleGen.destroy_handle(handle));
    }
    SECTION("correctly verifies deleted handles that have not been reused")
    {
        handleGen.destroy_handle(handle);
        CHECK_FALSE(handleGen.verify_handle(handle));
    }
}

TEST_CASE("ReuseHandleGenerator", "[logic][ecs][handle]")
{
    SECTION("reuses deleted handles")
    {
        oki::intl_::ReuseHandleGenerator<> handleGen;

        auto handle = handleGen.create_handle();
        handleGen.destroy_handle(handle);

        REQUIRE(handleGen.create_handle() == handle);
    }
}

TEST_CASE("DebugHandleGenerator", "[logic][ecs][handle]")
{
    oki::intl_::DebugHandleGenerator<> handleGen;

    auto handle = handleGen.create_handle();
    handleGen.destroy_handle(handle);

    SECTION("correctly identifies a double-delete")
    {
        REQUIRE_FALSE(handleGen.destroy_handle(handle));
    }
    SECTION("returns deletion state to default on reset")
    {
        handleGen.reset();
        CHECK_FALSE(handleGen.destroy_handle(handle));
    }
}
