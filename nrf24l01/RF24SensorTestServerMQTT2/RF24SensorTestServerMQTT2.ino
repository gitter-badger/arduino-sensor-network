/*
Copyright (C) 2012 J. Coliz <maniacbug@ymail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.
*/

/**
* @file simple_webclient.pde
*
* This example shows how to use NanodeUIP to get a web page.
*/

//#define ETHERSHIELD // uncomment to run on EtherShield
#include <NanodeUIP.h>
#include <psock.h>
#include "webclient.h"
#ifndef ETHERSHIELD
#include <NanodeUNIO.h>
#endif


#ifndef __PRINTF_H__
#define __PRINTF_H__

#ifdef ARDUINO

int serial_putc(char c, FILE *)
{
	Serial.write(c);

	return c;
}

void printf_begin(void)
{
	fdevopen(&serial_putc, 0);
}

#else
#error This example is only for use on Arduino.
#endif // ARDUINO

#endif // __PRINTF_H__


// To ensure that uip-conf.h is set up correctly to accomodate webclient.
UIPASSERT(sizeof(struct webclient_state) <= TCP_APP_STATE_SIZE)

// Dear compiler, please be quiet about those uninitalized progmem 
// warnings.
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] __attribute__ (( section (".progmem") )) = (s); &__c[0];}))

/**************************************************************************/
/**
* @defgroup app_logic Application Logic
*
* The main stuff the sketch does is handled here.
*
* @{
*/

/**
* All available application states handled by the loop()
*/
enum app_states_e
{
	state_none = 0, /**< No state has been set. Illegal */
	state_needip, /**< We are blocked until we get an IP address */
	state_needresolv, /**< We are blocked until we can resolve the host of our service */
	state_noconnection, /**< Everything is ready, webclient needs to be started */
	state_connecting, /**< Trying to connect to server */
	state_done, /**< Application complete.  Stopped. */
	state_invalid  /**< An invalid state.  Illegal */
};

/**
* The current state the application is in
*/
static app_states_e app_state = state_needip;

/**
* Flags related to the application state.  Generally, the application state
* will transition when one of these flags changes state.
*/
struct app_flags_t
{
	uint8_t have_ip : 1;
	uint8_t have_resolv : 1;
};

/**
* The current state of the application flags
*/
static app_flags_t app_flags;

static void dhcp_status_cb(int s, const uint16_t *dnsaddr) {
	char buf[20];
	if (s == DHCP_STATUS_OK) {
		resolv_conf(dnsaddr);
		uip.get_ip_addr_str(buf);
		printf_P(PSTR("%lu: IP: %s\r\n"), millis(), buf);
		app_flags.have_ip = 1;
	}
}

static void resolv_found_cb(char *name, uint16_t *addr) {
	char buf[20];
	uip.format_ipaddr(buf, addr);
	printf_P(PSTR("%lu: DNS: %s has address %s\r\n"), millis(), name, buf);
	app_flags.have_resolv = 1;
}

void ___dhcp_status(int s, const uint16_t *) {
	char buf[20] = "IP:";
	if (s == DHCP_STATUS_OK) {
		uip.get_ip_addr_str(buf + 3);
		Serial.println(buf);

		nanode_log_P(PSTR("Starting web download..."));
		webclient_init();
		webclient_get_P(PSTR("98.136.240.40"), 80, PSTR("/7159/6645514331_38eb2bdeaa_s.jpg"));
	}
}

extern uint16_t* __brkval;

void setup()
{
	//
	// Start networking
	//

#ifdef ETHERSHIELD // EtherShield
	byte macaddr[6] = { 0x2, 0x00, 0x00, 0x1, 0x2, 0x3 };
	uip.init(macaddr, SS);
#else  // Nanode
	byte macaddr[6];
	NanodeUNIO unio(NANODE_MAC_DEVICE);
	unio.read(macaddr, NANODE_MAC_ADDRESS, 6);
	uip.init(macaddr);
#endif

	//
	// Print Preamble
	//
	Serial.begin(38400);
	printf_begin();
	printf_P(PSTR(__FILE__"\r\n"));
	printf_P(PSTR("FREE=%u\r\n"), SP - (uint16_t)__brkval);

	//
	// Setup Ethernet networking
	//

	char buf[20];
	uip.get_mac_str(buf);
	printf_P(PSTR("MAC: %s\r\n"), buf);
	uip.wait_for_link();
	nanode_log_P(PSTR("Link is up"));
	uip.start_dhcp(dhcp_status_cb);
	uip.init_resolv(resolv_found_cb);
}

void loop() {
	uip.poll();

	switch (app_state)
	{
	case state_needip:
		if (app_flags.have_ip)
		{
			// launch resolver
			uip.query_name("www.gravatar.com");
			app_state = state_needresolv;
		}
		break;
	case state_needresolv:
		if (app_flags.have_resolv)
		{
			app_state = state_noconnection;
		}
		break;
	case state_noconnection:
	{
							   // Try to connect
							   nanode_log_P(PSTR("Starting web download..."));
							   webclient_init();
							   webclient_get_P(PSTR("www.gravatar.com"), 80, PSTR("/avatar/05a71c61ee0246ac3a180c5fadeb99ed"));
							   app_state = state_connecting;
	}
		break;
	case state_done:
		printf_P(PSTR("+OK\r\n"));
		app_state = state_none;
		break;
	case state_connecting:
	case state_none:
	case state_invalid:
		break;
	}
}

// Stats
uint32_t size_received = 0;
uint32_t started_at = 0;

/****************************************************************************/
/**
* Callback function that is called from the webclient code when HTTP
* data has been received.
*
* This function must be implemented by the module that uses the
* webclient code. The function is called from the webclient module
* when HTTP data has been received. The function is not called when
* HTTP headers are received, only for the actual data.
*
* \note This function is called many times, repetedly, when data is
* being received, and not once when all data has been received.
*
* \param data A pointer to the data that has been received.
* \param len The length of the data that has been received.
*/
void webclient_datahandler(char *data, u16_t len)
{
	//printf_P(PSTR("%lu: webclient_datahandler data=%p len=%u\r\n"),millis(),data,len);
	Serial.print('.');

	if (!started_at)
		started_at = millis();

	size_received += len;
#if 0
	// Dump out the text
	while (len--)
	{
		char c = *data++;
		if (c == '\n')
			Serial.print('\r');
		Serial.print(c);
	}
	Serial.println();
#endif

	// If data is NULL, we are done.  Print the stats
	if (!data)
	{
		Serial.println();
		printf_P(PSTR("%lu: DONE. Received %lu bytes in %lu msec.\r\n"), millis(), size_received, millis() - started_at);
		app_state = state_done;
		started_at = 0;
	}
}

/****************************************************************************/
/**
* Callback function that is called from the webclient code when the
* HTTP connection has been connected to the web server.
*
* This function must be implemented by the module that uses the
* webclient code.
*/
void webclient_connected(void)
{
	uip_log_P(PSTR("webclient_connected"));
}

/****************************************************************************/
/**
* Callback function that is called from the webclient code if the
* HTTP connection to the web server has timed out.
*
* This function must be implemented by the module that uses the
* webclient code.
*/
void webclient_timedout(void)
{
	printf_P(PSTR("webclient_timedout\r\n"));
	app_state = state_noconnection;
}

/****************************************************************************/
/**
* Callback function that is called from the webclient code if the
* HTTP connection to the web server has been aborted by the web
* server.
*
* This function must be implemented by the module that uses the
* webclient code.
*/
void webclient_aborted(void)
{
	printf_P(PSTR("webclient_aborted\r\n"));
	app_state = state_noconnection;
}

/****************************************************************************/
/**
* Callback function that is called from the webclient code when the
* HTTP connection to the web server has been closed.
*
* This function must be implemented by the module that uses the
* webclient code.
*/
void webclient_closed(void)
{
	printf_P(PSTR("webclient_closed\r\n"));
	app_state = state_noconnection;
}

// vim:cin:ai:sts=2 sw=2 ft=cpp
// vim:cin:ai:sts=2 sw=2 ft=cpp
