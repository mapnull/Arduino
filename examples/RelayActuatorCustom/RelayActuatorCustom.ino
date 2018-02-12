/*
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2018 Sensnology AB
 * Full contributor list: https://github.com/mysensors/MySensors/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - Henrik Ekblad
 *
 * DESCRIPTION
 * Example sketch showing how to control physical relays.
 * This example will remember relay state after power failure.
 * http://www.mysensors.org/build/relay
 */

#include <SPI.h>
#include <Ethernet.h>

// Enable debug prints to serial monitor
#define MY_DEBUG

// For the custom transport
#define MY_SENSOR_NETWORK

// Enable repeater functionality for this node
#define MY_REPEATER_FEATURE

#include <MySensors.h>

#define RELAY_PIN 4  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define NUMBER_OF_RELAYS 1 // Total number of attached relays
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

EthernetClient client;

unsigned char nodeId;
MyMessage msg;
uint8_t data_available = 0;

void before()
{
	for (int sensor=1, pin=RELAY_PIN; sensor<=NUMBER_OF_RELAYS; sensor++, pin++) {
		// Then set relay pins in output mode
		pinMode(pin, OUTPUT);
		// Set relay to last known state (using eeprom storage)
		digitalWrite(pin, loadState(sensor)?RELAY_ON:RELAY_OFF);
	}
}

void setup()
{

}

void presentation()
{
	// Send the sketch version information to the gateway and Controller
	sendSketchInfo("Relay", "1.0");

	for (int sensor=1, pin=RELAY_PIN; sensor<=NUMBER_OF_RELAYS; sensor++, pin++) {
		// Register all sensors to gw (they will be created as child devices)
		present(sensor, S_BINARY);
	}
}


void loop()
{

}

void receive(const MyMessage &message)
{
	// We only expect one type of message from controller. But we better check anyway.
	if (message.type==V_STATUS) {
		// Change relay state
		digitalWrite(message.sensor-1+RELAY_PIN, message.getBool()?RELAY_ON:RELAY_OFF);
		// Store state in eeprom
		saveState(message.sensor, message.getBool());
		// Write some debug info
		Serial.print("Incoming change for sensor:");
		Serial.print(message.sensor);
		Serial.print(", New status: ");
		Serial.println(message.getBool());
	}
}

bool transportSend(const uint8_t to, const void* data, const uint8_t len, const bool noACK)
{
	if (client.connected()) {
		client.write((uint8_t *)data, len);
	}
}

bool transportInit(void)
{
	// TODO
}

void transportSetAddress(const uint8_t address)
{
	nodeId = address;
}

uint8_t transportGetAddress(void)
{
	return nodeId;
}

bool transportAvailable(void)
{
	uint8_t *ptr = (uint8_t *)&msg.last;
	data_available = 0;

	if (!client.connected()) {
		client.stop();
		// TODO: reconnect
	} else {
		// Check new packets
		while (client.available()) {
			*ptr++ = client.read();
			data_available++;
		}
	}

	return data_available;
}

uint8_t transportReceive(void* data)
{
	uint8_t bytes = data_available;

	memcpy(data, (void *)&msg.last, bytes);
	data_available = 0;

	return bytes;
}

bool transportSanityCheck(void)
{
	// not implemented yet
	return true;
}

void transportPowerDown(void)
{
	// Nothing to shut down here
}

void transportPowerUp(void)
{
	// Nothing to power up here
}

void transportSleep(void)
{
	// not implemented
}

void transportStandBy(void)
{
	// not implemented
}

int16_t transportGetSendingRSSI(void)
{
	// not implemented
	return INVALID_RSSI;
}

int16_t transportGetReceivingRSSI(void)
{
	// not implemented
	return INVALID_RSSI;
}

int16_t transportGetSendingSNR(void)
{
	// not implemented
	return INVALID_SNR;
}

int16_t transportGetReceivingSNR(void)
{
	// not implemented
	return INVALID_SNR;
}

int16_t transportGetTxPowerPercent(void)
{
	// not implemented
	return static_cast<int16_t>(100);
}

int16_t transportGetTxPowerLevel(void)
{
	// not implemented
	return static_cast<int16_t>(100);
}

bool transportSetTxPowerPercent(const uint8_t powerPercent)
{
	// not possbile
	(void)powerPercent;
	return false;
}
