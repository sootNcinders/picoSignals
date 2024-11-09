#include "radio.h"

#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "RFM95/RFM95.h"

#include "LED.h"
#include "ctc.h"
#include "heads.h"

SemaphoreHandle_t Radio::radioMutex; 
RFM95 Radio::radio = RFM95(spi0, PICO_DEFAULT_SPI_CSN_PIN, RADIOINT, 0);
bool Radio::sendError = false;
bool Radio::sleeping = false;
uint8_t Radio::addr = 0;
uint8_t Radio::primaryPartner = 0;
uint8_t Radio::radioFaults = 0;
float Radio::avgRSSI;

TaskHandle_t Radio::radioTaskHandle;

void gpio_isr(uint gpio, uint32_t event_mask)
{
    BaseType_t taskInterrupted = pdFALSE;

    if(Radio::radioTaskHandle != NULL)
    {
        if(gpio == RADIOINT)
        {
            Radio::radio.handleInterrupt();
            
            vTaskNotifyGiveFromISR(Radio::radioTaskHandle, &taskInterrupted);
        }
    }

    if(taskInterrupted == pdTRUE)
    {
        portYIELD_FROM_ISR(taskInterrupted);
    }

    gpio_acknowledge_irq(gpio, event_mask);
}

void Radio::init(void)
{
    addr = Main::cfg["address"];
    primaryPartner = Main::cfg["head1"]["destination"][0];

    radioMutex = xSemaphoreCreateMutex();

    spi_init(spi0, 5000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    
    // Make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI));

    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    //Initialize the RFM95 interrupt
    gpio_init(RADIOINT);
    gpio_set_dir(RADIOINT, GPIO_IN);

    gpio_set_irq_enabled_with_callback(RADIOINT, RFM95_INT_MODE, true, gpio_isr);

    initRadio();

    DPRINTF("\nNODE: %d\n", addr);

    xTaskCreate(radioTask, "Radio Task", 640, NULL, RADIOPRIORITY, &radioTaskHandle);

    DPRINTF("Radio Task Initialized\n");
}

void Radio::initRadio(void)
{
    DPRINTF("Initializing Radio\n");

    uint16_t timeout = (Main::cfg["retryTime"] | 150)/2;
    
    //Set up the RFM95 radio
    //12500bps datarate
    radio.init(NULL);
    radio.setLEDS(RXLED, TXLED);
    radio.setAddress(addr);
    //Preamble length: 8
    radio.setPreambleLength(8);
    //Center Frequency
    radio.setFrequency(915.0);
    //Set TX power to Max
    radio.setTxPower(20);
    //Set Bandwidth 500kHz
    radio.setSignalBandwidth(500000);
    //Set Coding Rate 4/5
    radio.setCodingRate(5);
    //Spreading Factor of 12 gives 1172bps - ~425ms round trip - Too Slow
    //Spreading Factor of 11 gives 2148bps - ~260ms round trip - Untested, Maybe too slow
    //Spreading Factor of 10 gives 3906bps - ~130ms round trip - Untested
    //Spreading Factor of 9 gives  7031bps - ~70ms round trip
    //Spreading Factor of 8 gives 12500bps - ~45ms round trip
    //Spreading Factor of 7 gives 21875bps - ~24ms round trip ****Insufficient Range****
    radio.setSpreadingFactor(9);
    //Accept all packets
    radio.setPromiscuous(true);

    radio.setCADTimeout(timeout);

    radio.setModeRX();
}

void Radio::radioTask(void *pvParameters)
{
    uint8_t buf[255];
    uint8_t size;
    uint8_t from;
    uint8_t to;
    uint8_t mode; 

    RCL msg;
    TOCTC toCtc;
    FROMCTC fromCtc;

    //gpio_set_irq_enabled_with_callback(RADIOINT, RFM95_INT_MODE, true, gpio_isr);

    //initRadio();

    while(true)
    {
        //DPRINTF("Radio Task\n");

        if(!sleeping)
        {
            while(radio.available())
            {
                memset(&buf, 0, sizeof(buf));
                to = 0;
                from = 0;
                size = sizeof(buf);
                radio.recv((uint8_t *)&buf, &size, &from, &to);

                if(size == sizeof(RCL) && (to == addr || to == 255))
                {
                    memcpy(&msg, buf, sizeof(RCL));
                    HEADS::processRxMsg(msg, from);
                }
                else if(size == sizeof(TOCTC) && (to == addr || to == 255))
                {
                    memcpy(&toCtc, buf, sizeof(TOCTC));
                    CTC::processToMsg(toCtc);
                }
                else if(size == sizeof(FROMCTC) && (to == addr || to == 255))
                {
                    memcpy(&fromCtc, buf, sizeof(FROMCTC));
                    if(fromCtc.dest == addr)
                    {
                        CTC::processFromMsg(fromCtc, from);
                    }
                }

                if(from == primaryPartner)
                {
                    if(avgRSSI == 0)
                    {
                        avgRSSI = radio.lastSNR();
                    }
                    else
                    {
                        avgRSSI = (radio.lastSNR() * (float)ALPHA) + (avgRSSI * (1-(float)ALPHA));
                    }
                }
            }

            mode = radio.getMode()&0x7; //Only Bits 0-2 represent mode
            if(mode != RFM95_MODE_RXCONTINUOUS && mode != RFM95_MODE_RXSINGLE && mode != RFM95_MODE_TX && mode != RFM95_MODE_CAD)
            {
                radioFaults++;
                //DPRINTF("Radio fault %d Mode: %d\n", radioFaults, mode);
            }
            else
            {
                radioFaults = 0;
            }

            if(radioFaults > 10)
            {
                DPRINTF("Radio fault Mode: %d\n", mode);
                radio.setModeSleep();
                initRadio();
                DPRINTF("Radio reset\n");
            }
        }

        xTaskNotifyWait(0, ULONG_MAX, NULL, 1000/portTICK_PERIOD_MS);
    }
}

//transmit a certain packet out
//Transmit time: 2ms
//Response time: 47mS
void Radio::transmit(uint8_t dest, char asp, bool ack, bool code)
{
    xSemaphoreTake(radioMutex, portMAX_DELAY);
    RCL transmission;
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    vTaskPrioritySet(NULL, MAXPRIORITY);

    DPRINTF("Sending to %d: aspect: %c, ack: %d, code: %d\n", dest, asp, ack, code);

    //Pack the data packet
    transmission.destination = dest;
    transmission.aspect = asp;
    transmission.isACK = ack;
    transmission.isCode = code;

    //If the transmission fails for any reason, set the error light
    if(!radio.send(255, (uint8_t*) &transmission, sizeof(transmission)))
    {
        LED::setError(TRANSMISSIONFAIL);
        sendError = true;
    }
    //If the radio recovers, clear the error light
    else if(sendError)
    {
        //if the transciever driver recovers, clear the error
        LED::setError(NOERROR);
        sendError = false;
    }

    vTaskPrioritySet(NULL, priority);

    xSemaphoreGive(radioMutex);
}

/**
 * Sends data to the CTC (Central Traffic Control) system.
 *
 * @param data The data to be sent to the CTC system.
 *
 * @throws None
 */
void Radio::sendToCTC(TOCTC data)
{
    xSemaphoreTake(radioMutex, portMAX_DELAY);

    UBaseType_t priority = uxTaskPriorityGet(NULL);

    vTaskPrioritySet(NULL, MAXPRIORITY);

    DPRINTF("Sending To CTC\n");

    //If the transmission fails for any reason, set the error light
    if(!radio.send(255, (uint8_t*) &data, sizeof(data)))
    {
        //if the send failed set the error light
        gpio_put(GOODLED, HIGH);
        LED::setError(TRANSMISSIONFAIL);
        sendError = true;
    }
    //If the radio recovers, clear the error light
    else if(sendError)
    {
        //if the transciever driver recovers, clear the error
        LED::setError(NOERROR);
        sendError = false;
    }

    vTaskPrioritySet(NULL, priority);

    xSemaphoreGive(radioMutex);
}

/**
 * Sends data from the CTC (Central Traffic Control) system to a specified destination.
 *
 * @param data The data to be sent, containing the destination and command.
 *
 * @throws None
 */
void Radio::sendFromCTC(FROMCTC data)
{
    xSemaphoreTake(radioMutex, portMAX_DELAY);

    UBaseType_t priority = uxTaskPriorityGet(NULL);

    vTaskPrioritySet(NULL, MAXPRIORITY);

    DPRINTF("Sending from CTC to %d cmd %d\n", data.dest, data.cmd);

    //If the transmission fails for any reason, set the error light
    if(!radio.send(255, (uint8_t*) &data, sizeof(data)))
    {
        //if the send failed set the error light
        gpio_put(GOODLED, HIGH);
        LED::setError(TRANSMISSIONFAIL);
        sendError = true;
    }
    //If the radio recovers, clear the error light
    else if(sendError)
    {
        //if the transciever driver recovers, clear the error
        LED::setError(NOERROR);
        sendError = false;
    }

    vTaskPrioritySet(NULL, priority);

    xSemaphoreGive(radioMutex);
}

int8_t Radio::getAvgRSSI(void)
{
    return (int8_t) round(avgRSSI);
}

uint8_t Radio::getAddr(void)
{
    return addr;
}

void Radio::sleep()
{
    radio.setModeSleep();
    sleeping = true;
}

void Radio::wake()
{
    radio.setModeRX();
    sleeping = false;
}
