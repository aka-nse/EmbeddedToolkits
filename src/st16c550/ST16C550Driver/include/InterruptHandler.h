#if !defined(EMBEDDEDTOOLKITS_INTERRUPT_HANDLER_H)
#define EMBEDDEDTOOLKITS_INTERRUPT_HANDLER_H

namespace embedded::platform
{
    typedef void (*InterruptHandler)(void* arg);
}

#endif /* EMBEDDEDTOOLKITS_INTERRUPT_HANDLER_H */
