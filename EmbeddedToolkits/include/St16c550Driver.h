#if !defined(EMBEDDEDTOOLKITS_ST16C550_DRIVER_H)
#define EMBEDDEDTOOLKITS_ST16C550_DRIVER_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include "platform.h"


namespace embedded::st16c550
{
    template<class TPtr>
    concept UInt8Ptr = requires(TPtr ptr) {
        { ptr.operator*() } -> std::same_as<volatile std::uint8_t&>;
        { ptr.operator->() } -> std::same_as<volatile std::uint8_t*>;
        { ptr.operator++() } -> std::same_as<TPtr&>;
        { ptr.operator++(0) } -> std::same_as<TPtr>;
        { ptr.operator--() } -> std::same_as<TPtr&>;
        { ptr.operator--(0) } -> std::same_as<TPtr>;
        { ptr.operator[](size_t) } -> std::same_as<TPtr&>;
    };


    /// @brief
    ///     The platform abstraction interface for the ST16C550 driver.
    class ISt16c550Context
    {
    protected:
        using size_t   = std::size_t;
        using uint8_t  = std::uint8_t;
        using uint32_t = std::uint32_t;

    public:
        inline virtual ~ISt16c550Context() {}

        /// @brief
        ///     Gets the binary semaphore for the RX buffer of the ST16C550.
        virtual platform::IBinarySemaphore& rx_semaphore() = 0;

        /// @brief
        ///     Gets the binary semaphore for the TX buffer of the ST16C550.
        virtual platform::IBinarySemaphore& tx_semaphore() = 0;

        /// @brief
        ///     Connects an interrupt handler to the RXRDY interrupt of the ST16C550.
        /// @param [in] handler
        /// @param [in] arg
        /// @return
        ///     Result status code. 0 for success, otherwise for error.
        virtual int connect_intr_rxrdy(platform::InterruptHandler handler, void* arg) = 0;

        /// @brief 
        ///     Connects an interrupt handler to the THRE interrupt of the ST16C550.
        /// @param [in] handler
        /// @param [in] arg
        /// @return
        ///     Result status code. 0 for success, otherwise for error.
        virtual int connect_intr_thre(platform::InterruptHandler handler, void* arg) = 0;

        /// @brief
        ///     The event handler which is triggered when the ST16C550 starts to transmit data.
        inline virtual void on_transimitting() {}

        /// @brief
        ///     The event handler which is triggered when the ST16C550 has finished transmitting data.
        inline virtual void on_transmitted() {}
    };


    /// @brief
    ///     The driver class for the ST16C550 UART.
    /// @tparam TPtr
    ///     The pointer type for accessing the ST16C550 registers.
    ///     Typically, this is `volatile uint8_t*`, but smart pointers that implement the hooks are also acceptable.
    template<UInt8Ptr TPtr = volatile uint8_t*>
    class St16c550Driver
    {
        using size_t   = std::size_t;
        using uint8_t  = std::uint8_t;
        using uint32_t = std::uint32_t;

    public:
        enum StopBits
        {
            STOP_BITS_1 = 0,
            STOP_BITS_2 = 1,
        };

        enum ParityBits
        {
            PARITY_NONE = 0,
            PARITY_ODD = 1,
            PARITY_EVEN = 3,
        };


        /// @brief
        ///     Creates a new instance of the ST16C550 driver.
        /// @param [in] base_address
        ///     The base address of the ST16C550 registers.
        /// @param [in] clock
        ///     The clock frequency of the ST16C550 in Hz.
        /// @param [in] context
        ///     The platform abstraction context for the ST16C550 driver.
        St16c550Driver(
            TPtr base_address,
            uint8_t clock,
            std::shared_ptr<ISt16c550Context> context)
            : m_baseAddress(base_address)
            , m_clock(clock)
            , m_context(context)
            , m_rxBuffer{}
            , m_txBuffer{}
            , m_opened(false)
            , m_rxrdy_connected(false)
            , m_thre_connected(false)
        {
            if (
                this->m_context == nullptr ||
                this->m_baseAddress == nullptr ||
                this->m_clock == 0)
            {
                throw std::invalid_argument("invalid st16c550 context");
            }
        }


        ~St16c550Driver()
        {
            this->close();
        }


        /// @brief
        ///     Opens a new serial port connection.
        /// @param [in] baud_rate
        ///     The baud rate.
        /// @param [in] data_bits
        ///     The number of data bits.
        /// @param [in] stop_bits
        ///     The number of stop bits.
        /// @param [in] parity
        ///     The parity setting.
        void open(
            uint32_t   baud_rate,
            uint8_t    data_bits,
            StopBits   stop_bits,
            ParityBits parity)
        {
            if (baud_rate == 0)
            {
                throw std::invalid_argument("baud_rate must not be zero");
            }

            if (!this->m_rxrdy_connected)
            {
                this->m_rxrdy_connected = (this->m_context->connect_intr_rxrdy(&St16c550Driver::handle_rxrdy_interrupt, this) == 0);
            }

            if (!this->m_thre_connected)
            {
                this->m_thre_connected = (this->m_context->connect_intr_thre(&St16c550Driver::handle_thre_interrupt, this) == 0);
            }

            const uint32_t divisor = this->m_clock / (16u * baud_rate);
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

            this->set_reg(IER, 0);
            this->set_reg(FCR, static_cast<uint8_t>(FCR_FIFO_ENABLE | FCR_RX_FIFO_RESET | FCR_TX_FIFO_RESET));
            this->set_reg(LCR, static_cast<uint8_t>(lineControl | LCR_DLAB));
            this->set_reg(DLL, static_cast<uint8_t>(divisor & 0xFFu));
            this->set_reg(DLM, static_cast<uint8_t>((divisor >> 8) & 0xFFu));
            this->set_reg(LCR, lineControl);
            this->set_reg(IER, this->m_rxrdy_connected ? IER_RXRDY : 0);

            this->m_opened = true;
        }


        /// @brief
        ///     Closes the serial port connection.
        void close()
        {
            if (!this->m_opened)
            {
                return;
            }

            this->set_reg(IER, 0);
            this->set_reg(FCR, static_cast<uint8_t>(FCR_FIFO_ENABLE | FCR_RX_FIFO_RESET | FCR_TX_FIFO_RESET));
            this->m_opened = false;
        }


        /// @brief
        ///     Writes data to the serial port.
        /// @param [in] buffer
        ///     The buffer containing the data to write.
        /// @param [in] size
        ///     The number of bytes to write.
        /// @return
        ///     The number of bytes actually written.
        /// @remarks
        ///     If TX buffer is full, this function returns `0` immediately instead of blocking.
        size_t write(const uint8_t* buffer, size_t size)
        {
            if (!this->m_opened || buffer == nullptr || size == 0)
            {
                return 0;
            }

            size_t written = 0;
            while (written < size)
            {
                if (!this->m_txBuffer.try_push(buffer[written]))
                {
                    break;
                }

                ++written;
            }

            if (written == 0)
            {
                return 0;
            }

            if (!this->m_context->tx_semaphore().try_acquire(static_cast<uint32_t>(-1)))
            {
                return 0;
            }
            this->m_context->on_transimitting();

            if (this->m_thre_connected)
            {
                this->set_flag(IER, IER_THRE, true);
            }

            return written;
        }


        /// @brief
        ///     Reads data from the serial port.
        /// @param [out] buffer
        ///     The buffer to store the read data.
        /// @param [in] size
        ///     The maximum number of bytes to read.
        /// @param [in] timeout_ms
        ///     The timeout in milliseconds. If `timeout_ms` is zero, this function returns immediately without blocking.
        /// @return
        ///     The number of bytes actually read.
        /// @remarks
        ///     If `timeout_ms` is zero and RX buffer is empty, this function returns `0` immediately instead of blocking.
        size_t read(uint8_t* buffer, size_t size, uint32_t timeout_ms)
        {
            if (!this->m_opened || buffer == nullptr || size == 0)
            {
                return 0;
            }

            if (this->m_rxBuffer.empty())
            {
                if (!this->m_context->rx_semaphore().try_acquire(timeout_ms))
                {
                    return 0;
                }
            }

            size_t readSize = 0;
            while (readSize < size)
            {
                uint8_t value = 0;
                if (!this->m_rxBuffer.try_pop(value))
                {
                    break;
                }

                buffer[readSize] = value;
                ++readSize;
            }

            return readSize;
        }


        /// @brief
        ///     Returns the number of bytes immediately available to read from the serial port.
        /// @return
        ///     The number of bytes available to read.
        size_t available() const
        {
            return this->m_opened ? this->m_rxBuffer.size() : 0;
        }

    private:
        static const size_t RX_BUFFER_SIZE = 1024;
        static const size_t TX_BUFFER_SIZE = 1024;

        enum RegisterIndex
        {
            RHR = 0, ///< [-/r] RX Holding Register
            THR = 0, ///< [w/-] TX Holding Register
            IER = 1, ///< [w/r] Interrupt Enable Register
            FCR = 2, ///< [w/-] FIFO Control Register
            ISR = 2, ///< [-/r] Interrupt Status Register
            LCR = 3, ///< [w/r] Line Control Register
            MCR = 4, ///< [w/r] Modem Control Register
            LSR = 5, ///< [-/r] Line Status Register
            MSR = 6, ///< [-/r] Modem Status Register
            SPR = 7, ///< [w/r] Scratchpad Register

            DLL = 0, ///< [w/r] Divisor Latch Low (when DLAB=1)
            DLM = 1, ///< [w/r] Divisor Latch High (when DLAB=1)
        };

        enum RegisterFlags
        {
            IER_RXRDY = 0x01,
            IER_THRE = 0x02,

            FCR_FIFO_ENABLE = 0x01,
            FCR_RX_FIFO_RESET = 0x02,
            FCR_TX_FIFO_RESET = 0x04,

            LCR_WORD_LENGTH_5 = 0x00,
            LCR_WORD_LENGTH_6 = 0x01,
            LCR_WORD_LENGTH_7 = 0x02,
            LCR_WORD_LENGTH_8 = 0x03,
            LCR_STOP_BITS = 0x04,
            LCR_PARITY_ENABLE = 0x08,
            LCR_EVEN_PARITY = 0x10,
            LCR_DLAB = 0x80,

            LSR_DATA_READY = 0x01,
            LSR_THR_EMPTY = 0x20,
        };

        uint32_t                             const m_clock;
        TPtr                                 const m_baseAddress;
        std::shared_ptr<ISt16c550Context>    const m_context;
        platform::RingBuffer<RX_BUFFER_SIZE>       m_rxBuffer;
        platform::RingBuffer<TX_BUFFER_SIZE>       m_txBuffer;

        bool m_opened;
        bool m_rxrdy_connected;
        bool m_thre_connected;


        uint8_t get_reg(RegisterIndex index) const
        {
            return this->m_baseAddress[index];
        }


        void set_reg(RegisterIndex index, uint8_t value)
        {
            this->m_baseAddress[index] = value;
        }


        bool has_flag(RegisterIndex index, RegisterFlags flag) const
        {
            uint8_t value = this->m_baseAddress[index];
            return (value & flag) != 0;
        }


        void set_flag(RegisterIndex index, RegisterFlags flag, bool value)
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


        void handle_rxrdy_interrupt()
        {
            if (!this->m_opened)
            {
                return;
            }

            bool received = false;
            while (has_flag(LSR, LSR_DATA_READY))
            {
                const uint8_t value = get_reg(RHR);
                if (!this->m_rxBuffer.try_push(value))
                {
                    break;
                }

                received = true;
            }

            if (received)
            {
                this->m_context->rx_semaphore().release();
            }
        }


        static void handle_rxrdy_interrupt(void* arg)
        {
            if (arg == nullptr)
            {
                return;
            }

            static_cast<St16c550Driver*>(arg)->handle_rxrdy_interrupt();
        }


        void handle_thre_interrupt()
        {
            if (!this->m_opened)
            {
                return;
            }

            uint8_t value = 0;
            if (!this->m_txBuffer.try_pop(value))
            {
                this->set_flag(IER, IER_THRE, false);
                this->m_context->on_transmitted();
                this->m_context->tx_semaphore().release();
                return;
            }

            this->set_reg(THR, value);
            this->m_context->tx_semaphore().release();
        }


        static void handle_thre_interrupt(void* arg)
        {
            if (arg == nullptr)
            {
                return;
            }

            static_cast<St16c550Driver*>(arg)->handle_thre_interrupt();
        }


    };
}

#endif /* EMBEDDEDTOOLKITS_ST16C550_DRIVER_H */
