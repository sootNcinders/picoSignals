#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include <string.h>

#ifndef RFM95_H
#define RFM95_H

// Max number of octets the LORA Rx/Tx FIFO can hold
#define RFM95_FIFO_SIZE 255

// This is the maximum number of bytes that can be carried by the LORA.
// We use some for headers, keeping fewer for RadioHead messages
#define RFM95_MAX_PAYLOAD_LEN RFM95_FIFO_SIZE

// The length of the headers we add.
// The headers are inside the LORA's payload
#define RFM95_HEADER_LEN 4

// This is the maximum message length that can be supported by this driver. 
// Can be pre-defined to a smaller size (to save SRAM) prior to including this header
// Here we allow for 1 byte message length, 4 bytes headers, user data and 2 bytes of FCS
#ifndef RFM95_MAX_MESSAGE_LEN
 #define RFM95_MAX_MESSAGE_LEN (RFM95_MAX_PAYLOAD_LEN - RFM95_HEADER_LEN)
#endif

// The crystal oscillator frequency of the module
#define RFM95_FXOSC 32000000.0

// The Frequency Synthesizer step = RFM95_FXOSC / 2^^19
#define RFM95_FSTEP  (RFM95_FXOSC / 524288)

// Register names (LoRa Mode, from table 85)
#define RFM95_REG_00_FIFO                                0x00
#define RFM95_REG_01_OP_MODE                             0x01
#define RFM95_REG_02_RESERVED                            0x02
#define RFM95_REG_03_RESERVED                            0x03
#define RFM95_REG_04_RESERVED                            0x04
#define RFM95_REG_05_RESERVED                            0x05
#define RFM95_REG_06_FRF_MSB                             0x06
#define RFM95_REG_07_FRF_MID                             0x07
#define RFM95_REG_08_FRF_LSB                             0x08
#define RFM95_REG_09_PA_CONFIG                           0x09
#define RFM95_REG_0A_PA_RAMP                             0x0a
#define RFM95_REG_0B_OCP                                 0x0b
#define RFM95_REG_0C_LNA                                 0x0c
#define RFM95_REG_0D_FIFO_ADDR_PTR                       0x0d
#define RFM95_REG_0E_FIFO_TX_BASE_ADDR                   0x0e
#define RFM95_REG_0F_FIFO_RX_BASE_ADDR                   0x0f
#define RFM95_REG_10_FIFO_RX_CURRENT_ADDR                0x10
#define RFM95_REG_11_IRQ_FLAGS_MASK                      0x11
#define RFM95_REG_12_IRQ_FLAGS                           0x12
#define RFM95_REG_13_RX_NB_BYTES                         0x13
#define RFM95_REG_14_RX_HEADER_CNT_VALUE_MSB             0x14
#define RFM95_REG_15_RX_HEADER_CNT_VALUE_LSB             0x15
#define RFM95_REG_16_RX_PACKET_CNT_VALUE_MSB             0x16
#define RFM95_REG_17_RX_PACKET_CNT_VALUE_LSB             0x17
#define RFM95_REG_18_MODEM_STAT                          0x18
#define RFM95_REG_19_PKT_SNR_VALUE                       0x19
#define RFM95_REG_1A_PKT_RSSI_VALUE                      0x1a
#define RFM95_REG_1B_RSSI_VALUE                          0x1b
#define RFM95_REG_1C_HOP_CHANNEL                         0x1c
#define RFM95_REG_1D_MODEM_CONFIG1                       0x1d
#define RFM95_REG_1E_MODEM_CONFIG2                       0x1e
#define RFM95_REG_1F_SYMB_TIMEOUT_LSB                    0x1f
#define RFM95_REG_20_PREAMBLE_MSB                        0x20
#define RFM95_REG_21_PREAMBLE_LSB                        0x21
#define RFM95_REG_22_PAYLOAD_LENGTH                      0x22
#define RFM95_REG_23_MAX_PAYLOAD_LENGTH                  0x23
#define RFM95_REG_24_HOP_PERIOD                          0x24
#define RFM95_REG_25_FIFO_RX_BYTE_ADDR                   0x25
#define RFM95_REG_26_MODEM_CONFIG3                       0x26

#define RFM95_REG_27_PPM_CORRECTION                      0x27
#define RFM95_REG_28_FEI_MSB                             0x28
#define RFM95_REG_29_FEI_MID                             0x29
#define RFM95_REG_2A_FEI_LSB                             0x2a
#define RFM95_REG_2C_RSSI_WIDEBAND                       0x2c
#define RFM95_REG_31_DETECT_OPTIMIZE                     0x31
#define RFM95_REG_33_INVERT_IQ                           0x33
#define RFM95_REG_37_DETECTION_THRESHOLD                 0x37
#define RFM95_REG_39_SYNC_WORD                           0x39

#define RFM95_REG_40_DIO_MAPPING1                        0x40
#define RFM95_REG_41_DIO_MAPPING2                        0x41
#define RFM95_REG_42_VERSION                             0x42

#define RFM95_REG_4B_TCXO                                0x4b
#define RFM95_REG_4D_PA_DAC                              0x4d
#define RFM95_REG_5B_FORMER_TEMP                         0x5b
#define RFM95_REG_61_AGC_REF                             0x61
#define RFM95_REG_62_AGC_THRESH1                         0x62
#define RFM95_REG_63_AGC_THRESH2                         0x63
#define RFM95_REG_64_AGC_THRESH3                         0x64

// RFM95_REG_01_OP_MODE                             0x01
#define RFM95_LONG_RANGE_MODE                       0x80
#define RFM95_ACCESS_SHARED_REG                     0x40
#define RFM95_LOW_FREQUENCY_MODE                    0x08
#define RFM95_MODE                                  0x07
#define RFM95_MODE_SLEEP                            0x00
#define RFM95_MODE_STDBY                            0x01
#define RFM95_MODE_FSTX                             0x02
#define RFM95_MODE_TX                               0x03
#define RFM95_MODE_FSRX                             0x04
#define RFM95_MODE_RXCONTINUOUS                     0x05
#define RFM95_MODE_RXSINGLE                         0x06
#define RFM95_MODE_CAD                              0x07

// RFM95_REG_09_PA_CONFIG                           0x09
#define RFM95_PA_SELECT                             0x80
#define RFM95_MAX_POWER                             0x70
#define RFM95_OUTPUT_POWER                          0x0f

// RFM95_REG_0A_PA_RAMP                             0x0a
#define RFM95_LOW_PN_TX_PLL_OFF                     0x10
#define RFM95_PA_RAMP                               0x0f
#define RFM95_PA_RAMP_3_4MS                         0x00
#define RFM95_PA_RAMP_2MS                           0x01
#define RFM95_PA_RAMP_1MS                           0x02
#define RFM95_PA_RAMP_500US                         0x03
#define RFM95_PA_RAMP_250US                         0x04
#define RFM95_PA_RAMP_125US                         0x05
#define RFM95_PA_RAMP_100US                         0x06
#define RFM95_PA_RAMP_62US                          0x07
#define RFM95_PA_RAMP_50US                          0x08
#define RFM95_PA_RAMP_40US                          0x09
#define RFM95_PA_RAMP_31US                          0x0a
#define RFM95_PA_RAMP_25US                          0x0b
#define RFM95_PA_RAMP_20US                          0x0c
#define RFM95_PA_RAMP_15US                          0x0d
#define RFM95_PA_RAMP_12US                          0x0e
#define RFM95_PA_RAMP_10US                          0x0f

// RFM95_REG_0B_OCP                                 0x0b
#define RFM95_OCP_ON                                0x20
#define RFM95_OCP_TRIM                              0x1f

// RFM95_REG_0C_LNA                                 0x0c
#define RFM95_LNA_GAIN                              0xe0
#define RFM95_LNA_GAIN_G1                           0x20
#define RFM95_LNA_GAIN_G2                           0x40
#define RFM95_LNA_GAIN_G3                           0x60                
#define RFM95_LNA_GAIN_G4                           0x80
#define RFM95_LNA_GAIN_G5                           0xa0
#define RFM95_LNA_GAIN_G6                           0xc0
#define RFM95_LNA_BOOST_LF                          0x18
#define RFM95_LNA_BOOST_LF_DEFAULT                  0x00
#define RFM95_LNA_BOOST_HF                          0x03
#define RFM95_LNA_BOOST_HF_DEFAULT                  0x00
#define RFM95_LNA_BOOST_HF_150PC                    0x03

// RFM95_REG_11_IRQ_FLAGS_MASK                      0x11
#define RFM95_RX_TIMEOUT_MASK                       0x80
#define RFM95_RX_DONE_MASK                          0x40
#define RFM95_PAYLOAD_CRC_ERROR_MASK                0x20
#define RFM95_VALID_HEADER_MASK                     0x10
#define RFM95_TX_DONE_MASK                          0x08
#define RFM95_CAD_DONE_MASK                         0x04
#define RFM95_FHSS_CHANGE_CHANNEL_MASK              0x02
#define RFM95_CAD_DETECTED_MASK                     0x01

// RFM95_REG_12_IRQ_FLAGS                           0x12
#define RFM95_RX_TIMEOUT                            0x80
#define RFM95_RX_DONE                               0x40
#define RFM95_PAYLOAD_CRC_ERROR                     0x20
#define RFM95_VALID_HEADER                          0x10
#define RFM95_TX_DONE                               0x08
#define RFM95_CAD_DONE                              0x04
#define RFM95_FHSS_CHANGE_CHANNEL                   0x02
#define RFM95_CAD_DETECTED                          0x01

// RFM95_REG_18_MODEM_STAT                          0x18
#define RFM95_RX_CODING_RATE                        0xe0
#define RFM95_MODEM_STATUS_CLEAR                    0x10
#define RFM95_MODEM_STATUS_HEADER_INFO_VALID        0x08
#define RFM95_MODEM_STATUS_RX_ONGOING               0x04
#define RFM95_MODEM_STATUS_SIGNAL_SYNCHRONIZED      0x02
#define RFM95_MODEM_STATUS_SIGNAL_DETECTED          0x01

// RFM95_REG_1C_HOP_CHANNEL                         0x1c
#define RFM95_PLL_TIMEOUT                           0x80
#define RFM95_RX_PAYLOAD_CRC_IS_ON                  0x40
#define RFM95_FHSS_PRESENT_CHANNEL                  0x3f

// RFM95_REG_1D_MODEM_CONFIG1                       0x1d
#define RFM95_BW                                    0xf0

#define RFM95_BW_7_8KHZ                             0x00
#define RFM95_BW_10_4KHZ                            0x10
#define RFM95_BW_15_6KHZ                            0x20
#define RFM95_BW_20_8KHZ                            0x30
#define RFM95_BW_31_25KHZ                           0x40
#define RFM95_BW_41_7KHZ                            0x50
#define RFM95_BW_62_5KHZ                            0x60
#define RFM95_BW_125KHZ                             0x70
#define RFM95_BW_250KHZ                             0x80
#define RFM95_BW_500KHZ                             0x90
#define RFM95_CODING_RATE                           0x0e
#define RFM95_CODING_RATE_4_5                       0x02
#define RFM95_CODING_RATE_4_6                       0x04
#define RFM95_CODING_RATE_4_7                       0x06
#define RFM95_CODING_RATE_4_8                       0x08
#define RFM95_IMPLICIT_HEADER_MODE_ON               0x01

// RFM95_REG_1E_MODEM_CONFIG2                       0x1e
#define RFM95_SPREADING_FACTOR                      0xf0
#define RFM95_SPREADING_FACTOR_64CPS                0x60
#define RFM95_SPREADING_FACTOR_128CPS               0x70
#define RFM95_SPREADING_FACTOR_256CPS               0x80
#define RFM95_SPREADING_FACTOR_512CPS               0x90
#define RFM95_SPREADING_FACTOR_1024CPS              0xa0
#define RFM95_SPREADING_FACTOR_2048CPS              0xb0
#define RFM95_SPREADING_FACTOR_4096CPS              0xc0
#define RFM95_TX_CONTINUOUS_MODE                    0x08

#define RFM95_PAYLOAD_CRC_ON                        0x04
#define RFM95_SYM_TIMEOUT_MSB                       0x03

// RFM95_REG_26_MODEM_CONFIG3
#define RFM95_MOBILE_NODE                           0x08 // HopeRF term
#define RFM95_LOW_DATA_RATE_OPTIMIZE                0x08 // Semtechs term
#define RFM95_AGC_AUTO_ON                           0x04

// RFM95_REG_4B_TCXO                                0x4b
#define RFM95_TCXO_TCXO_INPUT_ON                    0x10

// RFM95_REG_4D_PA_DAC                              0x4d
#define RFM95_PA_DAC_DISABLE                        0x04
#define RFM95_PA_DAC_ENABLE                         0x07

#define RFM95_BROADCAST_ADDR                        0xFF

#define RFM95_WRITE_BIT                             0x80

#define RFM95_NUM_INTERRUPTS                        4

//Number of packets to hold from receiver
#define RFM95_BUF_SIZE                              4

#define RFM95_INT_MODE                              GPIO_IRQ_EDGE_RISE

typedef enum
    {
	RFMModeInitialising = 0, ///< Transport is initialising. Initial default value until init() is called..
	RFMModeSleep,            ///< Transport hardware is in low power sleep mode (if supported)
	RFMModeIdle,             ///< Transport is idle.
	RFMModeTx,               ///< Transport is in the process of transmitting a message.
	RFMModeRx,               ///< Transport is in the process of receiving a message.
    RFMModeRxSingle,         ///< Transport will receive one message and then go idle
	RFMModeCad               ///< Transport is in the process of detecting channel activity (if supported)
    } RHMode;

static const char* modeName[] = {"INIT", "SLEEP", "IDLE", "TX", "RX", "CAD"};

class RFM95
{
    public:
        /// @brief  Constructor for the RFM95 radio instance
        /// @param spiBus Instance of pico spi bus. spi0, spi1, or spi_default
        /// @param csPin Chip select pin
        /// @param intPin Interrupt pin
        RFM95(spi_inst_t *spiBus, uint8_t csPin, uint8_t intPin, uint8_t address);

        /// @brief Initializes RFM95 Radio
        /// @return true if initialization successful 
        bool init();

        /// @brief Prints the value of all registers to STDIO
        void printRegisters();

        /// @brief Prints the value of Reg 0x01
        void printOpMode();

        /// @brief Checks if a valid message is available
        /// @return returns true if message is available
        bool available();

        /// @brief Copies received message to app buffer
        /// @param buf Target buffer
        /// @param len Number of bytes to copy, Reset to number of bytes copied
        void recv(uint8_t *buf, uint8_t *len, uint8_t *from, uint8_t *to);

        /// @brief Copies data buffer to TX buffer to transmissit
        /// @param data data buffer to transmit 
        /// @param len number of bytes to transmit
        bool send(uint8_t to, const uint8_t *data, uint8_t len);

        /// @brief Checks for LoRa activity on the selected frequency using CAD
        /// @return true if activity is detected on the channel
        bool channelActive();

        /// @brief Sets the lengths of the preamble in bytes. Should be the same in all nodes
        /// @param bytes Preamble length in bytes. Default is 8
        void setPreambleLength(uint16_t bytes);

        /// @brief Sets the center frequency of the radio
        /// @param center Frequency in MHz. 902.0 to 928.0 for US
        /// @return true if the selected frequency is in range
        bool setFrequency(float center);

        /// @brief Sets Transmitter value in dBm
        /// @param power +2 to +20dBm transmission power
        void setTxPower(uint8_t power);

        /// @brief Sets the radio signal bandwidth
        /// sbw ranges and resultant settings are as follows:-
        /// sbw range    actual bw (kHz)
        /// 0-7800       7.8
        /// 7801-10400   10.4
        /// 10401-15600  15.6
        /// 15601-20800  20.8
        /// 20801-31250  31.25
        /// 31251-41700	 41.7
        /// 41701-62500	 62.5
        /// 62501-12500  125.0
        /// 12501-250000 250.0
        /// >250000      500.0
        /// @param sbw long, signal bandwidth e.g. 125000
        void setSignalBandwidth(long sbw);

        /// @brief Sets the coding rate to 4/5, 4/6, 4/7 or 4/8.
        /// Valid denominator values are 5, 6, 7 or 8. A value of 5 sets the coding rate to 4/5 etc.
        /// Values below 5 are clamped at 5
        /// values above 8 are clamped at 8.
        /// Default for all standard modem config options is 4/5.
        /// @param denominator uint8_t range 5..8
        void setCodingRate(uint8_t denominator);

        /// @brief Sets the radio spreading factor.
        /// valid values are 6 through 12.
        /// Out of range values below 6 are clamped to 6
        /// Out of range values above 12 are clamped to 12
        /// See Semtech DS SX1276/77/78/79 page 27 regarding SF6 configuration.
        /// @param sf sf (spreading factor 6..12)
        void setSpreadingFactor(uint8_t sf);

        /// @brief Puts RFM95 in idle mode
        void setModeIdle();

        /// @brief Puts RFM95 in RX Continuous Mode
        void setModeRX();

        /// @brief Puts the RFM95 in RX Single Shot Mode
        void setModeRXSingle();

        /// @brief Puts RFM95 in Transmit mode
        void setModeTX();

        /// @brief Put RFM95 to sleep. Clears all modes and buffers
        void setModeSleep();

        //
        void setAddress(uint8_t addr);

        //
        void setCRC(bool onOff);

        /// @brief Returns last Signal to Noise Ratio from the receiver
        /// @return SNR of last message in db
        int lastSNR();

        /// @brief Gets device version from register 0x42
        /// @return device ID 
        uint8_t getDeviceVersion();

        /// @brief Sets driver to accept all messages
        /// @param onOff true to turn on promicuous mode
        void setPromiscuous(bool onOff);

        /// @brief Write data to RFM95 register
        /// @param reg Register Address
        /// @param data data to write
        //void writeRegister(uint8_t reg, uint8_t data);

        /// @brief Read RFM95 register
        /// @param reg Register Address
        /// @param buf buffer to store value
        void readRegister(uint8_t reg, uint8_t *buf);

        /// @brief RFM95 IRQ Handler
        void handleInterrupt(void);

        /// @brief Sets pins for status LEDs
        /// @param rx RX LED pin
        /// @param tx TX LED pin
        void setLEDS(uint8_t rx, uint8_t tx);

        void setTX();

        void clearTX();

        void setRX();

        void clearRX();

        volatile bool rxAlarmSet;
        volatile bool txAlarmSet;
    
    private:
        spi_inst_t* _bus;
        uint8_t _cs;
        uint8_t _int;
        //uint8_t _buf[2];
        volatile uint8_t _mode;

        uint8_t _address;
        bool _promiscuous;

        volatile uint8_t _rxTo;
        volatile uint8_t _rxFrom;
        
        uint8_t _txTo;

        volatile uint8_t _payloadBuf[RFM95_BUF_SIZE][255];
        volatile uint8_t _payloadLen[RFM95_BUF_SIZE];
        volatile uint8_t _head;
        volatile uint8_t _tail;

        volatile int8_t _lastRSSI;
        volatile int8_t _lastSNR;
        volatile bool _cad;

        volatile uint8_t RXled;
        volatile uint8_t TXled;

        /// @brief select this chip
        void chipSelect();

        /// @brief delect this chip
        void chipDeselect();

        /// @brief sets the low data rate flag if symbol time exceeds 16ms
        void setLowDatarate();

    //Statics for interrupt handling
    private:
        static RFM95* _deviceForInterrupt[];

        static uint8_t interruptCount;

        static void isr0();

        static void isr1();

        static void isr2();

        static void isr3();

};

#endif
