/*
* The MySensors Arduino library handles the wireless radio link and protocol
* between your home built sensors/actuators and HA controller of choice.
* The sensors forms a self healing radio network with optional repeaters. Each
* repeater and gateway builds a routing tables in EEPROM which keeps track of the
* network topology allowing messages to be routed to nodes.
*
* Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
* Copyright (C) 2013-2015 Sensnology AB
* Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
*
* Documentation: http://www.mysensors.org
* Support Forum: http://forum.mysensors.org
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* Based on maniacbug's RF24 library, copyright (C) 2011 J. Coliz <maniacbug@ymail.com>
*/

static uint8_t MY_RF24_BASE_ADDR[MY_RF24_ADDR_WIDTH] = { MY_RF24_BASE_RADIO_ID };
static uint8_t MY_RF24_NODE_ADDRESS = AUTO;

// pipes
#define BROADCAST_PIPE 1
#define NODE_PIPE 2

// debug 
#if defined(MY_DEBUG_VERBOSE_RF24)
	#define RF24_DEBUG(x,...) debug(x, ##__VA_ARGS__)
#else
	#define RF24_DEBUG(x,...)
#endif

RF24 _rf24(MY_RF24_CE_PIN, MY_RF24_CS_PIN);

static void RF24_startListening(void) {
	RF24_DEBUG(PSTR("start listening\n"));

	_rf24.startListening();
}

static void RF24_powerDown(void) {
	_rf24.powerDown();
}

static bool RF24_sendMessage( uint8_t recipient, const void* buf, uint8_t len ) {
	// Make sure radio has powered up
	_rf24.powerUp();
	_rf24.stopListening();

	RF24_DEBUG(PSTR("send message to %d, len=%d\n"), recipient,len);

	MY_RF24_BASE_ADDR[0] = recipient;
	_rf24.openWritingPipe(MY_RF24_BASE_ADDR);
	bool ok = _rf24.write(buf, len, recipient == BROADCAST_ADDRESS);
	_rf24.startListening();

	return ok;
}

static bool RF24_isDataAvailable(uint8_t* to) {
	uint8_t pipe_num = 255;
	_rf24.available(&pipe_num);
	#if defined(MY_DEBUG_VERBOSE_RF24)
		if (pipe_num <= 5)
			RF24_DEBUG(PSTR("Data available on pipe %d\n"), pipe_num);	
	#endif	

	if (pipe_num == NODE_PIPE)
		*to = MY_RF24_NODE_ADDRESS;
	else if (pipe_num == BROADCAST_PIPE)
		*to = BROADCAST_ADDRESS;
	return (pipe_num <= 5);
}

static uint8_t RF24_readMessage( void* buf) {
	uint8_t len = _rf24.getDynamicPayloadSize();
	_rf24.read(buf, len);

	return len;
}

static void RF24_setNodeAddress(uint8_t address) {
	if (address != AUTO){
		MY_RF24_NODE_ADDRESS = address;
		// enable node pipe
		MY_RF24_BASE_ADDR[0] = MY_RF24_NODE_ADDRESS;
		_rf24.openReadingPipe(NODE_PIPE, MY_RF24_BASE_ADDR);
		// enable autoACK on node pipe
		_rf24.setAutoAck(NODE_PIPE, true);
	}
}

static uint8_t RF24_getNodeID(void) {
	return MY_RF24_NODE_ADDRESS;
}

static bool RF24_initialize(void) {
	// start up the radio library
	_rf24.begin();
	// determine whether the hardware is an nRF24L01+ or not
	if (!_rf24.isPVariant()) {
		RF24_DEBUG(PSTR("radio hardware not compatible"));
		return false;
	}
	// set CRC
	_rf24.setCRCLength(RF24_CRC_16);
	// set address width
	_rf24.setAddressWidth(MY_RF24_ADDR_WIDTH);
	// auto retransmit delay 1500us, auto retransmit count 15 
	_rf24.setRetries(5, 15);
	// channel
	_rf24.setChannel(MY_RF24_CHANNEL);
	// PA level
	_rf24.setPALevel(MY_RF24_PA_LEVEL);
	// data rate
	_rf24.setDataRate(MY_RF24_DATARATE);
	// sanity check
	#if defined(MY_RF24_SANITY_CHECK)
		if (_rf24.getPALevel() != MY_RF24_PA_LEVEL || _rf24.getDataRate() != MY_RF24_DATARATE) {
			RF24_DEBUG(PSTR("RF24 sanity check failed"));
			return false;
		}
	#endif
	// toggle features (necessary on some clones)
	_rf24.toggle_features();
	// enable Dynamic payload
	_rf24.enableDynamicPayloads();
	// enable ACK payload
	_rf24.enableAckPayload();
	// disable AA on all pipes, activate when node pipe set
	_rf24.setAutoAck(false);
	// all nodes listen to broadcast pipe (for FIND_PARENT_RESPONSE messages)
	MY_RF24_BASE_ADDR[0] = BROADCAST_ADDRESS;
	_rf24.openReadingPipe(BROADCAST_PIPE, MY_RF24_BASE_ADDR);

	//_rf24.printDetails();

	return true;
}
