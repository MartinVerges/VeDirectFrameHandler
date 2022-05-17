/* frameHandler.h
 *
 * Arduino library to read from Victron devices using VE.Direct protocol.
 * Derived from Victron framehandler reference implementation.
 * 
 * 2020.05.05 - 0.2 - initial release
 * 2021.02.23 - 0.3 - change frameLen to 22 per VE.Direct Protocol version 3.30
 * 2022.05.16 - 0.4 - drop arduino requirements, by Martin Verges
 * 2022.05.17 - 0.5 - add support for the HEX protocol
 */

#ifndef FRAMEHANDLER_H_
#define FRAMEHANDLER_H_

const uint8_t frameLen = 22;        // VE.Direct Protocol: max frame size is 18
const uint8_t nameLen = 9;          // VE.Direct Protocol: max name size is 9 including /0
const uint8_t valueLen = 33;        // VE.Direct Protocol: max value size is 33 including /0
const uint8_t buffLen = 40;         // Maximum number of lines possible from the device. Current protocol shows this to be the BMV700 at 33 lines.
const uint8_t hexBuffLen = 100;	    // Maximum size of hex frame - max payload 34 byte (=68 char) + safe buffer

typedef void (*hexCallback)(const char*, int, void*);

struct VeHexCB {
  hexCallback cb;
  void* data;
};

typedef void (*logFunction)(const char *, const char *);

class VeDirectFrameHandler {
    public:
        VeDirectFrameHandler();
        virtual ~VeDirectFrameHandler();
        void rxData(uint8_t inbyte);                // byte of serial data to be passed by the application
        void addHexCallback(hexCallback, void*);	// add function called back when hex frame is ready (sync or async)

        char veName[buffLen][nameLen] = { };        // public buffer for received names
        char veValue[buffLen][valueLen] = { };      // public buffer for received values
        char veHexBuffer[hexBuffLen] = { };		    // public buffer for received hex frames

        int frameIndex = 0;                         // which line of the frame are we on
        int veEnd = 0;                              // current size (end) of the public buffer
        int veHEnd;				                	// size of hex buffer

    private:
        enum States {                               // state machine
            IDLE,
            RECORD_BEGIN,
            RECORD_NAME,
            RECORD_VALUE,
            CHECKSUM,
            RECORD_HEX
        };

        int mState = States::IDLE;                  // current state

        uint8_t mChecksum = 0;                      // checksum value

        char * mTextPointer;                        // pointer to the private buffer we're writing to, name or value

        char tempName[frameLen][nameLen];           // private buffer for received names
        char tempValue[frameLen][valueLen];         // private buffer for received values

        char mName[9];                              // buffer for the field name
        char mValue[33];                            // buffer for the field value

        void textRxEvent(char *, char *);
        void frameEndEvent(bool);
        
        int hexRxEvent(uint8_t);

        VeHexCB* veHexCBList;
        int veCBEnd;
        int maxCB;
        int vePushedState;
};

#endif // FRAMEHANDLER_H_
