#if !defined(EMBEDDEDTOOLKITS_ST16C550_DRIVER_H)
#define EMBEDDEDTOOLKITS_ST16C550_DRIVER_H
#include <memory>
#include <cstddef>
#include <cstdint>
#include "IBinarySemaphore.h"
#include "InterruptHandler.h"
#include "RingBuffer.h"


namespace embedded::st16c550
{
    /// @brief The platform abstraction interface for the ST16C550 driver.
    class ISt16c550Context
    {
        using size_t   = std::size_t;
        using uint8_t  = std::uint8_t;
        using uint32_t = std::uint32_t;

    public:
        virtual ~ISt16c550Context();

        /// @brief Gets the base address of the ST16C550 registers.
        virtual volatile uint8_t* base_address() const = 0;

        /// @brief Gets the clock frequency of the ST16C550 in Hz.
        virtual uint32_t clock() const = 0;

        /// @brief Gets the binary semaphore for the RX buffer of the ST16C550.
        virtual platform::IBinarySemaphore& rx_semaphore() = 0;

        /// @brief Gets the binary semaphore for the TX buffer of the ST16C550.
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
        virtual void on_transimitting();

        /// @brief
        ///     The event handler which is triggered when the ST16C550 has finished transmitting data.
        virtual void on_transmitted();
    };


    /// @brief The driver class for the ST16C550 UART.
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
        /// @param [in] context
        ///     The platform abstraction context for the ST16C550 driver.
        St16c550Driver(std::unique_ptr<ISt16c550Context>&& context);

        ~St16c550Driver();

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
            ParityBits parity);

        /// @brief
        ///     Closes the serial port connection.
        void close();

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
        size_t write(const uint8_t* buffer, size_t size);

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
        size_t read(uint8_t* buffer, size_t size, uint32_t timeout_ms);

        /// @brief
        ///     Returns the number of bytes immediately available to read from the serial port.
        /// @return
        ///     The number of bytes available to read.
        size_t available() const;

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
        volatile uint8_t*                    const m_baseAddress;
        std::unique_ptr<ISt16c550Context>    const m_context;
        platform::RingBuffer<RX_BUFFER_SIZE>       m_rxBuffer;
        platform::RingBuffer<TX_BUFFER_SIZE>       m_txBuffer;

        bool m_opened;
        bool m_rxrdy_connected;
        bool m_thre_connected;

        uint8_t get_reg(RegisterIndex index) const;
        void set_reg(RegisterIndex index, uint8_t value);
        bool has_flag(RegisterIndex index, RegisterFlags flag) const;
        void set_flag(RegisterIndex index, RegisterFlags flag, bool value);

        void handle_rxrdy_interrupt();
        static void handle_rxrdy_interrupt(void* arg);

        void handle_thre_interrupt();
        static void handle_thre_interrupt(void* arg);
    };
}

#endif /* EMBEDDEDTOOLKITS_ST16C550_DRIVER_H */
