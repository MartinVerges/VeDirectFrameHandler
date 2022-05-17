# VeDirectFrameHandler

This is a Library to read from Victron devices using VE.Direct protocol.

This library is based on a publically released reference implementation by Victron.
Victron only released the framehandler.cpp portion, so some assumptions were made to adapt it.

The VE.Direct Protocol FAQ is located here: https://www.victronenergy.com/live/vedirect_protocol:faq

The library was tested using a Raspberry PI and a ESP32.

The application passes serial bytes to the library.
The library parses those bytes, verifies the frame, and makes a public buffer available to the application to read as a name/value pair.

It's able to parse TEXT and HEX protocol messages.
