#include "catch2/catch_test_macros.hpp"

#include <cstdint>
#include <optional>

namespace test_helper {
struct ObjHelper
{
    static inline std::size_t numConstructs = 0, numCopies = 0, numMoves = 0,
                              numDestructs = 0;

    ObjHelper()
        : value_(0)
    {
        ++numConstructs;
    }

    ObjHelper(std::size_t value)
        : value_(value)
    {
        ++numConstructs;
    }

    ObjHelper(const ObjHelper& that)
        : value_(that.value_)
    {
        ++numConstructs;
        ++numCopies;
    }

    ObjHelper(ObjHelper&& that)
        : value_(that.value_)
    {
        ++numConstructs;
        ++numMoves;
    }

    ~ObjHelper() { ++numDestructs; }

    ObjHelper& operator=(const ObjHelper& that)
    {
        ++numCopies;
        value_ = that.value_;
        return (*this);
    }

    ObjHelper& operator=(ObjHelper&& that)
    {
        ++numMoves;
        value_ = that.value_;
        return (*this);
    }

    static void reset()
    {
        numConstructs = numCopies = numMoves = numDestructs = 0;
    }

    static void test() { CHECK(numConstructs == numDestructs); }

    static void test(std::optional<std::size_t> constr,
        std::optional<std::size_t> copies, std::optional<std::size_t> moves)
    {
        test();

        if (constr) {
            CHECK(numConstructs == constr.value());
        }
        if (copies) {
            CHECK(numCopies == copies.value());
        }
        if (moves) {
            CHECK(numMoves == moves.value());
        }
    }

    static void test_max_num_copies(std::size_t copies)
    {
        test();
        CHECK(numCopies <= copies);
    }

    std::size_t value_;
};
}
