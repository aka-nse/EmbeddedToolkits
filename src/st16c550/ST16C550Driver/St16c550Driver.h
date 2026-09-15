#if !defined(ST16C550_DRIVER_H)
#define ST16C550_DRIVER_H
#include <memory>
#include <queue>
#include <stdint.h>


typedef void (*InterruptHandler)(void* arg);
typedef int (*RegisterInterrupt)(InterruptHandler handler, void* arg);


class SemaphoreBase
{
public:
    virtual ~SemaphoreBase();
    virtual void acquire() = 0;
    virtual void release() = 0;
};


class St16c550Driver
{
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


    St16c550Driver(
        uint32_t clock_frequency_hz,
        volatile uint8_t* base_address,
        RegisterInterrupt register_interrupt,
        std::unique_ptr<SemaphoreBase>&& rxSemaphore);

    ~St16c550Driver();

    void open(
        uint32_t baud_rate,
        uint8_t data_bits,
        StopBits stop_bits,
        ParityBits parity);

    void close();

    size_t write(const uint8_t* buffer, size_t size);

    size_t read(uint8_t* buffer, size_t size);

    size_t available() const;

private:
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

    const uint32_t m_clockFrequencyHz;
    volatile uint8_t* m_baseAddress;
    RegisterInterrupt m_registerInterrupt;
    bool m_isOpen;
    bool m_isInterruptRegistered;
    std::queue<uint8_t> m_rxQueue;
    std::unique_ptr<SemaphoreBase> m_rxSemaphore;

    void set_reg(RegisterIndex reg, uint8_t value);

    uint8_t get_reg(RegisterIndex reg) const;

    void configure_baud_rate(uint32_t baud_rate);

    void drain_receive_fifo(bool signal_waiters);

    void handle_interrupt();

    static void interrupt_entry(void* arg);
};


#endif // ST16C550_DRIVER_H
