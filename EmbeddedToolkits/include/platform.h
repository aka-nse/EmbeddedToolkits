#if !defined(EMBEDDEDTOOLKITS_PLATFORM_H)
#define EMBEDDEDTOOLKITS_PLATFORM_H

/*
    Platform abstraction layer for embedded systems.
    Provides basic synchronization primitives and utilities.
*/

#include <atomic>
#include <cstdint>

namespace embedded::platform
{
    /// @brief
    ///     Provides a fixed-size ring buffer (circular buffer) implementation.
    /// @tparam N
    ///     The size of the ring buffer. Must be a power of 2.
    template<std::size_t N>
    class RingBuffer
    {
        using uint8_t = std::uint8_t;
        using atomic_size_t = std::atomic_size_t;

        static_assert((N& (N - 1)) == 0, "RingBuffer size must be a power of 2");
    public:
        bool empty() const
        {
            return m_head == m_tail;
        }

        bool full() const
        {
            return m_head - m_tail == N;
        }

        size_t size() const
        {
            return m_head - m_tail;
        }

        bool try_push(uint8_t value)
        {
            if (full())
            {
                return false;
            }
            m_buffer[m_head & (N - 1)] = value;
            ++m_head;
            return true;
        }

        bool try_pop(uint8_t& value)
        {
            if (empty())
            {
                return false;
            }
            value = m_buffer[m_tail & (N - 1)];
            ++m_tail;
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
}

#endif /* EMBEDDEDTOOLKITS_PLATFORM_H */
