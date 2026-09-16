#include "pch.h"
#include "ContextEmulator.h"
#include <chrono>
#include <string.h>


StlSemaphore::StlSemaphore()
    : m_semaphore(1)
{}


StlSemaphore::~StlSemaphore()
{}


bool StlSemaphore::try_acquire(uint32_t timeout_ms)
{
    return m_semaphore.try_acquire_for(std::chrono::milliseconds(timeout_ms));
}


void StlSemaphore::release()
{
    m_semaphore.release();
}


ContextEmulator::ContextEmulator()
    : m_currreg{ 0 }
    , m_prevreg{ 0}
    , m_dll(0)
    , m_dlm(0)
    , m_rx_semaphore()
    , m_tx_semaphore()
    , m_rxrdy_handler(nullptr)
    , m_thre_handler(nullptr)
    , m_rxrdy_arg(nullptr)
    , m_thre_arg(nullptr)
{}


ContextEmulator::~ContextEmulator()
{}


volatile uint8_t* ContextEmulator::base_address() const
{
    return nullptr;
}


uint32_t ContextEmulator::clock() const
{
    return 0;
}


embedded::platform::IBinarySemaphore& ContextEmulator::rx_semaphore()
{
    return m_rx_semaphore;
}


embedded::platform::IBinarySemaphore& ContextEmulator::tx_semaphore()
{
    return m_tx_semaphore;
}


int ContextEmulator::connect_intr_rxrdy(embedded::platform::InterruptHandler handler, void* arg)
{
    m_rxrdy_handler = handler;
    m_rxrdy_arg = arg;
    return 0;
}


int ContextEmulator::connect_intr_thre(embedded::platform::InterruptHandler handler, void* arg)
{
    m_thre_handler = handler;
    m_thre_arg = arg;
    return 0;
}



void ContextEmulator::handle_frame_triggered()
{
    using std::uint8_t;

    // pre operations
    bool rxrdy_triggered = false;
    bool thre_triggered = false;
    uint8_t newreg[REGISTER_SIZE];
    ::memcpy(
        newreg,
        const_cast<const uint8_t*>(m_currreg),
        REGISTER_SIZE);

    uint8_t fcr = m_currreg[FCR];
    uint8_t lcr = m_currreg[LCR];
    uint8_t mcr = m_currreg[MCR];
    bool dlab = (lcr & LCR_DLAB) != 0;

    // main operations
    throw std::logic_error("Not implemented yet");

    // post operations
    ::memcpy(
        m_prevreg,
        const_cast<const uint8_t*>(m_currreg),
        REGISTER_SIZE);
    ::memcpy(
        const_cast<uint8_t*>(m_currreg),
        newreg,
        REGISTER_SIZE);
    if (rxrdy_triggered && m_rxrdy_handler != nullptr)
    {
        m_rxrdy_handler(m_rxrdy_arg);
    }
    if (thre_triggered && m_thre_handler != nullptr)
    {
        m_thre_handler(m_thre_arg);
    }
}
