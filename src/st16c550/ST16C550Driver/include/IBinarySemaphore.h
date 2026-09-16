#if !defined(EMBEDDEDTOOLKITS_BINARY_SEMAPHORE_H)
#define EMBEDDEDTOOLKITS_BINARY_SEMAPHORE_H
#include <chrono>

namespace embedded::platform
{
    class IBinarySemaphore
    {
    public:
        inline virtual ~IBinarySemaphore() {}
        virtual bool try_acquire(uint32_t timeout_ms) = 0;
        virtual void release() = 0;
    };
}

#endif /* EMBEDDEDTOOLKITS_BINARY_SEMAPHORE_H */
