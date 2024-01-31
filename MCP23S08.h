/**********************************************************************************************
 * 
 * This library for the Arduino IDE allows the simple usage of the MCP23S08 SPI based
 * I/O port expander by providing functions like digitalWrite, digitalRead and pinMode.
 * It is licensed under the MIT License, see LICENSE.txt.
 * 
 *********************************************************************************************/
 

#ifndef __MCP23S08_h__
#define __MCP23S08_h__

#include "Arduino.h"
#include <SPI.h>


// register addresses
#define MCP23S08_IODIR		0x00
#define MCP23S08_IPOL		0x01
#define MCP23S08_GPINTEN	0x02
#define MCP23S08_DEFVAL		0x03
#define MCP23S08_INTCON		0x04
#define MCP23S08_IOCON		0x05
#define MCP23S08_GPPU		0x06
#define MCP23S08_INTF		0x07
#define MCP23S08_INTCAP		0x08
#define MCP23S08_GPIO		0x09
#define MCP23S08_OLAT		0x0A


class MCP23S08 {
public:
	// constructors
	MCP23S08(uint8_t csPin);
	MCP23S08(uint8_t csPin, uint8_t deviceAddr);
	
	// call in setup
	void begin();
	
	// usage equivalent to the default IDE functions, see examples
	bool digitalReadIO(uint8_t pin);
	void digitalWriteIO(uint8_t pin, bool state);
	void pinModeIO(uint8_t pin, uint8_t mode);
	
	// advanced, write settings for all pins at once
	void setOutputStates(uint8_t states);
	void setPinModes(uint8_t modes);
	void enablePullups(uint8_t enables);
	
	// advanced, get settings for all pin at once
	uint8_t getInputStates();
	uint8_t getOutputStates();
	uint8_t getPinModes();
	uint8_t getEnabledPullups();
	
private:
	// private variables
	uint8_t csPin;
	uint8_t deviceOpcode = 0x40;
	
	// low level SPI communication
	void writeRegister(uint8_t address, uint8_t data);
	uint8_t readRegister(uint8_t address);
};

#endif	// __MCP23S08_h__
