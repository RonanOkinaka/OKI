#ifndef OKI_HANDLE_GEN_H
#define OKI_HANDLE_GEN_H

#include "oki/oki_handle.h"

#include <forward_list>
#include <functional>
#include <unordered_set>

namespace oki
{
    namespace intl_
    {
        /*
         * This is the fastest version of the handle generator family,
         * but will only issue only a limited number of valid handles 
         * (regardless of deletion!). 
         * 
         * That said, the currently implementation - using integers - 
         * will issue std::numeric_limits<HandleType>::max() handles, which
         * is likely plenty.
         * 
         * The default Handle is a 64-bit integer, but even a 32-bit handle
         * type can create 2^32-1 handles before having issues (that's about
         * 1000 handles per second for over 1000 hours!).
         */
        template <typename HandleType = oki::Handle>
        class LinearHandleGenerator
        {
        public:
            // Move-only: Two generators with same state can only be trouble
            constexpr LinearHandleGenerator() noexcept = default;
            LinearHandleGenerator(const LinearHandleGenerator<HandleType>&) noexcept = delete;
            constexpr LinearHandleGenerator(LinearHandleGenerator<HandleType>&&) noexcept = default;
            ~LinearHandleGenerator() noexcept = default;

            // Consumes the next handle value
            constexpr HandleType create_handle() noexcept
            {
                return oki::intl_::advance(counter_);
            }

            /*
             * Semantically, signifies that this Handle will no longer be used.
             * Returns a bool explaining whether the Handle was deleted without error
             * (which is always true for this class).
             */
            constexpr bool destroy_handle(const HandleType handle) noexcept
            {
                return true;
            }

            // Returns the generator's state to one equivalent to immediately after initialization
            constexpr void reset() noexcept
            {
                counter_ = oki::intl_::get_first_valid_handle();
            }

            /*
             * The simplest of the handle verifications, this will miss most bugs:
             * it will return true if the handle was at one point given by this
             * generator, but makes no guarantees that it was never deleted.
             * For better verification, use one of the other generators.
             */
            constexpr bool verify_handle(const HandleType handle) const noexcept
            {
                return !oki::intl_::is_bad_handle(handle)
                    && std::less<HandleType>{ }(handle, counter_);
            }

        private:
            HandleType counter_ = oki::intl_::get_first_valid_handle();
        };

        /*
         * This is the slowest, but safest, handle generator.
         * 
         * It is mostly useful when one is looking for errors relating to handle
         * generation/deletion. It is able to catch double-deletes and bad
         * handles so is useful for debugging.
         * 
         * Like LinearHandleGenerator, this option will issue a limited number 
         * of handles over its lifespan [currently, that number is exactly
         * std::numeric_limits<HandleType>::max()].
         */
        template <typename HandleType = oki::Handle>
        class DebugHandleGenerator
        {
        public:
            // Move-only: Two generators with same state can only be trouble
            DebugHandleGenerator() = default;
            DebugHandleGenerator(const DebugHandleGenerator<HandleType>&) = delete;
            DebugHandleGenerator(DebugHandleGenerator<HandleType>&&) = default;
            ~DebugHandleGenerator() noexcept = default;

            // Consumes the next handle value
            constexpr HandleType create_handle() noexcept
            {
                return handleGen_.create_handle();
            }

            /*
             * Returns true if handle destruction was successful.
             * This generator will catch double-deletes and attempts to
             * delete a bad handle.
             */
            bool destroy_handle(const HandleType handle)
            {
                // Fail if the handle was already deleted (or is the invalid constant)
                return handleGen_.verify_handle(handle)
                    && invalidHandles_.insert(handle).second;
            }

            // Returns the generator's state to one equivalent to immediately after initialization
            void reset() noexcept
            {
                handleGen_.reset();
                invalidHandles_.clear();
            }

            /*
             * This verify_handle() comes with the most guarantees.
             * If it returns true:
             *   - The handle must be active:
             *     - The handle was given by this generator instance
             *     - The handle has not been deleted
             */
            bool verify_handle(const HandleType handle) const noexcept
            {
                return handleGen_.verify_handle(handle) // Could have been given
                    && !invalidHandles_.count(handle);  // Was not deleted
            }

        private:
            std::unordered_set<HandleType> invalidHandles_;

            LinearHandleGenerator<HandleType> handleGen_;
        };

        /*
         * This is the middle-ground handle generator, being able to run the
         * longest (by far) at the cost of speed.
         * 
         * It attempts to guarantee that, if memory permits, the entire domain of 
         * the HandleType will always be available, even after deleting some. This 
         * assumes that the user is properly deleting handles.
         */
        template <typename HandleType = oki::Handle>
        class ReuseHandleGenerator
        {
        public:
            ReuseHandleGenerator() = default;
            ReuseHandleGenerator(const ReuseHandleGenerator<HandleType>&) noexcept = delete;
            ReuseHandleGenerator(ReuseHandleGenerator<HandleType>&&) noexcept = default;
            ~ReuseHandleGenerator() noexcept = default;

            HandleType create_handle() noexcept
            {
                if (!deletedHandles_.empty())
                {
                    auto handle = deletedHandles_.front();
                    deletedHandles_.pop_front();

                    return handle;
                }

                return handleGen_.create_handle();
            }

            /*
             * Returns true if destruction was successful (so the handle can be reused later).
             * Returns false if there was an exception thrown when attempting to destroy.
             */
            bool destroy_handle(const HandleType handle) noexcept
            {
                try 
                {
                    deletedHandles_.push_front(handle);
                }
                catch (...)
                {
                    // Because this is not the debug generator, it's better to just leak 
                    // the handle than to allow the exception to propagate
                    return false;
                }

                return true;
            }

            void reset() noexcept
            {
                deletedHandles_.clear();
                handleGen_.reset();
            }

            /*
             * This is a mediocre verify_handle(), guaranteeing that:
             *   - The handle is currently active:
             *     - It could have been given by this generator
             *     - It is LIKELY not currently deleted (i.e. it refers to something)
             * However, there are some caveats:
             *   - The handle could have been leaked
             *   - The handle could have been reused so might not refer to the same
             *       object it once did
             *   - The function traverses an unbounded linked list so should be used 
             *       as rarely as possible
             */
            bool verify_handle(const HandleType handle) const noexcept
            {
                auto listEnd = deletedHandles_.end();

                return handleGen_.verify_handle(handle) // Could have been given
                    && (std::find(deletedHandles_.begin(), listEnd, handle) == listEnd);
                    // Is not currently deleted (as far as we know)
            }

        private:
            std::forward_list<HandleType> deletedHandles_;

            LinearHandleGenerator<HandleType> handleGen_;
        };

        template <typename HandleType = oki::Handle>
        using DefaultHandleGenerator = LinearHandleGenerator<HandleType>;
    }
}

#endif // OKI_HANDLE_GEN_H
