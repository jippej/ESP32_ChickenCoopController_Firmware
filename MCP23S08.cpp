/**********************************************************************
 * 
 * This is the C++ part of the MCP23S08 library.
 * See MCP23S08.h and the example files for a full documentation.
 * 
 *********************************************************************/


#include "MCP23S08.h"


/*##################################### PUBLIC FUNCTIONS #####################################*/

MCP23S08::MCP23S08(uint8_t csPin) : csPin(csPin) {}


MCP23S08::MCP23S08(uint8_t csPin, uint8_t deviceAddr) : csPin(csPin) {
	deviceOpcode |= ((deviceAddr & 0x03) << 1);
}


void MCP23S08::begin() {
	SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
	pinMode(csPin, OUTPUT);
	digitalWrite(csPin, LOW);
	// reset all registers to default:
	SPI.transfer(MCP23S08_IODIR);	//set address pointer to first register
	SPI.transfer(0xFF);				// reset first register
	for (uint8_t i = 0; i < MCP23S08_OLAT; i++) {
		SPI.transfer(0x00);			// reset other 10 registers
	}
	digitalWrite(csPin, HIGH);
	SPI.endTransaction();
}


bool MCP23S08::digitalReadIO(uint8_t pin) {
	if (pin > 7) {
		return 0;
	}
	return (getInputStates() >> pin) & 1;
}


void MCP23S08::digitalWriteIO(uint8_t pin, bool state) {
	if (pin > 7) {
		return;
	}
	
	setOutputStates((getOutputStates() & ~(1 << pin)) | (state << pin));
}


void MCP23S08::pinModeIO(uint8_t pin, uint8_t mode) {
	if (pin > 7) {
		return;
	}
	
	switch (mode) {
		case INPUT:
			setPinModes(getPinModes() & ~(1 << pin));			// set pin to input
			enablePullups(getEnabledPullups() & ~(1 << pin));	// disable pullup for pin
			break;
		case OUTPUT:
			setPinModes(getPinModes() | (1 << pin));				// set pin to output
			enablePullups(getEnabledPullups() & ~(1 << pin));	// disable pullup for pin
			break;
		case INPUT_PULLUP:
			setPinModes(getPinModes() & ~(1 << pin));			// set pin to input
			enablePullups(getEnabledPullups() | (1 << pin));	// enable pullup for pin
			break;
	}
}


void MCP23S08::setOutputStates(uint8_t states) {
	writeRegister(MCP23S08_OLAT, states);
}


void MCP23S08::setPinModes(uint8_t modes) {
	writeRegister(MCP23S08_IODIR, ~(modes));	// inverted to match IDE defaults
}


void MCP23S08::enablePullups(uint8_t enables) {
	writeRegister(MCP23S08_GPPU, enables);
}


uint8_t MCP23S08::getInputStates() {
	return readRegister(MCP23S08_GPIO);
}


uint8_t MCP23S08::getOutputStates() {
	return readRegister(MCP23S08_OLAT);
}


uint8_t MCP23S08::getPinModes() {
	return ~(readRegister(MCP23S08_IODIR));		// inverted to match IDE defaults
}


uint8_t MCP23S08::getEnabledPullups() {
	return readRegister(MCP23S08_GPPU);
}


/*##################################### PRIVATE FUNCTIONS #####################################*/

void MCP23S08::writeRegister(uint8_t address, uint8_t data) {
	SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin, LOW);
	SPI.transfer(deviceOpcode);		// initialize transfer with opcode and R/W-flag cleared
	SPI.transfer(address);
	SPI.transfer(data);
	digitalWrite(csPin, HIGH);
	SPI.endTransaction();
}


uint8_t MCP23S08::readRegister(uint8_t address) {
	uint8_t data;
	SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin, LOW);
	SPI.transfer(deviceOpcode | 1);		// initialize transfer with opcode and R/W-flag set
	SPI.transfer(address);
	data = SPI.transfer(0);
	digitalWrite(csPin, HIGH);
	SPI.endTransaction();
	return data;
}
