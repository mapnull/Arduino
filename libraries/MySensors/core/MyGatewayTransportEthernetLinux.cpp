/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Marcelo Aquino <marceloaqno@gmail.org>
 * Copyleft (c) 2016, Marcelo Aquino
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include "MyGatewayTransport.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <list>

#ifdef MY_GATEWAY_MQTT_CLIENT
	#include <mosquitto.h>
	
	#ifndef MQTT_IP
		#define MQTT_IP "127.0.0.1"
	#endif
	#ifndef MQTT_PORT
		#define MQTT_PORT 1883
	#endif
	#ifndef MQTT_KEEPALIVE
		#define MQTT_KEEPALIVE 60
	#endif
	#ifndef MY_MQTT_PUBLISH_TOPIC_PREFIX
		#define MY_MQTT_PUBLISH_TOPIC_PREFIX "mygateway1-out"
	#endif
	#ifndef MY_MQTT_SUBSCRIBE_TOPIC_PREFIX
		#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "mygateway1-in"
	#endif
#endif

//TODO
#ifdef MY_USE_UDP
	#error UDP not supported for this type of gateway
#endif

union _addrIPv4{
	uint8_t bytes[4];
	uint32_t dword;
};

static int controllers[MY_GATEWAY_MAX_CLIENTS];
static MyMessage _ethernetMsg;
static std::list<struct MyMessage> ethernetMsg_q;
static pthread_mutex_t ethernetMsg_mutex = PTHREAD_MUTEX_INITIALIZER;

// Prototypes
int open_listen(char *ip, uint16_t port);
uint32_t getAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet);
void *get_in_addr(struct sockaddr *sa);
void *waiting_controllers(void* thread_arg);
void *connected_controller(void* thread_arg);

#ifdef MY_GATEWAY_MQTT_CLIENT
	void *mqtt_thread(void *);
	struct mosquitto *mosq;
#endif

bool gatewayTransportInit() {
	long int sockfd;
	pthread_t thread_id;
	pthread_attr_t detached_attr;
	char *ip = NULL;

	for (uint8_t i = 0; i < MY_GATEWAY_MAX_CLIENTS; i++) {
		controllers[i] = -1;
	}

#ifdef MY_CONTROLLER_IP_ADDRESS
	//TODO
	return false;
#else
	#ifdef MY_IP_ADDRESS
		char ipstr[INET_ADDRSTRLEN];
		union _addrIPv4 addrIPv4 = getAddress(MY_IP_ADDRESS);
		sprintf(ipstr, "%d.%d.%d.%d", addrIPv4.bytes[0], addrIPv4.bytes[1],
				addrIPv4.bytes[2],	addrIPv4.bytes[3]);
		ip = ipstr;
	#endif

	sockfd = open_listen(ip, MY_PORT);
	if (sockfd < 0) {
		return false;
	}
#endif

	pthread_attr_init(&detached_attr);
	pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread_id, &detached_attr, &waiting_controllers, (void *)sockfd);
	
#ifdef MY_GATEWAY_MQTT_CLIENT
	/* mqtt thread */
	pthread_create(&thread_id, &detached_attr, &mqtt_thread, NULL);
	
#endif

	pthread_attr_destroy(&detached_attr);

	return true;
}

bool gatewayTransportSend(MyMessage &message)
{
	char *ethernetMsg = protocolFormat(message);

	for (uint8_t i = 0; i < MY_GATEWAY_MAX_CLIENTS; i++) {
		if (controllers[i] == -1)
			continue;
		//TODO: sleep/retry before shutdown to check if traffic really is clogged?
		if (send(controllers[i], ethernetMsg, strlen(ethernetMsg), MSG_NOSIGNAL | MSG_DONTWAIT) == -1) {
			if (errno == EAGAIN) {
				// traffic is clogged
				shutdown(controllers[i], SHUT_RDWR);
				controllers[i] = -1;
			}
			perror("send");
		}
	}
	
#ifdef MY_GATEWAY_MQTT_CLIENT
	char *mqttMsg = protocolFormatMQTTTopic(MY_MQTT_PUBLISH_TOPIC_PREFIX, message);
	//connection, message id, topic, bytes, data, QOS, retain
	mosquitto_publish(mosq, NULL, mqttMsg, strlen(message.getString(_convBuffer)), message.getString(_convBuffer), 0, false);
#endif
	
	return true;
}

bool gatewayTransportAvailable()
{
	bool empty;

	pthread_mutex_lock(&ethernetMsg_mutex);
	empty = ethernetMsg_q.empty();
	pthread_mutex_unlock(&ethernetMsg_mutex);

	return !empty;
}

MyMessage& gatewayTransportReceive()
{
	// Return the last parsed message
	pthread_mutex_lock(&ethernetMsg_mutex);
	_ethernetMsg = ethernetMsg_q.front();
	ethernetMsg_q.pop_front();
	pthread_mutex_unlock(&ethernetMsg_mutex);

	return _ethernetMsg;
}

int open_listen(char *address, uint16_t port)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int yes=1;
	int rv;
	char ipstr[INET_ADDRSTRLEN];
	char portstr[6];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	sprintf(portstr, "%d", port);
	if ((rv = getaddrinfo(address, portstr, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			freeaddrinfo(servinfo);
			return -1;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "failed to bind\n");
		freeaddrinfo(servinfo);
		return -1;
	}

	if (listen(sockfd, MY_GATEWAY_MAX_CLIENTS) == -1) {
		perror("listen");
		freeaddrinfo(servinfo);
		return -1;
	}

	struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
	void *addr = &(ipv4->sin_addr);
	inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
	debug("Eth: Listening for connections on %s:%s\n", ipstr, portstr);

	freeaddrinfo(servinfo);
	return sockfd;
}

uint32_t getAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet)
{
	union _addrIPv4 addrIPv4;

	addrIPv4.bytes[0] = first_octet;
    addrIPv4.bytes[1] = second_octet;
    addrIPv4.bytes[2] = third_octet;
    addrIPv4.bytes[3] = fourth_octet;
	
	return addrIPv4.dword;
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *waiting_controllers(void* thread_arg)
{
	long int sockfd = (long int) thread_arg;
	long int new_fd; // new connection on new_fd
	struct sockaddr_storage client_addr; // connector's address information
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];
	pthread_t thread_id;
	pthread_attr_t attr;
	uint8_t i;

	while (1) {  // accept() loop
		sin_size = sizeof client_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(client_addr.ss_family,
				get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
		debug("Eth: New connection from %s\n", s);

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&thread_id, &attr, &connected_controller, (void *)new_fd);
		pthread_attr_destroy(&attr);

		for (i = 0; i < MY_GATEWAY_MAX_CLIENTS; i++) {
			if (controllers[i] == -1) {
				controllers[i] = new_fd;
				break;
			}
		}
		if (i >= MY_GATEWAY_MAX_CLIENTS) {
			// no free/disconnected spot so reject
			close(new_fd);
		}
	}

	return NULL;
}

void *connected_controller(void* thread_arg)
{
	long int sockfd = (long int) thread_arg;
	char inputString[MY_GATEWAY_MAX_RECEIVE_LENGTH];
	char *ptr = inputString;
	int rc, nbytes = 0;
	MyMessage ethernetMsg;

	while (1) {
		if ((rc = recv(sockfd, ptr, 1, 0)) > 0) {
			if (*ptr == '\n' || *ptr == '\r') {
				// String terminator
				*ptr = 0;
				debug("Eth: %s\n", inputString);
				if (protocolParse(ethernetMsg, inputString)) {
					pthread_mutex_lock(&ethernetMsg_mutex);
					ethernetMsg_q.push_back(ethernetMsg);
					pthread_mutex_unlock(&ethernetMsg_mutex);
				}

				// Prepare for the next message
				ptr = inputString;
				nbytes = 0;
				continue;
			}
			nbytes++;
			if (nbytes == MY_GATEWAY_MAX_RECEIVE_LENGTH) {
				// Incoming message too long. Throw away
				debug("Eth: Message too long\n");
				ptr = inputString;
				nbytes = 0;
			} else {
				ptr++;
			}
		} else {
			break;
		}
	}
	
	if (rc == -1)
		perror("recv");

	for (uint8_t i = 0; i < MY_GATEWAY_MAX_CLIENTS; i++) {
		if (controllers[i] == sockfd) {
			controllers[i] = -1;
			break;
		}
	}

	close(sockfd);
	return NULL;
}

#ifdef MY_GATEWAY_MQTT_CLIENT
void mqtt_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	(void)mosq;
	(void)userdata;
	
	if(message->payloadlen){
		debug("MQTT: Got a message %s %s\n", message->topic, (char*)message->payload);
	}else{
		debug("MQTT: Got a message %s (null)\n", message->topic);
	}
	
	MyMessage msg;
	if (protocolMQTTParse(msg, message->topic, (uint8_t*)message->payload, message->payloadlen)) {
		if(msg.destination != 0 && msg.sensor != 0 && msg.type != 255)
		{
			pthread_mutex_lock(&ethernetMsg_mutex);
			ethernetMsg_q.push_back(msg);
			pthread_mutex_unlock(&ethernetMsg_mutex);

			// Forward the data to Ethernet
			// Likely this is a duplicate from a C_SET that we received and published
			// in msg_callback.  Not much we can do to avoid the duplicate as there is 
			// no way to tell if we performed the publish that is triggering  this callback.
			char *ethernetMsg = protocolFormat(msg);

			for (uint8_t i = 0; i < MY_GATEWAY_MAX_CLIENTS; i++) {
				if (controllers[i] == -1)
					continue;
				//TODO: sleep/retry before shutdown to check if traffic really is clogged?
				if (send(controllers[i], ethernetMsg, strlen(ethernetMsg), MSG_NOSIGNAL | MSG_DONTWAIT) == -1) {
					if (errno == EAGAIN) {
						// traffic is clogged
						shutdown(controllers[i], SHUT_RDWR);
						controllers[i] = -1;
					}
					perror("send");
				}
			}

			return;
		}
	}

	debug("MQTT: Recieved a bad message: '%s':'%s'\n destination:%i, sensor:%i, type:%i\n", 
	message->topic, (char*)message->payload, msg.destination, msg.sensor, msg.type);
}

void mqtt_connect_callback(struct mosquitto *mosq, void *userdata, int result)
{
	(void)mosq;
	(void)userdata;
	
	if(!result){
		debug("MQTT: Connected!\n");
		/* Subscribe to broker information topics on successful connect. */
		//connection, message id, topic, qos
		mosquitto_subscribe(mosq, NULL, protocolFormatMQTTSubscribe(MY_MQTT_SUBSCRIBE_TOPIC_PREFIX), 0);
	}else{
		fprintf(stderr, "MQTT: Connect failed\n");
	}
}

void mqtt_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{	
	(void)mosq;
	(void)userdata;

	int i;

	debug("MQTT: Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		debug(", %d", granted_qos[i]);
	}
	debug("\n");
}

void mqtt_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	(void)mosq;
	(void)userdata;
	(void)level;
	/* Pring all log messages regardless of level. */
	debug("MQTT: Log: %s\n", str);
}

void *mqtt_thread(void *)
{
	mosquitto_lib_init();
	bool clean_session = true; //will clean out subscriptions  on disconnect
	mosq = mosquitto_new(NULL, clean_session, NULL);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		exit(1);
	}

#ifdef MQTT_DEBUG
	mosquitto_log_callback_set(mosq, mqtt_log_callback);
#endif
	mosquitto_connect_callback_set(mosq, mqtt_connect_callback);
	mosquitto_message_callback_set(mosq, mqtt_message_callback);
	mosquitto_subscribe_callback_set(mosq, mqtt_subscribe_callback);

	if(mosquitto_connect(mosq, MQTT_IP, MQTT_PORT, MQTT_KEEPALIVE)){
		fprintf(stderr, "MQTT: Unable to connect.\n");
		exit(1);
	}

	mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	return NULL;
}

#endif
