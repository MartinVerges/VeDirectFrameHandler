/* framehandler.cpp
 *
 * Library to read from Victron devices using VE.Direct protocol.
 * Derived from Victron framehandler reference implementation.
 *
 * The MIT License
 *
 * Copyright (c) 2019 Victron Energy BV
 * Portions Copyright (C) 2020 Chris Terwilliger
 * Portions Copyright (C) 2022 Martin Verges
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * 2020.05.05 - 0.2 - initial release
 * 2020.06.21 - 0.2 - add MIT license, no code changes
 * 2020.08.20 - 0.3 - corrected #include reference
 * 2022.05.16 - 0.4 - drop arduino requirements, add cmake, add pedantic, chksum without \t\n\s
 * 2022.05.17 - 0.5 - add support for the HEX protocol
 */

#include <ctype.h>
#include <cstdint>
#include <stdio.h>
#include <string.h>

#include "VeDirectFrameHandler.h"

#ifndef DEBUG_MODE
#define DEBUG_MODE false
#endif

// initial number - buffer is dynamically increased if necessary
#define MAX_HEX_CALLBACK 10

// Helper functions for the HEX protocol
#define ascii2hex(v) (v-48-(v>='A'?7:0))
#define hex2byte(b) (ascii2hex(*(b)))*16+((ascii2hex(*(b+1))))

// The name of the record that contains the checksum.
// It's upper case as we upper all chars in the progress.
static constexpr char checksumTagName[] = "CHECKSUM";

/**
 * @brief Construct a new Ve Direct Frame Handler:: Ve Direct Frame Handler object
 */
VeDirectFrameHandler::VeDirectFrameHandler() {}

/**
 * @brief Destroy the Ve Direct Frame Handler:: Ve Direct Frame Handler object
 */
VeDirectFrameHandler::~VeDirectFrameHandler() {
      if (veHexCBList) delete veHexCBList;
}

/**
 * @brief Return true if new data can be read
 * 
 * @return true on new data
 * @return false when no data available
 */
bool VeDirectFrameHandler::isDataAvailable() {
  return newDataAvailable;
}

/**
 * @brief Clear state and wait for new data
 */
void VeDirectFrameHandler::clearData() {
  newDataAvailable = false;
}

/**
 * @brief Reads the next byte given and parses it into the frame
 * @details This function is called by the application which passes a byte of serial data
 *
 * @param inbyte Input byte to store in the tmp memory
 */
void VeDirectFrameHandler::rxData(uint8_t inbyte) {
  if ( inbyte == ':' && mState != CHECKSUM ) {
    vePushedState = mState; // hex frame can interrupt TEXT
    mState = RECORD_HEX;
    veHEnd = 0;
  }
  if (mState != RECORD_HEX) {
    if(DEBUG_MODE) printf("Checksum = %3d ", mChecksum);
    mChecksum += inbyte;
    //mChecksum = (mChecksum + inbyte) & 255;
    if(DEBUG_MODE) {
      if (isprint(inbyte))      printf("+ %3d (%c)  ", inbyte, inbyte);
      else if (inbyte == '\n')  printf("+ %3d (\\n) ", inbyte);
      else if (inbyte == '\r')  printf("+ %3d (\\r) ", inbyte);
      else if (inbyte == '\t')  printf("+ %3d (\\t) ", inbyte);
      else printf("+ %3d (.)  ", inbyte);
      printf("= %3d  ", mChecksum);
      printf("mState = %d\n", mState);
    }
  }

  switch(mState) {
    case IDLE:
      // Wait for \n of the start of an record
      // The next received byte will be part of the frame
      switch(inbyte) {
        case '\n':
          if(DEBUG_MODE) printf("Transition from IDLE to RECORD_BEGIN\n");
          mState = RECORD_BEGIN;
          break;
        default: // skip \r and incomplete line data
          break;
      }
      break;
    case RECORD_BEGIN:
      // Start the record of the label name
      mTextPointer = mName;
      *mTextPointer++ = inbyte;
      mState = RECORD_NAME;
      break;
    case RECORD_NAME:
      // The record name is being received, terminated by a \t
      switch(inbyte) {
        case '\t': // End of field name (sperator)
          // the Checksum record indicates a EOR
          if (mTextPointer < (mName + sizeof(mName))) {
            *mTextPointer = 0; // Zero terminate
            if (strcmp(mName, checksumTagName) == 0) {
              mState = CHECKSUM;
              break;
            }
          }
          mTextPointer = mValue; // Reset value pointer to record the value
          mState = RECORD_VALUE;
          break;
        default:
          // add byte to name, but do no overflow
          if (mTextPointer < (mName + sizeof(mName)-1))
              *mTextPointer++ = toupper(inbyte);
          break;
      }
      break;
    case RECORD_VALUE:
      // The record value is being received. The \n indicates a new record.
      switch(inbyte) {
        case '\n':
          // forward record, only if it could be stored completely
          if (mTextPointer < (mValue + sizeof(mValue))) {
            *mTextPointer = 0; // make zero ended
            textRxEvent(mName, mValue);
          }
          mState = RECORD_BEGIN;
          break;
        case '\r': // Ignore
          break;
        default:
          // add byte to value, but do no overflow
          if (mTextPointer < (mValue + sizeof(mValue)-1))
            *mTextPointer++ = inbyte;
          break;
      }
      break;
    case CHECKSUM:
      if (mChecksum != 0) printf("[CHECKSUM] Invalid frame - checksum is %d\n", mChecksum);
      mState = IDLE;
      frameEndEvent(ignoreCheckSum || mChecksum == 0);
      mChecksum = 0;
      break;
    case RECORD_HEX:
      mState = hexRxEvent(inbyte);
      break;
  } // End of switch(mState)
}

/**
 * @brief This function is called every time a new name/value is successfully parsed.  It writes the values to the temporary buffer.
 *
 * @param mName     Name of the element
 * @param mValue    Value of the element
 */
void VeDirectFrameHandler::textRxEvent(char * mName, char * mValue) {
  if (frameIndex < frameLen) {                       // prevent overflow
    strcpy(tempData[frameIndex].veName, mName);      // copy name to temporary buffer
    strcpy(tempData[frameIndex].veValue, mValue);    // copy value to temporary buffer
    frameIndex++;
  }
}

/**
 * @brief This function is called at the end of the received frame.
 * @details If the checksum is valid, the temp buffer is read line by line.
 *          If the name exists in the public buffer, the new value is copied to the public buffer.
 *          If not, a new name/value entry is created in the public buffer.
 *
 * @param valid Set to true if the checksum was correct
 */
void VeDirectFrameHandler::frameEndEvent(bool valid) {
  if (valid) {
    newDataAvailable = true;
    // check if there is an existing entry
    for (int i = 0; i < frameIndex; i++) {                  // read each name already in the temp buffer
      bool nameExists = false;
      for (int j = 0; j <= veEnd; j++) {                    // compare to existing names in the public buffer
        if (strcmp(tempData[i].veName, veData[j].veName) == 0) {
          strcpy(veData[j].veValue, tempData[i].veValue);   // overwrite tempValue in the public buffer
          nameExists = true;
          break;
        }
      }
      if (!nameExists) {
        strcpy(veData[veEnd].veName, tempData[i].veName);   // write new Name to public buffer
        strcpy(veData[veEnd].veValue, tempData[i].veValue); // write new Value to public buffer
        veEnd++;                                            // increment end of public buffer
        if (veEnd >= buffLen) veEnd = buffLen - 1;          // stop any buffer overrun
      }
    }
  }
  frameIndex = 0;    // reset frame
}

/**
 * @brief Verify that the HEX value is vaild
 *
 * @param buffer
 * @param size
 * @return true
 * @return false
 */
static bool hexIsValid(const char* buffer, int size) {
  uint8_t checksum = 0x55 - ascii2hex(buffer[1]);
  for (int i=2; i<size; i+=2) checksum -= hex2byte(buffer+i);
  return (checksum == 0);
}

/**
 * @brief This function records hex answers or async messages
 *
 * @param inbyte
 * @return mState
 */
int VeDirectFrameHandler::hexRxEvent(uint8_t inbyte) {
  int ret = RECORD_HEX; // default - continue recording until end of frame
  switch (inbyte) {
    case '\n':
      // message ready - call all callbacks
      if (hexIsValid(veHexBuffer, veHEnd)) {
        for(int i=0; i<veCBEnd; i++) {
          (*(veHexCBList[i].cb))(veHexBuffer, veHEnd, veHexCBList[i].data);
        }
      } else printf("[CHECKSUM] Invalid hex frame \n");
      // restore previous state
      ret = vePushedState;
      break;
    default:
      veHexBuffer[veHEnd++] = inbyte;
      if (veHEnd >= hexBuffLen) { // oops -buffer overflow - something went wrong, we abort
      printf("hexRx buffer overflow - aborting read");
      veHEnd = 0;
      ret = IDLE;
    }
  }
  return ret;
}

/**
 * @brief This function record a new callback for hex frames
 *
 * @param cb
 * @param data
 */
void VeDirectFrameHandler::addHexCallback(hexCallback cb, void* data) {
  if (veHexCBList == 0) {     // first time, allocate callbacks buffer
    veHexCBList = new VeHexCB[maxCB];
    veCBEnd=0;
  }
  else if (veCBEnd == maxCB) { // we need to resize the callbacks buffer, we double the max size
    int newMax = maxCB*2;
    VeHexCB* tmpb = new VeHexCB[newMax];
    memcpy(tmpb, veHexCBList, maxCB*sizeof(VeHexCB));
    maxCB = newMax;
    delete veHexCBList;
    veHexCBList = tmpb;
  }
  veHexCBList[veCBEnd].cb = cb;
  veHexCBList[veCBEnd].data = data;
  veCBEnd++;
}
