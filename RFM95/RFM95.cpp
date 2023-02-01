#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "RFM95.h"
#include <string.h>
#include "../utils.c"

//#define RFM95_DEBUG

RFM95* RFM95::_deviceForInterrupt[RFM95_NUM_INTERRUPTS] = {0,0,0,0};
uint8_t RFM95::interruptCount = 0;

int64_t RXalarm(alarm_id_t id, void *user_data)
{
    
    ((RFM95*)user_data)->clearRX();
    ((RFM95*)user_data)->rxAlarmSet = false;
    return 0;
}

int64_t TXalarm(alarm_id_t id, void *user_data)
{
    ((RFM95*)user_data)->clearTX();
    ((RFM95*)user_data)->txAlarmSet = false;
    return 0;
}

RFM95::RFM95(spi_inst_t *spiBus, uint8_t csPin, uint8_t intPin, uint8_t address)
{
    _bus = spiBus;
    _cs = csPin;
    _int = intPin;
    _address = address;
    _mode = RFMModeInitialising;

    RXled = 0xFF;
    TXled = 0xFF;

    rxAlarmSet = false;
    txAlarmSet = false;
}

bool RFM95::init()
{
    uint8_t buf[3];

    /*if(interruptCount >= RFM95_NUM_INTERRUPTS)
    {
        return false;
    }*/

    _head = 0;
    _tail = 0;
    _lastRSSI = 0;
    _lastSNR = 0;

    _promiscuous = true;

    spi_init(_bus, 500 * 1000);//5MBPS

    gpio_init(_int);
    gpio_init(_cs);

    gpio_set_dir(_int, GPIO_IN);
    gpio_set_dir(_cs, GPIO_OUT);

    chipDeselect();

    /*_deviceForInterrupt[interruptCount] = this;

    gpio_set_dormant_irq_enabled(_int, GPIO_IRQ_LEVEL_HIGH, true);

    switch(interruptCount)
    {
        case 0:
            gpio_add_raw_irq_handler(_int, isr0);
            break;

        case 1:
            gpio_add_raw_irq_handler(_int, isr1);
            break;

        case 2:
            gpio_add_raw_irq_handler(_int, isr2);
            break;

        case 3:
            gpio_add_raw_irq_handler(_int, isr3);
            break;
    }
    
    irq_set_enabled(IO_IRQ_BANK0, true);
    
    interruptCount++;*/

    //writeRegister(RFM95_REG_01_OP_MODE, RFM95_MODE_SLEEP | RFM95_LONG_RANGE_MODE);
    buf[0] = RFM95_REG_01_OP_MODE | RFM95_WRITE_BIT;
    buf[1] = RFM95_MODE_SLEEP | RFM95_LONG_RANGE_MODE;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    sleep_ms(10);
    //readRegister(RFM95_REG_01_OP_MODE, _buf);
    buf[0] = RFM95_REG_01_OP_MODE & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0xDD, buf+1, 1);
    chipDeselect();

    if(buf[1] != (RFM95_MODE_SLEEP | RFM95_LONG_RANGE_MODE))
    {
        printf("RFM95 Error! 0x01 = 0x%x\n", buf[1]);
        return false;
    }

    //writeRegister(RFM95_REG_0E_FIFO_TX_BASE_ADDR, 0);
    //writeRegister(RFM95_REG_0F_FIFO_RX_BASE_ADDR, 0);
    buf[0] = RFM95_REG_0E_FIFO_TX_BASE_ADDR | RFM95_WRITE_BIT;
    buf[1] = 0x00;
    buf[2] = 0x00;

    chipSelect();
    spi_write_blocking(_bus, buf, 3);
    chipDeselect();

    //writeRegister(RFM95_REG_0C_LNA, 0x20);
    buf[0] = RFM95_REG_0C_LNA | RFM95_WRITE_BIT;
    buf[1] = 0x20;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    //writeRegister(RFM95_REG_12_IRQ_FLAGS, 0xFF);
    buf[0] = RFM95_REG_12_IRQ_FLAGS | RFM95_WRITE_BIT;
    buf[1] = 0xFF;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    setSignalBandwidth(125);
    setCodingRate(5);
    setSpreadingFactor(7);
    setPreambleLength(8);
    setFrequency(915.0);
    setTxPower(13);
    setCRC(true);

    //printRegisters();
    setModeIdle();

    return true;
}

void RFM95::handleInterrupt(void)
{
    uint8_t buf[2];

    //readRegister(RFM95_REG_12_IRQ_FLAGS, _buf);
    buf[0] = RFM95_REG_12_IRQ_FLAGS & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0x77, buf+1, 1);
    chipDeselect();

    uint8_t irq_flags = buf[1];

    //readRegister(RFM95_REG_1C_HOP_CHANNEL, _buf);
    buf[0] = RFM95_REG_1C_HOP_CHANNEL & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0x88, buf+1, 1);
    chipDeselect();

    uint8_t hop_channel = buf[1];

    readRegister(RFM95_REG_01_OP_MODE, buf);

    #ifdef RFM95_DEBUG
    printf("RFM95 RADIO IRQ MODE: %s REG 0x01: 0x%x FLAGS: 0x%x\n\n", modeName[_mode], buf[0], irq_flags);
    #endif

    if(_mode == RFMModeRx && ((irq_flags & (RFM95_RX_TIMEOUT | RFM95_PAYLOAD_CRC_ERROR)) || !(hop_channel & RFM95_RX_PAYLOAD_CRC_IS_ON)))
    {
        _payloadLen[_head] = 0;
    }
    else if(_mode == RFMModeRx && (irq_flags & RFM95_RX_DONE))
    {
        setRX();

        //readRegister(RFM95_REG_13_RX_NB_BYTES, _buf);
        buf[0] = RFM95_REG_13_RX_NB_BYTES & ~RFM95_WRITE_BIT;

        chipSelect();
        spi_write_blocking(_bus, buf, 1);
        spi_read_blocking(_bus, 0x99, buf+1, 1);
        chipDeselect();

        _payloadLen[_head] = buf[1];

        //readRegister(RFM95_REG_10_FIFO_RX_CURRENT_ADDR, _buf);
        //writeRegister(RFM95_REG_0D_FIFO_ADDR_PTR, _buf[0]);

        buf[0] = RFM95_REG_10_FIFO_RX_CURRENT_ADDR & ~RFM95_WRITE_BIT;

        chipSelect();
        spi_write_blocking(_bus, buf, 1);
        spi_read_blocking(_bus, 0xAA, buf+1, 1);
        chipDeselect();

        buf[0] = RFM95_REG_0D_FIFO_ADDR_PTR | RFM95_WRITE_BIT;
        
        chipSelect();
        spi_write_blocking(_bus, buf, 2);
        chipDeselect();

        buf[0] = RFM95_REG_00_FIFO & ~0x80;
        uint8_t len = _payloadLen[_head];
        uint8_t index = 0;

        chipSelect();
        spi_write_blocking(_bus, buf, 1);
        while(len--)
        {
            spi_read_blocking(_bus, 0, buf+1, 1);
            _payloadBuf[_head][index] = buf[1];
            index++;
        }
        chipDeselect();

        //readRegister(RFM95_REG_19_PKT_SNR_VALUE, _buf);
        buf[0] = RFM95_REG_19_PKT_SNR_VALUE & ~RFM95_WRITE_BIT;

        chipSelect();
        spi_write_blocking(_bus, buf, 1);
        spi_read_blocking(_bus, 0xBB, buf+1, 1);
        chipDeselect();

        _lastSNR = (int8_t)(buf[1]/4);

        //readRegister(RFM95_REG_1A_PKT_RSSI_VALUE, _buf);
        buf[0] = RFM95_REG_1A_PKT_RSSI_VALUE & ~RFM95_WRITE_BIT;

        chipSelect();
        spi_write_blocking(_bus, buf, 1);
        spi_read_blocking(_bus, 0xCC, buf+1, 1);
        chipDeselect();

        _lastRSSI = buf[1];

        if (_lastSNR < 0)
        {
	        _lastRSSI += _lastSNR;
        }
        else
        {
            _lastRSSI = (int)_lastRSSI * 16 / 15;
        }

        _lastRSSI -= 157;

        #ifdef RFM95_DEBUG
        printf("RFM95 RX : LEN=%d SNR=%d RSSI=%d\n", _payloadLen[_head], _lastSNR, _lastRSSI);
        
        for(int i = 0; i < _payloadLen[_head]; i++)
        {
            printf("0x%x ", _payloadBuf[_head][i]);
        }
        printf("\n");
        #endif

        if(_payloadLen[_head] >= RFM95_HEADER_LEN)
        {
            _rxTo = _payloadBuf[_head][0];
            _rxFrom = _payloadBuf[_head][1];

            if(_rxTo == _address || _rxTo == RFM95_BROADCAST_ADDR)
            {
                _head = ((_head + 1) >= RFM95_BUF_SIZE) ? 0:_head+1;
                #ifdef RFM95_DEBUG
                printf("RFM95 HEAD: %d TAIL: %d\n", _head, _tail);
                #endif
            }
            else
            {
                clearRX();
            }
        }

        //writeRegister(RFM95_REG_10_FIFO_RX_CURRENT_ADDR, 0x00);
        buf[0] = RFM95_REG_10_FIFO_RX_CURRENT_ADDR | RFM95_WRITE_BIT;
        buf[1] = 0x00;

        chipSelect();
        spi_write_blocking(_bus, buf, 2);
        chipDeselect();
    }
    else if(_mode == RFMModeTx && (irq_flags & RFM95_TX_DONE))
    {
        if(!txAlarmSet)
        {
            add_alarm_in_ms(20, TXalarm, this, true);
            txAlarmSet = true;
        }
        setModeRX();
    }
    else if(_mode == RFMModeCad && (irq_flags & RFM95_CAD_DONE))
    {
        _cad = irq_flags & RFM95_CAD_DETECTED;
        setModeIdle();
    }

    buf[0] = RFM95_REG_12_IRQ_FLAGS | RFM95_WRITE_BIT;
    buf[1] = 0xFF;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();
}

void RFM95::isr0()
{
    if(_deviceForInterrupt[0])
    {
        _deviceForInterrupt[0]->handleInterrupt();
    }
}

void RFM95::isr1()
{
    if(_deviceForInterrupt[1])
    {
        _deviceForInterrupt[1]->handleInterrupt();
    }
}

void RFM95::isr2()
{
    if(_deviceForInterrupt[2])
    {
        _deviceForInterrupt[2]->handleInterrupt();
    }
}

void RFM95::isr3()
{
    if(_deviceForInterrupt[3])
    {
        _deviceForInterrupt[3]->handleInterrupt();
    }
}

void RFM95::printRegisters()
{
    uint8_t registers[] = { 0x01, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x014, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x4b};
    uint8_t index = 0;
    uint8_t buf[1];
    /*for(int i = 0; i < sizeof(registers); i++)
    {
        readRegister(registers[i], _buf);
        printf("0x%x: 0x%x\n", registers[i], _buf[0]);
    }*/

    printf("RFM95:\n");

    buf[0] = registers[0] & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    for(int i = 0x01; i < 0x65; i++)
    {
        spi_read_blocking(_bus, 0x55, buf, 1);

        if(i == registers[index])
        {
            printf("Reg 0x%x: 0x%x\n", i, buf[0]);
            index++;
        }
    }
    chipDeselect();
}

void RFM95::printOpMode()
{
    uint8_t buf[1];
    readRegister(0x01, buf);
    printf("RFM95 0x01: 0x%x\n", buf[0]);
}

bool RFM95::available()
{
    //handleInterrupt();
    //printf("HEAD: %d TAIL: %d\n", _head, _tail);

    if(_mode != RFMModeRx  && _mode != RFMModeTx)
    {
        setModeIdle();
        sleep_ms(10);
        setModeRX();
    }

    return (_head != _tail);
}

void RFM95::setAddress(uint8_t addr)
{
    _address = addr;
}

void RFM95::recv(uint8_t *buf, uint8_t *len, uint8_t *from, uint8_t *to)
{
    if(!available())
    {
        return;
    }

    //printf("Requested: %d Available: %d\n", *len, _payloadLen[_tail] - RFM95_HEADER_LEN);
    if(buf && len)
    {
        if(*len >= (_payloadLen[_tail] - RFM95_HEADER_LEN))
        {
            if(*len > (_payloadLen[_tail] - RFM95_HEADER_LEN))
            {
                *len = _payloadLen[_tail] - RFM95_HEADER_LEN;
            }

            memcpy(buf, (const void*) (_payloadBuf[_tail] + RFM95_HEADER_LEN), *len);
            _payloadLen[_tail] = 0;

            *from = _rxFrom;
            *to = _rxTo;

            _tail = ((_tail + 1) >= RFM95_BUF_SIZE) ? 0:_tail+1;
        }
    }

    if(!available())
    {
        if(!rxAlarmSet)
        {
            add_alarm_in_ms(60, RXalarm, this, true);
            rxAlarmSet = true;
        }
    }
}

bool RFM95::send(uint8_t to, const uint8_t *data, uint8_t len)
{
    uint8_t buf[5];
    
    absolute_time_t t = get_absolute_time();

    setTX();

    while(_mode != RFMModeIdle && _mode != RFMModeRx)//_buf[0] != RFM95_MODE_STDBY && _buf[0] != RFM95_MODE_RXCONTINUOUS)
    {
        if((absolute_time_diff_us(t, get_absolute_time()) / 1000) >= 1000)
        {
            printf("Mode Fault\n");

            if(!txAlarmSet)
            {
                add_alarm_in_ms(20, TXalarm, this, true);
                txAlarmSet = true;
            }

            return false;
        }
    }

    setModeIdle();

    t = get_absolute_time();
    while(channelActive())
    {  
        if((absolute_time_diff_us(t, get_absolute_time()) / 1000) >= 10000)
        {
            printOpMode();
            nop_sleep_ms(1);
            printf("CAD Fault\n");
            setModeSleep();

            if(!txAlarmSet)
            {
                add_alarm_in_ms(20, TXalarm, this, true);
                txAlarmSet = true;
            }

            return false;
        }
    }

    //Point FIFO to 0x00
    //writeRegister(RFM95_REG_0D_FIFO_ADDR_PTR, 0x00);
    buf[0] = RFM95_REG_0D_FIFO_ADDR_PTR | RFM95_WRITE_BIT;
    buf[1] = 0x00;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    buf[0] = RFM95_REG_00_FIFO | RFM95_WRITE_BIT;
    buf[1] = to;
    buf[2] = _address;
    buf[3] = 0x00;
    buf[4] = 0x00;

    #ifdef RFM95_DEBUG
    printf("RFM95 Sending: ");
    for(int i = 1; i < 5; i++)
    {
        printf("0x%x ", buf[i]);
    }
    for(int i = 0; i < len; i++)
    {
        printf("0x%x ", data[i]);
    }
    printf("\n");
    #endif

    chipSelect();
    spi_write_blocking(_bus, buf, 5);
    spi_write_blocking(_bus, data, len);
    chipDeselect();

    //writeRegister(RFM95_REG_22_PAYLOAD_LENGTH, len + RFM95_HEADER_LEN);
    buf[0] = RFM95_REG_22_PAYLOAD_LENGTH | RFM95_WRITE_BIT;
    buf[1] = len + RFM95_HEADER_LEN;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    setModeTX();

    return true;
}

bool RFM95::channelActive()
{
    uint8_t buf[2];

    buf[0] = RFM95_REG_01_OP_MODE | RFM95_WRITE_BIT;
    buf[1] = RFM95_MODE_CAD;
    
    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    buf[0] = RFM95_REG_40_DIO_MAPPING1 | RFM95_WRITE_BIT;
    buf[1] = 0x80;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    _mode = RFMModeCad;

    absolute_time_t t = get_absolute_time();
    int64_t tDiff = 0;
    while(_mode == RFMModeCad && (tDiff < (1000*1000)))
    {
        nop_sleep_ms(1);
        tDiff = absolute_time_diff_us(t, get_absolute_time());
    }
    
    if(_mode == RFMModeCad)
    {
        printf("RFM95 CAD Timeout time: %d\n", tDiff);
        return true;
        
    }
    else
    {
        return _cad;
    }
}

void RFM95::setPreambleLength(uint16_t bytes)
{
    uint8_t buf[3];

    //writeRegister(0x20, (bytes >> 8) & 0xFF); //Preamble length MSB
    //writeRegister(0x21, bytes & 0xFF); //Preamble length LSB

    buf[0] = RFM95_REG_20_PREAMBLE_MSB | RFM95_WRITE_BIT;
    buf[1] = (bytes >> 8) & 0xFF;
    buf[2] = bytes & 0xFF;

    chipSelect();
    spi_write_blocking(_bus, buf, 3);
    chipDeselect();
}

bool RFM95::setFrequency(float center)
{
    uint8_t buf[4];
    uint8_t buf2[4];
    uint8_t msb, csb, lsb;

    if(center >= 902.0 && center <= 928.0)
    {
        uint32_t frf = (center * 1000000.0) / RFM95_FSTEP;

        msb = (frf >> 16) & 0xFF;
        csb = (frf >> 8) & 0xFF;
        lsb = frf & 0xFF;

        //printf("FREQUENCY: 0x%x  MSB: 0x%x CSB: 0x%x LSB: 0x%x\n", frf, msb, csb, lsb);

        //writeRegister(RFM95_REG_06_FRF_MSB, (frf >> 16) & 0xFF);
        //writeRegister(RFM95_REG_07_FRF_MID, (frf >> 8) & 0xFF);
        //writeRegister(RFM95_REG_08_FRF_LSB, frf & 0xFF);
        
        buf[0] = RFM95_REG_06_FRF_MSB | RFM95_WRITE_BIT;
        buf[1] = msb;
        buf[2] = csb; 
        buf[3] = lsb;

        chipSelect();
        spi_write_blocking(_bus, buf, 4);
        chipDeselect();

        sleep_ms(10);

        

        /*readRegister(RFM95_REG_06_FRF_MSB, _buf);
        msb = _buf[0];

        readRegister(RFM95_REG_07_FRF_MID, _buf);
        csb = _buf[0];

        readRegister(RFM95_REG_08_FRF_LSB, _buf);
        lsb = _buf[0];*/

        buf2[0] = RFM95_REG_06_FRF_MSB;

        chipSelect();
        spi_write_blocking(_bus, buf2, 1);
        spi_read_blocking(_bus, 0, buf2+1, 3);
        chipDeselect();


        if(buf2[1] != msb|| buf2[2] != csb || buf2[3] != lsb)
        {
            printf("RFM95 ERROR FREQUENCY MISMATCH! msb 0x%x csb 0x%x lsb 0x%x\n", buf2[1], buf2[2], buf2[3]);
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

void RFM95::setTxPower(uint8_t power)
{
    uint8_t buf[2];

    if (power > 20)
    {
	    power = 20;
    }
	if (power < 2)
    {
	    power = 2;
    }

	// For RFM95_PA_DAC_ENABLE, manual says '+20dBm on PA_BOOST when OutputPower=0xf'
	// RFM95_PA_DAC_ENABLE actually adds about 3dBm to all power levels. We will use it
	// for 8, 19 and 20dBm
	if (power > 17)
	{
	    //writeRegister(RFM95_REG_4D_PA_DAC, RFM95_PA_DAC_ENABLE);
        buf[0] = RFM95_REG_4D_PA_DAC | RFM95_WRITE_BIT;
        buf[1] = RFM95_PA_DAC_ENABLE;

        chipSelect();
        spi_write_blocking(_bus, buf, 2);
        chipDeselect();

	    power -= 3;
	}
	else
	{
	    //writeRegister(RFM95_REG_4D_PA_DAC, RFM95_PA_DAC_DISABLE);
        buf[0] = RFM95_REG_4D_PA_DAC | RFM95_WRITE_BIT;
        buf[1] = RFM95_PA_DAC_DISABLE;

        chipSelect();
        spi_write_blocking(_bus, buf, 2);
        chipDeselect();
	}

	// RFM95/96/97/98 does not have RFO pins connected to anything. Only PA_BOOST
	// pin is connected, so must use PA_BOOST
	// Pout = 2 + OutputPower (+3dBm if DAC enabled)
	//writeRegister(RFM95_REG_09_PA_CONFIG, RFM95_PA_SELECT | (power-2));

    buf[0] = RFM95_REG_09_PA_CONFIG | RFM95_WRITE_BIT;
    buf[1] = RFM95_PA_SELECT | (power-2);

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();
}

void RFM95::setSignalBandwidth(long sbw)
{
    uint8_t buf[2];
    uint8_t bw; //register bit pattern
 
    if (sbw <= 7800)
    {
	    bw = RFM95_BW_7_8KHZ;
    }
    else if (sbw <= 10400)
    {
	    bw =  RFM95_BW_10_4KHZ;
    }
    else if (sbw <= 15600)
    {
	    bw = RFM95_BW_15_6KHZ;
    }
    else if (sbw <= 20800)
    {
	    bw = RFM95_BW_20_8KHZ;
    }
    else if (sbw <= 31250)
    {
	    bw = RFM95_BW_31_25KHZ;
    }
    else if (sbw <= 41700)
    {
	    bw = RFM95_BW_41_7KHZ;
    }
    else if (sbw <= 62500)
    {
	    bw = RFM95_BW_62_5KHZ;
    }
    else if (sbw <= 125000)
    {
	    bw = RFM95_BW_125KHZ;
    }
    else if (sbw <= 250000)
    {
	    bw = RFM95_BW_250KHZ;
    }
    else 
    {
	    bw =  RFM95_BW_500KHZ;
    }
     
    // top 4 bits of reg 1D control bandwidth
    //readRegister(RFM95_REG_1D_MODEM_CONFIG1, _buf);
    //writeRegister(RFM95_REG_1D_MODEM_CONFIG1, (_buf[0] & ~RFM95_BW) | bw);

    buf[0] = RFM95_REG_1D_MODEM_CONFIG1 & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0x44, buf+1, 1);
    chipDeselect();

    buf[0] = RFM95_REG_1D_MODEM_CONFIG1 | RFM95_WRITE_BIT;
    buf[1] = (buf[1] & ~RFM95_BW) | bw;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    setLowDatarate();
}

void RFM95::setCodingRate(uint8_t denominator)
{
    uint8_t buf[2];

    int cr = RFM95_CODING_RATE_4_5;
 
    if (denominator == 6)
    {
	    cr = RFM95_CODING_RATE_4_6;
    }
    else if (denominator == 7)
    {
	    cr = RFM95_CODING_RATE_4_7;
    }
    else if (denominator >= 8)
    {
	    cr = RFM95_CODING_RATE_4_8;
    }
 
    // CR is bits 3..1 of RFM95_REG_1D_MODEM_CONFIG1
    //readRegister(RFM95_REG_1D_MODEM_CONFIG1, _buf);
    //writeRegister(RFM95_REG_1D_MODEM_CONFIG1, (_buf[0] & ~RFM95_CODING_RATE) | cr);

    buf[0] = RFM95_REG_1D_MODEM_CONFIG1 & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0x33, buf+1, 1);
    chipDeselect();

    buf[0] = RFM95_REG_1D_MODEM_CONFIG1 | RFM95_WRITE_BIT;
    buf[1] = (buf[1] & ~RFM95_CODING_RATE) | cr;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();
}

void RFM95::setSpreadingFactor(uint8_t sf)
{
    uint8_t buf[2];

    if (sf <= 6) 
    {
        sf = RFM95_SPREADING_FACTOR_64CPS;
    }
    else if (sf == 7) 
    {
        sf = RFM95_SPREADING_FACTOR_128CPS;
    }
    else if (sf == 8) 
    {
        sf = RFM95_SPREADING_FACTOR_256CPS;
    }
    else if (sf == 9)
    {
        sf = RFM95_SPREADING_FACTOR_512CPS;
    }
    else if (sf == 10)
    {
        sf = RFM95_SPREADING_FACTOR_1024CPS;
    }
    else if (sf == 11) 
    {
        sf = RFM95_SPREADING_FACTOR_2048CPS;
    }
    else if (sf >= 12)
    {
        sf =  RFM95_SPREADING_FACTOR_4096CPS;
    }
 
   // set the new spreading factor
   //readRegister(RFM95_REG_1E_MODEM_CONFIG2, _buf);
   //writeRegister(RFM95_REG_1E_MODEM_CONFIG2, (_buf[0] & ~RFM95_SPREADING_FACTOR) | sf);
   buf[0] = RFM95_REG_1E_MODEM_CONFIG2 & ~RFM95_WRITE_BIT;

   chipSelect();
   spi_write_blocking(_bus, buf, 1);
   spi_read_blocking(_bus, 0x22, buf+1, 1);
   chipDeselect();

   buf[0] = RFM95_REG_1E_MODEM_CONFIG2 | RFM95_WRITE_BIT;
   buf[1] = (buf[1] & ~RFM95_SPREADING_FACTOR) | sf;

   chipSelect();
   spi_write_blocking(_bus, buf, 2);
   chipDeselect();

   // check if Low data Rate bit should be set or cleared
   setLowDatarate();
}

void RFM95::setModeIdle()
{
    uint8_t buf[2];

    buf[0] = RFM95_REG_01_OP_MODE | RFM95_WRITE_BIT;
    buf[1] = RFM95_MODE_STDBY;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();    
    
    _mode = RFMModeIdle;
}

void RFM95::setModeRX()
{
    uint8_t buf[2];

    buf[0] = RFM95_REG_01_OP_MODE | RFM95_WRITE_BIT;
    buf[1] = RFM95_MODE_RXCONTINUOUS;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    buf[0] = RFM95_REG_40_DIO_MAPPING1 | RFM95_WRITE_BIT;
    buf[1] = 0x00;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    _mode = RFMModeRx;
}

void RFM95::setModeTX()
{
    uint8_t buf[2];
    
    buf[0] = RFM95_REG_01_OP_MODE | RFM95_WRITE_BIT;
    buf[1] = RFM95_MODE_TX;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    buf[0] = RFM95_REG_40_DIO_MAPPING1 | RFM95_WRITE_BIT;
    buf[1] = 0x40;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    _mode = RFMModeTx;
}

void RFM95::setModeSleep()
{
    uint8_t buf[2];

    buf[0] = RFM95_REG_01_OP_MODE | RFM95_WRITE_BIT;
    buf[1] = RFM95_MODE_SLEEP | RFM95_LONG_RANGE_MODE;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();

    nop_sleep_ms(10);
    //readRegister(RFM95_REG_01_OP_MODE, _buf);
    buf[0] = RFM95_REG_01_OP_MODE & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0xDD, buf+1, 1);
    chipDeselect();

    if(buf[1] != (RFM95_MODE_SLEEP | RFM95_LONG_RANGE_MODE))
    {
        printf("RFM95 Error! 0x01 = 0x%x\n", buf[1]);
    }

    _mode = RFMModeSleep;
}

int RFM95::lastSNR()
{
    return _lastRSSI;
}

uint8_t RFM95::getDeviceVersion()
{
    uint8_t rtn[] = {RFM95_REG_42_VERSION & ~RFM95_WRITE_BIT};

    chipSelect();
    spi_write_blocking(_bus, rtn, 1);
    spi_read_blocking(_bus, 0x11, rtn, 1);
    chipDeselect();

    return rtn[0];
}

void RFM95::setPromiscuous(bool onOff)
{
    _promiscuous = onOff;
}

void RFM95::setCRC(bool onOff)
{
    uint8_t buf[2];

    buf[0] = RFM95_REG_1E_MODEM_CONFIG2 & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0, buf+1, 1);
    chipDeselect();

    if(onOff)
    {
        buf[1] |= RFM95_PAYLOAD_CRC_ON;
    }
    else
    {
        buf[1] &= ~RFM95_PAYLOAD_CRC_ON;
    }

    buf[0] = RFM95_REG_1E_MODEM_CONFIG2 | RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 2);
    chipDeselect();
}

/*void RFM95::writeRegister(uint8_t reg, uint8_t data)
{
    _buf[0] = reg | 0x80;  //Add Write bit
    _buf[1] = data;

    printf("Writing Reg 0x%x (0x%x): 0x%x\n", reg, _buf[0], _buf[1]);

    chipSelect();
    spi_write_blocking(spi_default, _buf, 2);
    chipDeselect();

    sleep_ms(10);
}*/

void RFM95::readRegister(uint8_t reg, uint8_t *buf)
{
    reg &= 0x7f; // remove read bit as this is a write

    uint8_t bufIn[2];
    uint8_t bufOut[2];
    //printf("Reading Reg 0x%x ", reg);

    bufIn[0] = reg;
    bufIn[1] = 0x66;

    chipSelect();
    spi_write_blocking(_bus, bufIn, 1);
    spi_read_blocking(_bus, 0x66, bufOut+1, 1);
    chipDeselect();

    buf[0] = bufOut[1];

    //printf("0x%x\n", buf[0]);
}


void RFM95::chipSelect()
{
    nop_sleep_us(1);
    gpio_put(_cs, 0);  // Active low
    nop_sleep_us(1);
}

void RFM95::chipDeselect()
{
    nop_sleep_us(1);
    gpio_put(_cs, 1);  // Active low
    nop_sleep_us(1);
}

void RFM95::setLowDatarate()
{
    uint8_t buf[2];

    // read current value for BW and SF
    //readRegister(RFM95_REG_1D_MODEM_CONFIG1, _buf);
    buf[0] = RFM95_REG_1D_MODEM_CONFIG1 & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0xEE, buf+1, 1);
    chipDeselect();

    uint8_t BW = buf[1] >> 4;	// bw is in bits 7..4

    //readRegister(RFM95_REG_1E_MODEM_CONFIG2, _buf);
    buf[0] = RFM95_REG_1E_MODEM_CONFIG2 & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0x01, buf+1, 1);
    chipDeselect();

    uint8_t SF = buf[1] >> 4;	// sf is in bits 7..4
   
    // calculate symbol time (see Semtech AN1200.22 section 4)
    float bw_tab[] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};
   
    float bandwidth = bw_tab[BW];
   
    float symbolTime = 1000.0 * (2 ^ SF) / bandwidth;	// ms
   
    // the symbolTime for SF 11 BW 125 is 16.384ms. 
    // and, according to this :- 
    // https://www.thethingsnetwork.org/forum/t/a-point-to-note-lora-low-data-rate-optimisation-flag/12007
    // the LDR bit should be set if the Symbol Time is > 16ms
    // So the threshold used here is 16.0ms
 
    // the LDR is bit 3 of RFM95_REG_26_MODEM_CONFIG3
    //readRegister(RFM95_REG_26_MODEM_CONFIG3, _buf);
    buf[0] = RFM95_REG_26_MODEM_CONFIG3 & ~RFM95_WRITE_BIT;

    chipSelect();
    spi_write_blocking(_bus, buf, 1);
    spi_read_blocking(_bus, 0x02, buf+1, 1);
    chipDeselect();

    uint8_t current = buf[1] & ~RFM95_LOW_DATA_RATE_OPTIMIZE; // mask off the LDR bit
    if (symbolTime > 16.0)
    {
	    //writeRegister(RFM95_REG_26_MODEM_CONFIG3, current | RFM95_LOW_DATA_RATE_OPTIMIZE);
        buf[0] = RFM95_REG_26_MODEM_CONFIG3 | RFM95_WRITE_BIT;
        buf[1] = current | RFM95_LOW_DATA_RATE_OPTIMIZE;

        chipSelect();
        spi_write_blocking(_bus, buf, 2);
        chipDeselect();
    }
    else
    {
	    //writeRegister(RFM95_REG_26_MODEM_CONFIG3, current);
        buf[0] = RFM95_REG_26_MODEM_CONFIG3 | RFM95_WRITE_BIT;
        buf[1] = current;

        chipSelect();
        spi_write_blocking(_bus, buf, 2);
        chipDeselect();
    }
}

void RFM95::setLEDS(uint8_t rx, uint8_t tx)
{
    RXled = rx;
    TXled = tx;

    gpio_init(RXled);
    gpio_set_dir(RXled, GPIO_OUT);
    gpio_put(RXled, 1);

    gpio_init(TXled);
    gpio_set_dir(TXled, GPIO_OUT);
    gpio_put(TXled, 1);
}

void RFM95::setTX()
{
    if(TXled != 0xFF)
    {
        gpio_put(TXled, 0);
    }
}

void RFM95::setRX()
{
    if(RXled != 0xFF)
    {
        gpio_put(RXled, 0);
    }
}

void RFM95::clearTX()
{
    if(TXled != 0xFF)
    {
        gpio_put(TXled, 1);
    }
}

void RFM95::clearRX()
{
    if(RXled != 0xFF)
    {
        gpio_put(RXled, 1);
    }
}
