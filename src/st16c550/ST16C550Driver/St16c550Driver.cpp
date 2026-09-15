#include "St16c550Driver.h"

#include <utility>

namespace
{
    constexpr uint8_t IER_RECEIVED_DATA_AVAILABLE = 0x01;

    constexpr uint8_t FCR_ENABLE_FIFO = 0x01;
    constexpr uint8_t FCR_CLEAR_RX_FIFO = 0x02;
    constexpr uint8_t FCR_CLEAR_TX_FIFO = 0x04;

    constexpr uint8_t LCR_STOP_BITS_MASK = 0x04;
    constexpr uint8_t LCR_DLAB = 0x80;

    constexpr uint8_t MCR_OUT2 = 0x08;

    constexpr uint8_t LSR_DATA_READY = 0x01;
    constexpr uint8_t LSR_THR_EMPTY = 0x20;
}

SemaphoreBase::~SemaphoreBase()
{}


St16c550Driver::St16c550Driver(
    uint32_t clock_frequency_hz,
    volatile uint8_t* base_address,
    RegisterInterrupt register_interrupt,
    std::unique_ptr<SemaphoreBase>&& rxSemaphore)
    : m_clockFrequencyHz(clock_frequency_hz)
    , m_baseAddress(base_address)
    , m_registerInterrupt(register_interrupt)
    , m_isOpen(false)
    , m_isInterruptRegistered(false)
    , m_rxQueue()
    , m_rxSemaphore(std::move(rxSemaphore))
{
    if (m_registerInterrupt != nullptr)
    {
        m_isInterruptRegistered = (m_registerInterrupt(&St16c550Driver::interrupt_entry, this) == 0);
    }
}

St16c550Driver::~St16c550Driver()
{
    close();
}

void St16c550Driver::open(
    uint32_t baud_rate,
    uint8_t data_bits,
    StopBits stop_bits,
    ParityBits parity)
{
    if (m_baseAddress == nullptr)
    {
        return;
    }

    if (m_isOpen)
    {
        close();
    }

    if (!m_isInterruptRegistered && m_registerInterrupt != nullptr)
    {
        m_isInterruptRegistered = (m_registerInterrupt(&St16c550Driver::interrupt_entry, this) == 0);
    }

    uint8_t lineControl = 0;
    switch (data_bits)
    {
    case 5:
        lineControl = 0;
        break;
    case 6:
        lineControl = 1;
        break;
    case 7:
        lineControl = 2;
        break;
    case 8:
    default:
        lineControl = 3;
        break;
    }

    if (stop_bits == STOP_BITS_2)
    {
        lineControl |= LCR_STOP_BITS_MASK;
    }

    lineControl |= static_cast<uint8_t>(parity) << 3;

    std::queue<uint8_t> emptyQueue;
    m_rxQueue.swap(emptyQueue);

    set_reg(IER, 0);
    set_reg(FCR, FCR_ENABLE_FIFO | FCR_CLEAR_RX_FIFO | FCR_CLEAR_TX_FIFO);
    set_reg(LCR, lineControl);
    configure_baud_rate(baud_rate);
    set_reg(MCR, m_isInterruptRegistered ? MCR_OUT2 : 0);
    drain_receive_fifo(false);
    set_reg(IER, m_isInterruptRegistered ? IER_RECEIVED_DATA_AVAILABLE : 0);

    m_isOpen = true;
}

void St16c550Driver::close()
{
    if (m_baseAddress == nullptr)
    {
        m_isOpen = false;
        return;
    }

    set_reg(IER, 0);
    set_reg(MCR, 0);
    set_reg(FCR, FCR_ENABLE_FIFO | FCR_CLEAR_RX_FIFO | FCR_CLEAR_TX_FIFO);
    set_reg(FCR, 0);

    while ((get_reg(LSR) & LSR_DATA_READY) != 0)
    {
        (void)get_reg(RHR);
    }

    std::queue<uint8_t> emptyQueue;
    m_rxQueue.swap(emptyQueue);
    m_isOpen = false;
}

size_t St16c550Driver::write(const uint8_t* buffer, size_t size)
{
    if (!m_isOpen || buffer == nullptr || size == 0)
    {
        return 0;
    }

    size_t written = 0;
    while (written < size)
    {
        while ((get_reg(LSR) & LSR_THR_EMPTY) == 0)
        {
        }

        set_reg(THR, buffer[written]);
        ++written;
    }

    return written;
}

size_t St16c550Driver::read(uint8_t* buffer, size_t size)
{
    if (!m_isOpen || buffer == nullptr || size == 0)
    {
        return 0;
    }

    drain_receive_fifo(false);

    if (m_rxQueue.empty())
    {
        if (m_isInterruptRegistered && m_rxSemaphore)
        {
            m_rxSemaphore->acquire();
        }
        else
        {
            while ((get_reg(LSR) & LSR_DATA_READY) == 0)
            {
            }
        }

        drain_receive_fifo(false);
    }

    size_t readSize = 0;
    while (readSize < size && !m_rxQueue.empty())
    {
        buffer[readSize] = m_rxQueue.front();
        m_rxQueue.pop();
        ++readSize;
    }

    return readSize;
}

size_t St16c550Driver::available() const
{
    if (!m_isOpen)
    {
        return 0;
    }

    const_cast<St16c550Driver*>(this)->drain_receive_fifo(false);
    return m_rxQueue.size();
}

void St16c550Driver::set_reg(RegisterIndex reg, uint8_t value)
{
    m_baseAddress[static_cast<size_t>(reg)] = value;
}

uint8_t St16c550Driver::get_reg(RegisterIndex reg) const
{
    return m_baseAddress[static_cast<size_t>(reg)];
}

void St16c550Driver::configure_baud_rate(uint32_t baud_rate)
{
    if (baud_rate == 0)
    {
        return;
    }

    uint32_t divisor = m_clockFrequencyHz / (16u * baud_rate);
    if (divisor == 0)
    {
        divisor = 1;
    }
    else if (divisor > 0xFFFFu)
    {
        divisor = 0xFFFFu;
    }

    const uint8_t lineControl = get_reg(LCR);
    set_reg(LCR, lineControl | LCR_DLAB);
    set_reg(DLL, static_cast<uint8_t>(divisor & 0xFFu));
    set_reg(DLM, static_cast<uint8_t>((divisor >> 8) & 0xFFu));
    set_reg(LCR, lineControl);
}

void St16c550Driver::drain_receive_fifo(bool signal_waiters)
{
    while ((get_reg(LSR) & LSR_DATA_READY) != 0)
    {
        m_rxQueue.push(get_reg(RHR));
        if (signal_waiters && m_rxSemaphore)
        {
            m_rxSemaphore->release();
        }
    }
}

void St16c550Driver::handle_interrupt()
{
    if (!m_isOpen || m_baseAddress == nullptr)
    {
        return;
    }

    drain_receive_fifo(true);
}

void St16c550Driver::interrupt_entry(void* arg)
{
    if (arg == nullptr)
    {
        return;
    }

    static_cast<St16c550Driver*>(arg)->handle_interrupt();
}
