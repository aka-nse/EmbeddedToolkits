#if !defined(EMBEDDEDTOOLKITS_ST16C550_DRIVER_H)
#define EMBEDDEDTOOLKITS_ST16C550_DRIVER_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include "platform.h"


namespace embedded { namespace st16c550 {

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
        ///     Connects an interrupt handler to the interrupt of the ST16C550.
        /// @param [in] handler
        /// @param [in] arg
        /// @return
        ///     Result status code. 0 for success, otherwise for error.
        virtual int connect_intr(platform::InterruptHandler handler, void* arg) = 0;

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
    template<EMBEDDEDTOOLKITS_CONCEPTS(platform::UInt8Ptr) TPtr = volatile uint8_t*>
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

        enum McrFlags
        {
            MCR_LOOPBACK = 0x10,
            MCR_OP2      = 0x08,
            MCR_OP1      = 0x04,
            MCR_RTS      = 0x02,
            MCR_DTR      = 0x01,
        };

        enum MsrFlags
        {
            MSR_CD        = 0x80,
            MSR_RI        = 0x40,
            MSR_DSR       = 0x20,
            MSR_CTS       = 0x10,
            MSR_DELTA_CD  = 0x08,
            MSR_DELTA_RI  = 0x04,
            MSR_DELTA_DSR = 0x02,
            MSR_DELTA_CTS = 0x01,
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
            uint32_t clock,
            std::shared_ptr<ISt16c550Context> context)
            : m_baseAddress(base_address)
            , m_clock(clock)
            , m_context(context)
            , m_rxBuffer{}
            , m_txBuffer{}
            , m_opened(false)
            , m_intr_connected(false)
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

            const uint32_t divisor = this->m_clock / (16u * baud_rate);
            if (divisor == 0 || divisor > 0xFFFFu)
            {
                throw std::invalid_argument("baud_rate is out of range");
            }

            // disables every interrupts
            this->set_reg(IER, 0);

            if (!this->m_intr_connected)
            {
                this->m_intr_connected = (this->m_context->connect_intr(&St16c550Driver::handle_interrupt, this) == 0);
            }

            if(!this->m_intr_connected)
            {
                throw std::runtime_error("interrupt handler registration has been failed.");
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

            // sets up buffer queue
            this->set_reg(FCR, static_cast<uint8_t>(FCR_FIFO_ENABLE | FCR_RX_FIFO_RESET | FCR_TX_FIFO_RESET));

            // configures clock
            this->set_reg(LCR, static_cast<uint8_t>(lineControl | LCR_DLAB));
            this->set_reg(DLL, static_cast<uint8_t>((divisor >> 0) & 0xFFu));
            this->set_reg(DLM, static_cast<uint8_t>((divisor >> 8) & 0xFFu));
            this->set_reg(LCR, lineControl);

            // enables RXRDY interrupt to receive data
            this->set_reg(IER, IER_RXRDY | IER_RLS);

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
        ///     Gets current baud rate.
        /// @return
        ///     The baud rate.
        uint32_t get_baud_rate() const
        {
            uint8_t lcr, dll, dlm;
            {
                St16c550Driver* this_ = const_cast<St16c550Driver*>(this);
                lcr = this_->get_reg(LCR);
                this_->set_reg(LCR, lcr | LCR_DLAB);
                dll = this_->get_reg(DLL);
                dlm = this_->get_reg(DLM);
                this_->set_reg(LCR, lcr & ~LCR_DLAB);
            }
            uint32_t divisor = (dlm << 8) | dll;
            return m_clock / (16 * divisor);
        }

        /// @brief
        ///     Sets MCR register value.
        /// @param value
        ///     The value to set to MCR register.
        void set_mcr_reg(uint8_t value)
        {
            this->set_reg(MCR, value);
        }


        /// @brief
        ///     Gets MCR register value.
        /// @return
        ///     MCR register value.
        uint8_t get_mcr_reg() const
        {
            return this->get_reg(MCR);
        }


        /// @brief
        ///     Gets MSR register value.
        /// @return
        ///     MSR register value.
        uint8_t get_msr_reg() const
        {
            return this->get_reg(MSR);
        }


        /// @brief
        ///     Writes data to the serial port.
        /// @param [in] buffer
        ///     The buffer containing the data to write.
        /// @param [in] size
        ///     The number of bytes to write.
        /// @param [in] timeout_ms
        ///     The timeout in milliseconds. If `timeout_ms` is zero, this function returns immediately without blocking.
        /// @return
        ///     The number of bytes actually written.
        /// @remarks
        ///     If TX buffer is full, this function returns `0` immediately instead of blocking.
        size_t write(const uint8_t* buffer, size_t size, uint32_t timeout_ms = static_cast<uint32_t>(-1))
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

            // calls pre-transmit hook
            this->m_context->on_transimitting();

            // locks tx_semaphore to allow THRE handler to release it
            this->m_context->tx_semaphore().try_acquire(0);

            // enables THRE interrupt to start transmitting
            this->set_flag(IER, IER_THRE, true);

            // waits txBuffer to get empty
            if (!this->m_context->tx_semaphore().try_acquire(timeout_ms))
            {
                this->set_flag(IER, IER_THRE, false);
                
                uint8_t _;
                while(this->m_txBuffer.try_pop(_))
                {
                    --written;
                }
                this->m_context->tx_semaphore().release();
            }

            // calls post-transmit hook
            this->m_context->on_transmitted();

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
        size_t read(uint8_t* buffer, size_t size, uint32_t timeout_ms = static_cast<uint32_t>(-1))
        {
            if (!this->m_opened || buffer == nullptr || size == 0)
            {
                return 0;
            }

            if (this->m_rxBuffer.empty())
            {
                if (!this->m_context->rx_semaphore().try_acquire(timeout_ms))
                {
                    this->m_context->rx_semaphore().release();
                    return 0;
                }
            }

            // pauses RXRDY interrupt to take data from rxBuffer
            this->set_flag(IER, IER_RXRDY, false);

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

            // resumes RXRDY interrupt
            this->set_flag(IER, IER_RXRDY, true);

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

        enum IerFlags
        {
            IER_RXRDY = 0x01,
            IER_THRE = 0x02,
            IER_RLS = 0x4,
        };

        enum FcrFlags
        {
            FCR_FIFO_ENABLE = 0x01,
            FCR_RX_FIFO_RESET = 0x02,
            FCR_TX_FIFO_RESET = 0x04,
        };

        enum LcrFlags
        {
            LCR_WORD_LENGTH_5 = 0x00,
            LCR_WORD_LENGTH_6 = 0x01,
            LCR_WORD_LENGTH_7 = 0x02,
            LCR_WORD_LENGTH_8 = 0x03,
            LCR_STOP_BITS = 0x04,
            LCR_PARITY_ENABLE = 0x08,
            LCR_EVEN_PARITY = 0x10,
            LCR_DLAB = 0x80,
        };

        enum LsrFlags
        {
            LSR_DATA_READY = 0x01,
            LSR_EOVERRUN   = 0x02,
            LSR_EPARITY    = 0x04,
            LSR_EFRAMING   = 0x08,
            LSR_BREAK      = 0x10,
            LSR_THR_EMPTY  = 0x20,
            LSR_TR_EMPTY   = 0x40,
            LSR_EFIFO      = 0x80,
        };
        
        enum InterruptSource
		{
        	INTR_MASK          = 0xF,
        	
        	INTR_NO            = 0x1,
			INTR_LSR           = 0x6,
			INTR_RXRDY         = 0x4,
			INTR_RXRDY_TIMEOUT = 0xC,
			INTR_TXRDY         = 0x2,
			INTR_MSR           = 0x0,
		};

        uint32_t                             const m_clock;
        TPtr                                 const m_baseAddress;
        std::shared_ptr<ISt16c550Context>    const m_context;
        platform::RingBuffer<RX_BUFFER_SIZE>       m_rxBuffer;
        platform::RingBuffer<TX_BUFFER_SIZE>       m_txBuffer;

        bool m_opened;
        bool m_intr_connected;


        uint8_t get_reg(RegisterIndex index) const
        {
            return this->m_baseAddress[index];
        }


        void set_reg(RegisterIndex index, uint8_t value)
        {
            this->m_baseAddress[index] = value;
        }


        bool has_flag(RegisterIndex index, uint8_t flag) const
        {
            uint8_t value = this->m_baseAddress[index];
            return (value & flag) != 0;
        }


        void set_flag(RegisterIndex index, uint8_t flag, bool value)
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


        void handle_thre_interrupt()
        {
            if (!this->m_opened)
            {
                return;
            }

            uint8_t value = 0;
            if (this->m_txBuffer.try_pop(value))
            {
                this->set_reg(THR, value);
            }
            else
            {
                // disables THRE interrupt to finish transmitting
                this->set_flag(IER, IER_THRE, false);

                // txBuffer got empty so releases blocked operation of `write`
                this->m_context->tx_semaphore().release();
            }
        }


        void handle_err_interrupt()
        {
        }


        static void handle_interrupt(void* arg)
        {
            if (arg == nullptr)
            {
                return;
            }

            auto this_ = static_cast<St16c550Driver*>(arg);
            auto lsr = this_->get_reg(LSR);
            if((lsr & LSR_THR_EMPTY) != 0)
            {
                this_->handle_thre_interrupt();
            }
            if((lsr & LSR_DATA_READY) != 0)
            {
                this_->handle_rxrdy_interrupt();
            }
            if((lsr & (LSR_EOVERRUN | LSR_EPARITY | LSR_EFRAMING | LSR_EFIFO)) != 0)
            {
                this_->handle_err_interrupt();
            }
        }

    };

} }

#endif /* EMBEDDEDTOOLKITS_ST16C550_DRIVER_H */
