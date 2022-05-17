/* framehandler.cpp
 *
 * Arduino library to read from Victron devices using VE.Direct protocol.
 * Derived from Victron framehandler reference implementation.
 * 
 * The MIT License
 * 
 * Copyright (c) 2019 Victron Energy BV
 * Portions Copyright (C) 2020 Chris Terwilliger
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
 * 2022.05.16 - 0.4 - drop arduino requirements, by Martin Verges
 */
 
#include <ctype.h>
#include <cstdint>
#include <stdio.h>
#include <string.h>

#include "VeDirectFrameHandler.h"

// The name of the record that contains the checksum. 
// It's upper case as we upper all chars in the progress.
static constexpr char checksumTagName[] = "CHECKSUM";

/**
 * @brief Construct a new Ve Direct Frame Handler:: Ve Direct Frame Handler object
 */
VeDirectFrameHandler::VeDirectFrameHandler() {}

/**
 * @brief Reads the next byte given and parses it into the frame
 * @details This function is called by the application which passes a byte of serial data
 * @param inbyte 
 */
void VeDirectFrameHandler::rxData(uint8_t inbyte) {
	if ( inbyte == ':' && mState != CHECKSUM ) {
		mState = RECORD_HEX;
	}
	if (mState != RECORD_HEX && mState != IDLE) {
		mChecksum += inbyte;
	}
	inbyte = toupper(inbyte);

	switch(mState) {
		case IDLE:
			// Wait for \n of the start of an record
			// The next received byte will be part of the frame
			switch(inbyte) {
				case '\n':
					mState = RECORD_BEGIN;
					mChecksum = 0;
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
				case '\t':	// End of field name (sperator)
					// the Checksum record indicates a EOR
					if ( mTextPointer < (mName + sizeof(mName)) ) {
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
					if ( mTextPointer < (mName + sizeof(mName)-1) )
						*mTextPointer++ = inbyte;
					break;
			}
			break;
		case RECORD_VALUE:
			// The record value is being received. The \r\n indicates a new record.
			switch(inbyte) {
				case '\n':
					// forward record, only if it could be stored completely
					if ( mTextPointer < (mValue + sizeof(mValue)) ) {
						*mTextPointer = 0; // make zero ended
						textRxEvent(mName, mValue);
					}
					mState = RECORD_BEGIN;
					break;
				case '\r': // Ignore
					break;
				default:
					// add byte to value, but do no overflow
					if ( mTextPointer < (mValue + sizeof(mValue)-1) )
						*mTextPointer++ = inbyte;
					break;
			}
			break;
		case CHECKSUM:
			switch(inbyte) {
				case '\n':
					if (mChecksum)
						printf("[CHECKSUM] Invalid frame\n");
					mState = IDLE;
					frameEndEvent(mChecksum == 0);
					break;
				default:	// ignore, but use the char for the checksum
					break;
			}
			break;
		case RECORD_HEX:
			if (hexRxEvent(inbyte)) {
				mChecksum = 0;
				mState = IDLE;
			}
			break;
	} // End of switch(mState)
}

/*
 * textRxEvent
 * This function is called every time a new name/value is successfully parsed.  It writes the values to the temporary buffer.
 */
void VeDirectFrameHandler::textRxEvent(char * mName, char * mValue) {
	if (frameIndex < frameLen) {				// prevent overflow
      strcpy(tempName[frameIndex], mName);		// copy name to temporary buffer
      strcpy(tempValue[frameIndex], mValue);	// copy value to temporary buffer
      frameIndex++;
    }
}

/*
 *	frameEndEvent
 *  This function is called at the end of the received frame.  If the checksum is valid, the temp buffer is read line by line.
 *  If the name exists in the public buffer, the new value is copied to the public buffer.	If not, a new name/value entry
 *  is created in the public buffer.
 */
void VeDirectFrameHandler::frameEndEvent(bool valid) {
	if (valid) {
		for (int i = 0; i < frameIndex; i++) {				// read each name already in the temp buffer
			bool nameExists = false;
			for ( int j = 0; j <= veEnd; j++ ) {				// compare to existing names in the public buffer
				if ( strcmp(tempName[i], veName[j]) == 0 ) {	
					strcpy(veValue[j], tempValue[i]);			// overwrite tempValue in the public buffer
					nameExists = true;
					break;
				}
			}
			if (!nameExists) {
				strcpy(veName[veEnd], tempName[i]);				// write new Name to public buffer
				strcpy(veValue[veEnd], tempValue[i]);			// write new Value to public buffer
				veEnd++;										// increment end of public buffer
				if ( veEnd >= buffLen ) {						// stop any buffer overrun
					veEnd = buffLen - 1;
				}
			}
		}
	}
	frameIndex = 0;	// reset frame
}

/*
 *	hexRxEvent
 *  This function included for continuity and possible future use.	
 */
bool VeDirectFrameHandler::hexRxEvent(uint8_t inbyte) {
	(void)inbyte;
	return true;		// stubbed out for future
}
