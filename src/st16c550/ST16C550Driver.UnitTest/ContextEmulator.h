#pragma once
#include "St16c550Driver.h"
#include <semaphore>


class StlSemaphore : public embedded::platform::IBinarySemaphore
{
public:
    StlSemaphore();
    ~StlSemaphore() override;
    bool try_acquire(uint32_t timeout_ms) override;
    void release() override;

private:
    std::binary_semaphore m_semaphore;
};

class ContextEmulator : public embedded::st16c550::ISt16c550Context
{
public:
    ContextEmulator();
    ~ContextEmulator() override;
    volatile uint8_t* base_address() const override;
    uint32_t clock() const override;
    embedded::platform::IBinarySemaphore& rx_semaphore() override;
    embedded::platform::IBinarySemaphore& tx_semaphore() override;
    int connect_intr_rxrdy(embedded::platform::InterruptHandler handler, void* arg) override;
    int connect_intr_thre(embedded::platform::InterruptHandler handler, void* arg) override;

    /// @brief
    ///     As a time-triggered architecture, handles one frame state updation.
    ///     
    void handle_frame_triggered();

private:
    static const size_t REGISTER_SIZE = 8;

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

    volatile std::uint8_t m_currreg[REGISTER_SIZE];
    std::uint8_t m_prevreg[REGISTER_SIZE];
    std::uint8_t m_dll;
    std::uint8_t m_dlm;
    StlSemaphore m_rx_semaphore;
    StlSemaphore m_tx_semaphore;
    embedded::platform::InterruptHandler m_rxrdy_handler;
    embedded::platform::InterruptHandler m_thre_handler;
    void* m_rxrdy_arg;
    void* m_thre_arg;
};