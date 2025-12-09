#ifndef EXT_STOPWATCH_H
#define EXT_STOPWATCH_H

#include <chrono>
#include <ratio>
#include <utility>

namespace ext {
namespace intl_ {
template <typename Clock, typename Ratio, typename Type>
class BasicStopWatch
{
public:
    Type start() noexcept
    {
        auto now = Clock::now();
        return this->get_diff_(std::exchange(start_, now), now);
    }

    Type count() const noexcept { return this->get_diff_(start_); }

private:
    using TimePoint = typename Clock::time_point;

    TimePoint start_ = Clock::now();

    static Type get_diff_(const TimePoint& early, TimePoint late = Clock::now())
    {
        using std::chrono::duration_cast;
        using DurationTarget = std::chrono::duration<Type, Ratio>;

        return duration_cast<DurationTarget>(late - early).count();
    }
};
}

using StopWatch = ext::intl_::BasicStopWatch<std::chrono::steady_clock, std::ratio<1>, float>;
}

#endif // EXT_STOPWATCH_H
