/*
 * app_main.h
 *
 *  Created on: 13.03.2017
 *      Author: michaelboeckling
 *  Modified on 15.09.2017 for KaraDio32
 *		jp Cocatrix
 * Copyright (c) 2017, jp Cocatrix
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MAIN_INCLUDE_APP_MAIN_H_
#define MAIN_INCLUDE_APP_MAIN_H_
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <nvs.h>

 //#include "esp_heap_trace.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include "driver/uart.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "mdns.h"

#define TIMER_DIVIDER 16 	//5000000Hz 5MHz
#define TIMER_DIVIDER1MS TIMER_BASE_CLK/10000 //10000Hz 
#define TIMER_DIVIDER1mS 8 //10000000Hz 10MHz

#define TIMERVALUE(x) (x*5000000 )
#define TIMERVALUE1MS(x) (x*10) 
#define TIMERVALUE1mS(x) (x*10000 )
#define TIMERGROUP TIMER_GROUP_0 
#define TIMERGROUP1MS TIMER_GROUP_1
#define TIMERGROUP1mS TIMER_GROUP_1
#define msTimer	TIMER_0
#define microsTimer	TIMER_1
#define sleepTimer  TIMER_0
#define wakeTimer TIMER_1

// event for timers and encoder
#define TIMER_SLEEP   0   
#define TIMER_WAKE    1 
#define TIMER_1MS	2
#define TIMER_1mS	3


// Tasks priority
#define PRIO_VS1053 	9
#define PRIO_CLIENT		8
#define PRIO_TIMER		10

// CPU for task
#define CPU_CLIENT		0
#define CPU_TIMER		0

#define TEMPO_SAVE_VOL	10000

// need this for ported soft to esp32
#define ESP32_IDF

#define PSTR(s) (s)
#define MAXDATAT	 256


#define RELEASE "1.9"
#define REVISION "7"
#define GPIO_NONE 255


#define APMODE		0
#define STA1		1
#define STA2		2
#define SSIDLEN		32
#define PASSLEN		64
#define HOSTLEN		24

struct device_settings {
	uint8_t dhcpEn1;
	uint8_t ipAddr1[4];
	uint8_t mask1[4];
	uint8_t gate1[4];
	uint8_t dhcpEn2;
	uint8_t ipAddr2[4];
	uint8_t mask2[4];
	uint8_t gate2[4];
	char ssid1[SSIDLEN];
	char ssid2[SSIDLEN];
	char pass1[PASSLEN];
	char pass2[PASSLEN];
	uint8_t current_ap; // 0 = AP mode, else STA mode: 1 = ssid1, 2 = ssid2
	uint8_t vol;
	int8_t treble;
	uint8_t bass;
	int8_t freqtreble;
	uint8_t freqbass;
	uint8_t spacial;
	uint16_t currentstation;  // 
	uint8_t autostart; // 0: stopped, 1: playing
	uint8_t i2sspeed; // 0 = 48kHz, 1 = 96kHz, 2 = 128kHz
	uint8_t options;  // bit0:0 theme ligth blue, 1 Dark brown, bit1: 0 patch load  1 no patch, bit2: O blink led  1 led on On play, bit3:led polarity 0 normal 1 reverse 
	uint32_t sleepValue;
	uint32_t wakeValue;
	// esp32
	uint8_t trace_level;
	uint32_t filler;	// timeout in seconds to switch off the lcd. 0 = no timeout
}g_device;

#define kprintf(fmt, ...) do {    \
        printf(fmt, ##__VA_ARGS__);   \
	} while (0)

#define kprintfl(fmt, ...) do {    \
        printf(fmt, ##__VA_ARGS__);   \
	} while (0)


typedef struct {
	int type;               /*!< event type */
	int i1;                 /*!< TIMER_xxx timer group */
	int i2;                 /*!< TIMER_xxx timer number */
} queue_event_t;

void start_network();
void autoPlay();
void switchCommand(void);
void checkCommand(int size, char* s);
esp_log_level_t getLogLevel();
void setLogLevel(esp_log_level_t level);

bool bigSram();

void partitions_init(void);

void sleepCallback(void* pArg);
void wakeCallback(void* pArg);
uint64_t getSleep();
uint64_t getWake();
void startSleep(uint32_t delay);
void stopSleep();
void startWake(uint32_t delay);
void stopWake();
void noInterrupt1Ms();
void interrupt1Ms();
#define noInterrupts noInterrupt1Ms
#define interrupts interrupt1Ms
//void noInterrupts();
//void interrupts();
char* getIp();
esp_netif_t* ap;
esp_netif_t* sta;


#endif /* MAIN_INCLUDE_APP_MAIN_H_ */