#if !defined(EMBEDDEDTOOLKITS_RING_BUFFER_H)
#define EMBEDDEDTOOLKITS_RING_BUFFER_H
#include <atomic>
#include <cstdint>

namespace embedded::platform
{
    template<std::size_t N>
    class RingBuffer
    {
        using uint8_t       = std::uint8_t;
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
}

#endif /* EMBEDDEDTOOLKITS_RING_BUFFER_H */
