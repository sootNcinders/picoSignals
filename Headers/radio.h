#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

#include "RFM95/RFM95.h"

#ifndef RADIO_H
#define RADIO_H

#define NUMSAMPLES (4)
#define ALPHA (2/(1+NUMSAMPLES))

//Packet Structure
//On Air Time - SF9: 31ms
struct RCL
{
  uint8_t destination; //Unsigned 8 bit integer destination address, 0-255
  //uint8_t voltage; //Unsinged 8 bit integer battery voltage, 0-255 representing 00.0 - 25.5V
  bool isACK;  //Is this packet an acknowledgement
  bool isCode; //Is this packet a code control packet
  char aspect; /*New aspect color for signal head. 
                * R - red 
                * A - amber 
                * G - green
                */
};

/* Serial input for map:
: - Start Chararcter
00 - Node Address, 00-FF
O - Head 1 - O, G, A, R, L
O - Head 2
O - Head 3
O - Head 4
0 - Captures, 0-F
0 - Releases, 0-F
0 - Turnouts, 0-F
00 - absolute value of average RSSI to primary partner, 0-FF
00 - Voltage x10, 0-FF */
//On Air Time - SF9: 41ms
struct TOCTC
{
    uint8_t sender;
    char head1;
    char head2;
    char head3;
    char head4;
    uint8_t captures;
    uint8_t releases;
    uint8_t turnouts;
    int8_t avgRSSI;
    uint8_t voltage;
    uint16_t ledError;
    uint8_t version = VERSION;
    uint8_t revision = REVISION;
};

struct FROMCTC
{
    uint8_t dest;
    uint8_t cmd;
};

class Radio
{
    public:
        static void init(void);

        //transmit a certain packet out
        //Transmit time: 2ms
        //Response time: 47mS
        static void transmit(uint8_t dest, char asp, bool ack, bool code);

        /**
         * Sends data to the CTC (Central Traffic Control) system.
         *
         * @param data The data to be sent to the CTC system.
         *
         * @throws None
         */
        static void sendToCTC(TOCTC data);

        /**
         * Sends data from the CTC (Central Traffic Control) system to a specified destination.
         *
         * @param data The data to be sent, containing the destination and command.
         *
         * @throws None
         */
        static void sendFromCTC(FROMCTC data);

        static int8_t getAvgRSSI(void);

        static uint8_t getAddr(void);

        static void sleep();

        static void wake();

        static TaskHandle_t radioTaskHandle;

        static RFM95 radio;

    private:
        static void radioTask(void *pvParameters);

        static void initRadio(void);

        static uint8_t addr;
        static uint8_t primaryPartner;
        static uint8_t radioFaults;

        static bool sendError;
        static bool sleeping;

        static float avgRSSI;

        static SemaphoreHandle_t radioMutex;
        static SemaphoreHandle_t RFM95mutex;
};

#endif
