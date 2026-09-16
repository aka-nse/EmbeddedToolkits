#include "St16c550Driver.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace embedded::st16c550
{
    ISt16c550Context::~ISt16c550Context() {}
    void ISt16c550Context::on_transimitting() {}
    void ISt16c550Context::on_transmitted() {}

    St16c550Driver::St16c550Driver(std::unique_ptr<ISt16c550Context>&& context)
        : m_clock(context->clock())
        , m_baseAddress(context->base_address())
        , m_context(std::move(context))
        , m_rxBuffer{}
        , m_txBuffer{}
        , m_opened(false)
        , m_rxrdy_connected(false)
        , m_thre_connected(false)
    {
        if (m_context == nullptr || m_baseAddress == nullptr || m_clock == 0)
        {
            throw std::invalid_argument("invalid st16c550 context");
        }
    }


    St16c550Driver::~St16c550Driver()
    {
        close();
    }


    void St16c550Driver::open(uint32_t baud_rate, uint8_t data_bits, StopBits stop_bits, ParityBits parity)
    {
        if (baud_rate == 0)
        {
            throw std::invalid_argument("baud_rate must not be zero");
        }

        if (!m_rxrdy_connected)
        {
            m_rxrdy_connected = (m_context->connect_intr_rxrdy(&St16c550Driver::handle_rxrdy_interrupt, this) == 0);
        }

        if (!m_thre_connected)
        {
            m_thre_connected = (m_context->connect_intr_thre(&St16c550Driver::handle_thre_interrupt, this) == 0);
        }

        const uint32_t divisor = m_clock / (16u * baud_rate);
        if (divisor == 0 || divisor > 0xFFFFu)
        {
            throw std::invalid_argument("baud_rate is out of range");
        }

        uint8_t lineControl = LCR_WORD_LENGTH_8;
        switch (data_bits)
        {
        case 5:
            lineControl = LCR_WORD_LENGTH_5;
            break;
        case 6:
            lineControl = LCR_WORD_LENGTH_6;
            break;
        case 7:
            lineControl = LCR_WORD_LENGTH_7;
            break;
        case 8:
            lineControl = LCR_WORD_LENGTH_8;
            break;
        default:
            throw std::invalid_argument("data_bits must be 5-8");
        }

        if (stop_bits == STOP_BITS_2)
        {
            lineControl = static_cast<uint8_t>(lineControl | LCR_STOP_BITS);
        }

        switch (parity)
        {
        case PARITY_NONE:
            break;
        case PARITY_ODD:
            lineControl = static_cast<uint8_t>(lineControl | LCR_PARITY_ENABLE);
            break;
        case PARITY_EVEN:
            lineControl = static_cast<uint8_t>(lineControl | LCR_PARITY_ENABLE | LCR_EVEN_PARITY);
            break;
        default:
            throw std::invalid_argument("unsupported parity");
        }

        set_reg(IER, 0);
        set_reg(FCR, static_cast<uint8_t>(FCR_FIFO_ENABLE | FCR_RX_FIFO_RESET | FCR_TX_FIFO_RESET));
        set_reg(LCR, static_cast<uint8_t>(lineControl | LCR_DLAB));
        set_reg(DLL, static_cast<uint8_t>(divisor & 0xFFu));
        set_reg(DLM, static_cast<uint8_t>((divisor >> 8) & 0xFFu));
        set_reg(LCR, lineControl);
        set_reg(IER, m_rxrdy_connected ? IER_RXRDY : 0);

        m_opened = true;
    }


    void St16c550Driver::close()
    {
        if (!m_opened)
        {
            return;
        }

        set_reg(IER, 0);
        set_reg(FCR, static_cast<uint8_t>(FCR_FIFO_ENABLE | FCR_RX_FIFO_RESET | FCR_TX_FIFO_RESET));
        m_opened = false;
    }


    size_t St16c550Driver::write(const uint8_t * buffer, size_t size)
    {
        if (!m_opened || buffer == nullptr || size == 0)
        {
            return 0;
        }

        size_t written = 0;
        while (written < size)
        {
            if (!m_txBuffer.try_push(buffer[written]))
            {
                break;
            }

            ++written;
        }

        if (written == 0)
        {
            return 0;
        }

        if (!m_context->tx_semaphore().try_acquire(static_cast<uint32_t>(-1)))
        {
            return 0;
        }
        m_context->on_transimitting();

        if (m_thre_connected)
        {
            set_flag(IER, IER_THRE, true);
        }

        return written;
    }


    size_t St16c550Driver::read(uint8_t* buffer, size_t size, uint32_t timeout_ms)
    {
        if (!m_opened || buffer == nullptr || size == 0)
        {
            return 0;
        }

        if (m_rxBuffer.empty())
        {
            if (!m_context->rx_semaphore().try_acquire(timeout_ms))
            {
                return 0;
            }
        }

        size_t readSize = 0;
        while (readSize < size)
        {
            uint8_t value = 0;
            if (!m_rxBuffer.try_pop(value))
            {
                break;
            }

            buffer[readSize] = value;
            ++readSize;
        }

        return readSize;
    }


    size_t St16c550Driver::available() const
    {
        return m_opened ? m_rxBuffer.size() : 0;
    }


    void St16c550Driver::set_reg(RegisterIndex index, uint8_t value)
    {
        this->m_baseAddress[index] = value;
    }


    std::uint8_t St16c550Driver::get_reg(RegisterIndex index) const
    {
        return this->m_baseAddress[index];
    }


    void St16c550Driver::set_flag(RegisterIndex index, RegisterFlags flag, bool value)
    {
        uint8_t regValue = this->m_baseAddress[index];

        regValue = static_cast<uint8_t>((regValue & ~flag) | (-static_cast<uint8_t>(value) & flag));
        // NOTE: The above line is equivalent to the following code:
        // ```
        // if (value)
        // {
        //     regValue |= flag;
        // }
        // else
        // {
        //     regValue &= ~flag;
        // }
        // ```

        this->m_baseAddress[index] = regValue;
    }


    bool St16c550Driver::has_flag(RegisterIndex index, RegisterFlags flag) const
    {
        uint8_t value = this->m_baseAddress[index];
        return (value & flag) != 0;
    }


    void St16c550Driver::handle_rxrdy_interrupt()
    {
        if (!m_opened)
        {
            return;
        }

        bool received = false;
        while (has_flag(LSR, LSR_DATA_READY))
        {
            const uint8_t value = get_reg(RHR);
            if (!m_rxBuffer.try_push(value))
            {
                break;
            }

            received = true;
        }

        if (received)
        {
            m_context->rx_semaphore().release();
        }
    }


    void St16c550Driver::handle_rxrdy_interrupt(void* arg)
    {
        if (arg == nullptr)
        {
            return;
        }

        static_cast<St16c550Driver*>(arg)->handle_rxrdy_interrupt();
    }


    void St16c550Driver::handle_thre_interrupt()
    {
        if (!m_opened)
        {
            return;
        }

        uint8_t value = 0;
        if (!m_txBuffer.try_pop(value))
        {
            set_flag(IER, IER_THRE, false);
            m_context->on_transmitted();
            m_context->tx_semaphore().release();
            return;
        }

        set_reg(THR, value);
        m_context->tx_semaphore().release();
    }


    void St16c550Driver::handle_thre_interrupt(void* arg)
    {
        if (arg == nullptr)
        {
            return;
        }

        static_cast<St16c550Driver*>(arg)->handle_thre_interrupt();
    }
}