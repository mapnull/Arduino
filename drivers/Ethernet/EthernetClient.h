/*
* The MySensors Arduino library handles the wireless radio link and protocol
* between your home built sensors/actuators and HA controller of choice.
* The sensors forms a self healing radio network with optional repeaters. Each
* repeater and gateway builds a routing tables in EEPROM which keeps track of the
* network topology allowing messages to be routed to nodes.
*
* Created by Marcelo Aquino <marceloaqno@gmail.org>
* Copyright (C) 2016 Marcelo Aquino
* Full contributor list: https://github.com/mysensors/MySensors/graphs/contributors
*
* Documentation: http://www.mysensors.org
* Support Forum: http://forum.mysensors.org
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* Based on Arduino ethernet library, Copyright (c) 2010 Arduino LLC. All right reserved.
*/

#ifndef ethernetclient_h
#define ethernetclient_h

#include "IPAddress.h"

// State codes from W5100 library
#define ETHERNETCLIENT_W5100_CLOSED 0x00
#define ETHERNETCLIENT_W5100_LISTEN 0x14
#define ETHERNETCLIENT_W5100_SYNSENT 0x15
#define ETHERNETCLIENT_W5100_SYNRECV 0x16
#define ETHERNETCLIENT_W5100_ESTABLISHED 0x17
#define ETHERNETCLIENT_W5100_FIN_WAIT 0x18
#define ETHERNETCLIENT_W5100_CLOSING 0x1A
#define ETHERNETCLIENT_W5100_TIME_WAIT 0x1B
#define ETHERNETCLIENT_W5100_CLOSE_WAIT 0x1C
#define ETHERNETCLIENT_W5100_LAST_ACK 0x1D

// debug 
#if defined(MY_DEBUG_VERBOSE_ETHERNET)
	#define ETHERNETCLIENT_DEBUG(x,...) debug(x, ##__VA_ARGS__)
#else
	#define ETHERNETCLIENT_DEBUG(x,...)
#endif

class EthernetClient {

public:
	EthernetClient();
	EthernetClient(int sock);

	uint8_t status();
	int connect(IPAddress ip, uint16_t port);
	int connect(const char *host, uint16_t port);
	size_t write(uint8_t);
	size_t write(const uint8_t *buf, size_t size);
	int available();
	int read();
	int read(uint8_t *buf, size_t size);
	int peek();
	void flush();
	void stop();
	uint8_t connected();
	operator bool();
	bool operator==(const bool value) { return bool() == value; }
	bool operator!=(const bool value) { return bool() != value; }
	bool operator==(const EthernetClient&);
	bool operator!=(const EthernetClient& rhs) { return !this->operator==(rhs); };

	friend class EthernetServer;

private:
	int _sock;
};

#endif
