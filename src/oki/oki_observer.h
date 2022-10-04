#ifndef OKI_OBSERVER_H
#define OKI_OBSERVER_H

#include "oki/oki_handle.h"
#include "oki/util/oki_handle_gen.h"
#include "oki/util/oki_type_erasure.h"

#include <algorithm>
#include <map>
#include <type_traits>
#include <unordered_map>

namespace oki
{
    /*
     * Utility class representing options about an observer.
     *
     * Left opaque for future additions but is only capable of
     * triggering a disconnect right now.
     */
    class ObserverOptions
    {
    public:
        void disconnect()
        {
            disconn_ = true;
        }

    private:
        bool disconn_ = false;

        template <typename Subject>
        friend class SubjectPipe;
    };

    /*
     * The base class for an observer, it explicitly describes which
     * subject it subscribes to as a template argument.
     * This allows one class to observe several subjects.
     */
    template <typename Subject>
    class Observer
    {
    public:
        Observer() noexcept = default;
        Observer(const Observer&) noexcept = default;
        Observer(Observer&&) noexcept = default;
        virtual ~Observer() noexcept = default;

        // Only function required to allow an Observer to observe
        virtual void observe(Subject, oki::ObserverOptions&) = 0;
    };

    /*
     * Also referred to as a channel or slot, the SubjectPipe represents
     * interest in a particular subject. Observers (sinks) with a matching
     * subject can connect to the SubjectPipe to receive messages from it,
     * and sources can send data to it.
     *
     * Unlike other implementations, this is NEITHER thread-safe NOR
     * particularly fast or flexible. That said, the content of this
     * file is not coupled with the content of oki_component.h and more
     * sophisticated setups can be used with the ComponentManager as needed.
     */
    template <typename Subject>
    class SubjectPipe
    {
    public:
        SubjectPipe() noexcept = default;
        SubjectPipe(const SubjectPipe&) = delete;
        SubjectPipe(SubjectPipe&&) = default;
        ~SubjectPipe() = default;

        SubjectPipe& operator=(const SubjectPipe&) = delete;
        SubjectPipe& operator=(SubjectPipe&&) = default;

        /*
         * Connects an observer to this subject.
         *
         * Returns a handle with which one can later disconnect the observer.
         *
         * DOES NOT take ownership of the object, which must remain in place
         * until it is disconnected.
         */
        oki::Handle connect(Observer<Subject>& observer)
        {
            auto handle = handGen_.create_handle();
            observers_.insert({ handle, std::addressof(observer) });

            return handle;
        }

        /*
         * Disconnects an observer given its handle. Also destroys
         * the handle.
         */
        void disconnect(oki::Handle handle)
        {
            observers_.erase(handle);
            handGen_.destroy_handle(handle);
        }

        /*
         * Disconnects ALL observers from this subject and resets
         * the handle generation.
         */
        void disconnect_all()
        {
            observers_.clear();
            handGen_.reset();
        }

        /*
         * Send data down the pipe to all currently connected observers.
         */
        void send(Subject data)
        {
            auto iter = observers_.begin();

            while (iter != observers_.end())
            {
                oki::ObserverOptions options;
                iter->second->observe(data, options);

                if (options.disconn_)
                {
                    iter = observers_.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }

    private:
        std::map<oki::Handle, Observer<Subject>*> observers_;

        oki::intl_::DefaultHandleGenerator<oki::Handle> handGen_;
    };

    /*
     * Opaque handle used to refer to observers connected to the SignalManager.
     */
    class ObserverHandle
    {
    public:
        ObserverHandle(const ObserverHandle&) = default;

    private:
        using HandleType = oki::Handle;

        HandleType handle_ = oki::intl_::get_invalid_handle_constant();
        oki::intl_::TypeIndex type_;

        friend class SignalManager;

        ObserverHandle(HandleType handle, oki::intl_::TypeIndex type)
            : handle_(handle)
            , type_(type) { }
    };

    /*
     * A convenience class that keeps track of SubjectPipe objects for any
     * number of subjects. Dispatches send() data to the proper SubjectPipe.
     */
    class SignalManager
    {
    public:
        SignalManager() noexcept = default;
        SignalManager(const SignalManager&) = delete;
        SignalManager(SignalManager&&) = default;
        ~SignalManager() = default;

        SignalManager& operator=(const SignalManager&) = delete;
        SignalManager& operator=(SignalManager&&) = default;

        /*
         * Connects an observer to the supplied subject. Returns a handle that
         * can be used to refer to this observer (i.e., for deletion).
         *
         * Like SubjectPipe's connect(), DOES NOT TAKE OWNERSHIP. The provided
         * reference must stay valid unless the observer is disconnected.
         */
        template <typename Subject>
        oki::ObserverHandle connect(Observer<Subject>& observer)
        {
            // Try to find our pipe
            auto type = oki::intl_::get_type<Subject>();
            auto pipeIter = data_.find(type);

            // If we don't have it, make one
            if (pipeIter == data_.end())
            {
                pipeIter = data_.emplace(
                    type,
                    this->create_erased_pipe_<Subject>()
                ).first;
            }

            // Then connect our observer
            auto& pipe = pipeIter->second.pipe_.template get_as<Pipe<Subject>>();
            return { pipe.connect(observer), type };
        }

        /*
         * Disconnects an observer and destroys its handle.
         */
        void disconnect(oki::ObserverHandle handle)
        {
            auto pipeIter = data_.find(handle.type_);

            if (pipeIter != data_.end())
            {
                (*pipeIter->second.disconnect_)(pipeIter->second.pipe_, handle);
            }
        }

        /*
         * Disconnects every observer connected to the provided subject.
         */
        template <typename Subject>
        void disconnect_all()
        {
            this->call_on_pipe_checked_<Subject>([](auto& pipe) {
                pipe.disconnect_all();
            });
        }

        /*
         * Disconnects all observers.
         */
        void disconnect_all()
        {
            data_.clear();
        }

        /*
         * Sends data to all observers of the provided subject.
         */
        template <typename Subject>
        void send(const Subject& data)
        {
            this->call_on_pipe_checked_<Subject>([&](auto& pipe) {
                pipe.send(data);
            });
        }

    private:
        template <typename Subject>
        using Pipe = oki::SubjectPipe<std::decay_t<Subject>>;

        using ErasedPipe = oki::intl_::OptimalErasedType<Pipe<void*>>;

        struct ErasedPipeData
        {
            ErasedPipe pipe_;
            void (*disconnect_)(ErasedPipe&, ObserverHandle);
        };

        std::unordered_map<
            oki::intl_::TypeIndex,
            ErasedPipeData
        > data_;

        template <typename Subject, typename Callback>
        void call_on_pipe_checked_(Callback func)
        {
            auto pipeIter = data_.find(oki::intl_::get_type<Subject>());

            if (pipeIter != data_.end())
            {
                func(pipeIter->second.pipe_
                    .template get_as<Pipe<Subject>>()
                );
            }
        }

        template <typename Subject>
        static void disconn_type_erased_(ErasedPipe& pipe, oki::ObserverHandle handle)
        {
            pipe.template get_as<Pipe<Subject>>().disconnect(handle.handle_);
        }

        template <typename Subject>
        static ErasedPipeData create_erased_pipe_()
        {
            return ErasedPipeData {
                ErasedPipe::erase_type<Pipe<Subject>>(),
                &disconn_type_erased_<Subject>
            };
        }
    };
}

#endif // OKI_OBSERVER_H
