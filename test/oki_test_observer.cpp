#include "oki/oki_observer.h"

#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include <numeric>
#include <vector>

template <typename Subject>
class Observer : public oki::Observer<Subject>
{
public:
    void observe(Subject value, oki::ObserverOptions& options) override
    {
        values_.push_back(value);

        if (disconnectNext_) {
            options.disconnect();
        }
    }

    std::vector<Subject> values_;

    bool disconnectNext_ = false;
};

TEMPLATE_TEST_CASE("SubjectPipe", "[logic][ecs][system]", (oki::SubjectPipe<int>),
    // Designed to extend SubjectPipe to any type so has same test semantics
    (oki::SignalManager))
{
    using Observer = Observer<int>;

    TestType intChannel;
    Observer observer;

    auto obsHandle = intChannel.connect(observer);

    SECTION("can emit a signal")
    {
        intChannel.send(1);

        std::vector<int> expected { 1 };
        REQUIRE(observer.values_ == expected);
    }
    SECTION("can emit multiple signals")
    {
        intChannel.send(1);
        intChannel.send(2);
        intChannel.send(3);
        intChannel.send(4);
        intChannel.send(5);

        auto expected = std::vector<int>(5);
        std::iota(expected.begin(), expected.end(), 1);
        REQUIRE(observer.values_ == expected);
    }
    SECTION("(multiple observers)")
    {
        Observer observer2;
        intChannel.connect(observer2);

        Observer observer3;
        intChannel.connect(observer3);

        SECTION("can emit multiple signals to multiple observers")
        {
            intChannel.send(1);
            intChannel.send(2);

            std::vector<int> expected { 1, 2 };
            REQUIRE(observer.values_ == expected);
            REQUIRE(observer2.values_ == expected);
            REQUIRE(observer3.values_ == expected);
        }
        SECTION("can emit signals when an observer disconnects")
        {
            observer2.disconnectNext_ = true;

            intChannel.send(1);

            std::vector<int> expected { 1 };
            REQUIRE(observer.values_ == expected);
            REQUIRE(observer2.values_ == expected);
            REQUIRE(observer3.values_ == expected);

            intChannel.send(2);
            expected = { 1, 2 };
            REQUIRE(observer.values_ == expected);
            REQUIRE(observer2.values_.size() == 1);
            REQUIRE(observer3.values_ == expected);
        }
        SECTION("can disconnect all observers")
        {
            intChannel.disconnect_all();

            intChannel.send(1);
            REQUIRE(observer.values_.empty());
            REQUIRE(observer2.values_.empty());
            REQUIRE(observer3.values_.empty());
        }
    }
    SECTION("can disconnect observers with handle")
    {
        intChannel.send(1);
        intChannel.send(2);

        intChannel.disconnect(obsHandle);

        intChannel.send(3);
        intChannel.send(4);

        std::vector<int> expected { 1, 2 };
        REQUIRE(observer.values_ == expected);
    }
    SECTION("can disconnect observers with options")
    {
        intChannel.send(1);

        observer.disconnectNext_ = true;

        intChannel.send(2);

        intChannel.send(3);
        intChannel.send(4);

        std::vector<int> expected { 1, 2 };
        REQUIRE(observer.values_ == expected);
    }
}

TEST_CASE("SignalManager")
{
    oki::SignalManager sigMan;

    Observer<int> intObserver;
    Observer<float> floatObserver;

    sigMan.connect(intObserver);
    sigMan.connect(floatObserver);

    SECTION("can emit a signal to the correct observer")
    {
        sigMan.send(1);

        REQUIRE(intObserver.values_ == std::vector<int> { 1 });
        REQUIRE(floatObserver.values_.empty());
    }
    SECTION("can emit multiple signals to different observers")
    {
        sigMan.send(1);
        sigMan.send(2.f);
        sigMan.send(3.f);

        CHECK(intObserver.values_ == std::vector<int> { 1 });
        CHECK(floatObserver.values_ == std::vector<float> { 2., 3. });
    }
    SECTION("can disconnect_all() listeners of some type")
    {
        Observer<int> intObserver2;
        sigMan.connect(intObserver2);

        sigMan.disconnect_all<int>();

        sigMan.send(1);
        sigMan.send(2.f);

        CHECK(intObserver.values_.empty());
        CHECK(intObserver2.values_.empty());
        CHECK(floatObserver.values_ == std::vector<float> { 2. });
    }
    SECTION("can disconnect_all() listeners")
    {
        sigMan.disconnect_all();

        sigMan.send(1);
        sigMan.send(2.f);

        CHECK(intObserver.values_.empty());
        CHECK(floatObserver.values_.empty());
    }
}
