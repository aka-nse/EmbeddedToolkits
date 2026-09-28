#if !defined(EMBEDDEDTOOLKITS_PLATFORM_H)
#define EMBEDDEDTOOLKITS_PLATFORM_H

/*
    Platform abstraction layer for embedded systems.
    Provides basic synchronization primitives and utilities.
*/

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#   define EMBEDDEDTOOLKITS_LANGFEATURE_CONCEPTS 1
#   define EMBEDDEDTOOLKITS_CONCEPTS(constraints) constraints
#   include <concepts>
#else
#   define EMBEDDEDTOOLKITS_LANGFEATURE_CONCEPTS 0
#   define EMBEDDEDTOOLKITS_CONCEPTS(constraints) typename
#endif
#include <atomic>
#include <cstdint>

namespace embedded { namespace platform {

#if EMBEDDEDTOOLKITS_LANGFEATURE_CONCEPTS
    template<class TPtr, class IndexType = std::ptrdiff_t>
    concept UInt8Ptr =
        std::integral<IndexType> &&
        requires(TPtr ptr, IndexType n) {
            { *ptr } -> std::same_as<volatile std::uint8_t&>;
            { ptr[n] } -> std::same_as<volatile std::uint8_t&>;
            { ++ptr } -> std::convertible_to<TPtr>;
            { ptr++ } -> std::convertible_to<TPtr>;
            { --ptr } -> std::convertible_to<TPtr>;
            { ptr-- } -> std::convertible_to<TPtr>;
            { ptr + n } -> std::convertible_to<TPtr>;
            { ptr - n } -> std::convertible_to<TPtr>;
        };
    
    static_assert(UInt8Ptr<volatile std::uint8_t*>);
#endif

    /// @brief
    ///     Provides a fixed-size ring buffer (circular buffer) implementation.
    /// @tparam N
    ///     The size of the ring buffer. Must be a power of 2.
    template<std::size_t N>
    class RingBuffer
    {
        friend class RingBufferTest;

        using uint8_t = std::uint8_t;
        using atomic_size_t = std::atomic_size_t;

        static_assert(N >= 2, "RingBuffer size must be at least 2");
        static_assert((N & (N - 1)) == 0, "RingBuffer size must be a power of 2");
    public:
        RingBuffer()
            : m_head(0)
            , m_tail(0)
        {
        }

        bool empty() const
        {
            return m_head.load(std::memory_order_acquire) ==
                   m_tail.load(std::memory_order_relaxed);
        }

        bool full() const
        {
            return m_head.load(std::memory_order_relaxed) -
                   m_tail.load(std::memory_order_acquire) == N;
        }

        size_t size() const
        {
            return m_head.load(std::memory_order_acquire) -
                   m_tail.load(std::memory_order_acquire);
        }

        bool try_push(uint8_t value)
        {
            const size_t head = m_head.load(std::memory_order_relaxed);
            const size_t tail = m_tail.load(std::memory_order_acquire);
            if (head - tail == N)
            {
                return false;
            }
            m_buffer[head & (N - 1)] = value;
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        bool try_pop(uint8_t& value)
        {
            const size_t tail = m_tail.load(std::memory_order_relaxed);
            const size_t head = m_head.load(std::memory_order_acquire);
            if (head == tail)
            {
                return false;
            }
            value = m_buffer[tail & (N - 1)];
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

    private:
        uint8_t m_buffer[N];
        atomic_size_t m_head;
        atomic_size_t m_tail;
    };


    /// @brief
    ///     Interface for a binary semaphore.
    class IBinarySemaphore
    {
    public:
        inline virtual ~IBinarySemaphore() {}

        /// @brief
        ///     Tries to acquire the semaphore.
        /// @param [in] timeout_ms
        ///     The timeout in milliseconds.
        /// @return
        ///     `true` if the semaphore was successfully acquired, `false` otherwise.
        virtual bool try_acquire(uint32_t timeout_ms) = 0;

        /// @brief
        ///     Releases the semaphore.
        virtual void release() = 0;
    };


    /// @brief
    ///     Defines the type for an interrupt handler function.
    /// @param [in] arg
    ///     A pointer to user-defined data passed to the handler.
    typedef void (*InterruptHandler)(void* arg);

} }

#endif /* EMBEDDEDTOOLKITS_PLATFORM_H */
