/* frameHandler.h
 *
 * Library to read from Victron devices using VE.Direct protocol.
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

class VeDirectFrameHandler {
  public:
    VeDirectFrameHandler();
    virtual ~VeDirectFrameHandler();

    void rxData(uint8_t inbyte);
    int addHexCallback(hexCallback cbFunction, void* cbAdditionalData);
    bool isDataAvailable();
    void clearData();
    
    struct VeData {
      char veName[nameLen];
      char veValue[valueLen];
    };
    VeData veData[buffLen] = { };               // public buffer for received text frames
    bool newDataAvailable = false;              // will be set to true after receiving a frame 

    // VE HEX Protocol
    char veHexBuffer[hexBuffLen] = { };         // public buffer for received hex frames
    struct VeHexCB {
      hexCallback cbFunction;                   // function to call on each received hex frame
      void* cbAdditionalData;                   // optional additional data send to CB function
    };
    int frameIndex = 0;                         // which line of the frame are we on
    int veEnd = 0;                              // current size (end) of the public buffer
    int veHEnd = 0;                             // size of hex buffer

    bool ignoreCheckSum = false;                // Disable checksum verification

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

    VeData tempData[frameLen];                  // private buffer for received key+values

    char mName[9];                              // buffer for the field name
    char mValue[33];                            // buffer for the field value

    void textRxEvent(char *, char *);
    void frameEndEvent(bool);
    
    int hexRxEvent(uint8_t);

    VeHexCB* veHexCallBacks;                    // struct of registered callback functions
    int numRegisteredCbFunctions;               // number of currently registered callback functions
    int reservedSizeCB = 1;                     // reserved memory to store callbacks n*sizeof VeHexCB
    int veLastTextState;                        // After HEX data, the TEXT message can continue (just wtf..)
};

#endif // FRAMEHANDLER_H_
