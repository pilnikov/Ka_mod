/*
  KaRadio 32
  A WiFi webradio player
Copyright (C) 2017  KaraWin
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "app_main.h"
#include "ClickButtons.h"
#include "addonu8g2.h"
#include "spiram_fifo.h"
#include "audio_renderer.h"

#include "audio_player.h"
#include <u8g2.h>
#include "u8g2_esp32_hal.h"
#include "addon.h"

#include "eeprom.h"

/////////////////////////////////////////////////////
///////////////////////////
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "nvs_flash.h"
#include "gpio.h"
#include "servers.h"
#include "webclient.h"
#include "webserver.h"
#include "interface.h"
#include "vs10xx.h"
#include "ClickEncoder.h"
#include "addon.h"
//#include "rda5807Task.h"

/* The event group allows multiple bits for each event*/
//   are we connected  to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;
//
const int CONNECTED_AP = 0x00000010;

#define TAG "main"

//Priorities of the reader and the decoder thread. bigger number = higher prio
#define PRIO_READER configMAX_PRIORITIES - 3
#define PRIO_MQTT configMAX_PRIORITIES - 3
#define PRIO_CONNECT configMAX_PRIORITIES - 1
#define striWATERMARK "watermark: %d  heap: %d"

void start_network();
void autoPlay();
/* */
static bool wifiInitDone = false;
static EventGroupHandle_t wifi_event_group;
xQueueHandle event_queue;

//xSemaphoreHandle print_mux;
static uint16_t FlashOn = 5, FlashOff = 5;
bool ledStatus = true;	// true: normal blink, false: led on when playing
bool ledPolarity = false; // false: normal true: reverse
player_t *player_config;
static output_mode_t audio_output_mode;
static uint8_t clientIvol = 0;
//ip
static char localIp[20];
// 4MB sram?
static bool bigRam = false;
// timeout to save volume in flash
static uint32_t ctimeVol = 0;
static uint32_t ctimeMs = 0;
static bool divide = false;

// disable 1MS timer interrupt
IRAM_ATTR void noInterrupt1Ms() { timer_disable_intr(TIMERGROUP1MS, msTimer); }
// enable 1MS timer interrupt
IRAM_ATTR void interrupt1Ms() { timer_enable_intr(TIMERGROUP1MS, msTimer); }
//IRAM_ATTR void noInterrupts() {noInterrupt1Ms();}
//IRAM_ATTR void interrupts() {interrupt1Ms();}

IRAM_ATTR char *getIp() { return (localIp); }
IRAM_ATTR uint8_t getIvol() { return clientIvol; }
IRAM_ATTR void setIvol(uint8_t vol)
{
	clientIvol = vol;
	ctimeVol = 0;
}
IRAM_ATTR output_mode_t get_audio_output_mode() { return audio_output_mode; }

/*
IRAM_ATTR void   microsCallback(void *pArg) {
	int timer_idx = (int) pArg;
	queue_event_t evt;	
	TIMERG1.hw_timer[timer_idx].update = 1;
	TIMERG1.int_clr_timers.t1 = 1; //isr ack
		evt.type = TIMER_1mS;
        evt.i1 = TIMERGROUP1mS;
        evt.i2 = timer_idx;
	xQueueSendFromISR(event_queue, &evt, NULL);	
	TIMERG1.hw_timer[timer_idx].config.alarm_en = 1;
}*/
//
IRAM_ATTR bool bigSram() { return bigRam; }
//-----------------------------------
// every 500µs
IRAM_ATTR void msCallback(void *pArg)
{
	int timer_idx = (int)pArg;

	//	queue_event_t evt;
	TIMERG1.hw_timer[timer_idx].update = 1;
	TIMERG1.int_clr_timers.t0 = 1; //isr ack
	if (divide)
	{
		ctimeMs++;  // for led
		ctimeVol++; // to save volume
	}
	divide = !divide;
	if (serviceAddon != NULL)
		serviceAddon(); // for the encoders and buttons
	TIMERG1.hw_timer[timer_idx].config.alarm_en = 1;
}

IRAM_ATTR void sleepCallback(void *pArg)
{
	int timer_idx = (int)pArg;
	queue_event_t evt;
	TIMERG0.int_clr_timers.t0 = 1; //isr ack
	evt.type = TIMER_SLEEP;
	evt.i1 = TIMERGROUP;
	evt.i2 = timer_idx;
	xQueueSendFromISR(event_queue, &evt, NULL);
	TIMERG0.hw_timer[timer_idx].config.alarm_en = 0;
}

IRAM_ATTR void wakeCallback(void *pArg)
{
	int timer_idx = (int)pArg;
	queue_event_t evt;
	TIMERG0.int_clr_timers.t1 = 1;
	evt.i1 = TIMERGROUP;
	evt.i2 = timer_idx;
	evt.type = TIMER_WAKE;
	xQueueSendFromISR(event_queue, &evt, NULL);
	TIMERG0.hw_timer[timer_idx].config.alarm_en = 0;
}

uint64_t getSleep()
{
	uint64_t ret = 0;
	uint64_t tot = 0;
	timer_get_alarm_value(TIMERGROUP, sleepTimer, &tot);
	timer_get_counter_value(TIMERGROUP, sleepTimer, &ret);
	return ((tot - ret) / 5000000);
}
uint64_t getWake()
{
	uint64_t ret = 0;
	uint64_t tot = 0;
	timer_get_alarm_value(TIMERGROUP, wakeTimer, &tot);
	timer_get_counter_value(TIMERGROUP, wakeTimer, &ret);
	return ((tot - ret) / 5000000);
}

void tsocket(const char *lab, uint32_t cnt)
{
	char *title = malloc(88);
	sprintf(title, "{\"%s\":\"%d\"}", lab, cnt * 60);
	websocketbroadcast(title, strlen(title));
	free(title);
}

void stopSleep()
{
	ESP_LOGD(TAG, "stopDelayDelay\n");
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP, sleepTimer));
	tsocket("lsleep", 0);
}

void startSleep(uint32_t delay)
{
	ESP_LOGD(TAG, "Delay:%d\n", delay);
	if (delay == 0)
		return;
	stopSleep();
	ESP_ERROR_CHECK(timer_set_counter_value(TIMERGROUP, sleepTimer, 0x00000000ULL));
	ESP_ERROR_CHECK(timer_set_alarm_value(TIMERGROUP, sleepTimer, TIMERVALUE(delay * 60)));
	ESP_ERROR_CHECK(timer_enable_intr(TIMERGROUP, sleepTimer));
	ESP_ERROR_CHECK(timer_set_alarm(TIMERGROUP, sleepTimer, TIMER_ALARM_EN));
	ESP_ERROR_CHECK(timer_start(TIMERGROUP, sleepTimer));
}

void stopWake()
{
	ESP_LOGD(TAG, "stopDelayWake\n");
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP, wakeTimer));
	tsocket("lwake", 0);
}

void startWake(uint32_t delay)
{
	ESP_LOGD(TAG, "Wake Delay:%d\n", delay);
	if (delay == 0)
		return;
	stopWake();
	ESP_ERROR_CHECK(timer_set_counter_value(TIMERGROUP, wakeTimer, 0x00000000ULL));
	//TIMER_INTERVAL0_SEC * TIMER_SCALE - TIMER_FINE_ADJ
	ESP_ERROR_CHECK(timer_set_alarm_value(TIMERGROUP, wakeTimer, TIMERVALUE(delay * 60)));
	ESP_ERROR_CHECK(timer_enable_intr(TIMERGROUP, wakeTimer));
	ESP_ERROR_CHECK(timer_set_alarm(TIMERGROUP, wakeTimer, TIMER_ALARM_EN));
	ESP_ERROR_CHECK(timer_start(TIMERGROUP, wakeTimer));
	tsocket("lwake", delay);
}

void initTimers()
{
	timer_config_t config;
	config.alarm_en = 1;
	config.auto_reload = TIMER_AUTORELOAD_DIS;
	config.counter_dir = TIMER_COUNT_UP;
	config.divider = TIMER_DIVIDER;
	config.intr_type = TIMER_INTR_LEVEL;
	config.counter_en = TIMER_PAUSE;

	/*Configure timer sleep*/
	ESP_ERROR_CHECK(timer_init(TIMERGROUP, sleepTimer, &config));
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP, sleepTimer));
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP, sleepTimer, sleepCallback, (void *)sleepTimer, 0, NULL));
	/*Configure timer wake*/
	ESP_ERROR_CHECK(timer_init(TIMERGROUP, wakeTimer, &config));
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP, wakeTimer));
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP, wakeTimer, wakeCallback, (void *)wakeTimer, 0, NULL));
	/*Configure timer 1MS*/
	config.auto_reload = TIMER_AUTORELOAD_EN;
	config.divider = TIMER_DIVIDER1MS;
	ESP_ERROR_CHECK(timer_init(TIMERGROUP1MS, msTimer, &config));
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP1MS, msTimer));
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP1MS, msTimer, msCallback, (void *)msTimer, 0, NULL));
	/* start 1MS timer*/
	ESP_ERROR_CHECK(timer_set_counter_value(TIMERGROUP1MS, msTimer, 0x00000000ULL));
	ESP_ERROR_CHECK(timer_set_alarm_value(TIMERGROUP1MS, msTimer, TIMERVALUE1MS(1)));
	ESP_ERROR_CHECK(timer_enable_intr(TIMERGROUP1MS, msTimer));
	ESP_ERROR_CHECK(timer_set_alarm(TIMERGROUP1MS, msTimer, TIMER_ALARM_EN));
	ESP_ERROR_CHECK(timer_start(TIMERGROUP1MS, msTimer));

	/*Configure timer 1µS*/
	/*	config.auto_reload = TIMER_AUTORELOAD_EN;
	config.divider = TIMER_DIVIDER1mS;
	ESP_ERROR_CHECK(timer_init(TIMERGROUP1mS, microsTimer, &config));
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP1mS, microsTimer));
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP1mS, microsTimer, microsCallback, (void*) microsTimer, 0, NULL));
*/
	/* start 1µS timer*/
	/*	ESP_ERROR_CHECK(timer_set_counter_value(TIMERGROUP1mS, microsTimer, 0x00000000ULL));
	ESP_ERROR_CHECK(timer_set_alarm_value(TIMERGROUP1mS, microsTimer,TIMERVALUE1mS(10))); // 10 ms timer
	ESP_ERROR_CHECK(timer_enable_intr(TIMERGROUP1mS, microsTimer));
	ESP_ERROR_CHECK(timer_set_alarm(TIMERGROUP1mS, microsTimer,TIMER_ALARM_EN));
	ESP_ERROR_CHECK(timer_start(TIMERGROUP1mS, microsTimer));*/
}

//////////////////////////////////////////////////////////////////

// Renderer config creation
static renderer_config_t *create_renderer_config()
{
	renderer_config_t *renderer_config = calloc(1, sizeof(renderer_config_t));

	renderer_config->bit_depth = I2S_BITS_PER_SAMPLE_16BIT;
	renderer_config->i2s_num = I2S_NUM_0;
	renderer_config->sample_rate = 44100;
	renderer_config->sample_rate_modifier = 1.0;
	renderer_config->output_mode = audio_output_mode;

	if (renderer_config->output_mode == I2S_MERUS)
	{
		renderer_config->bit_depth = I2S_BITS_PER_SAMPLE_32BIT;
	}

	if (renderer_config->output_mode == DAC_BUILT_IN)
	{
		renderer_config->bit_depth = I2S_BITS_PER_SAMPLE_16BIT;
	}

	return renderer_config;
}

/******************************************************************************
 * FunctionName : checkUart
 * Description  : Check for a valid uart baudrate
 * Parameters   : baud
 * Returns      : baud
*******************************************************************************/
uint32_t checkUart(uint32_t speed)
{
	uint32_t valid[] = {1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 76880, 115200, 230400};
	int i;
	for (i = 0; i < 12; i++)
	{
		if (speed == valid[i])
			return speed;
	}
	return 115200; // default
}

/******************************************************************************
 * FunctionName : init_hardware
 * Description  : Init all hardware, partitions etc
 * Parameters   : 
 * Returns      : 
*******************************************************************************/
static void init_vs_hw()
{
	if (vsHW_init()) // init spi
		{
			vsStart();
			ESP_LOGI(TAG, "vsHW initialized");
		}
}

/* event handler for pre-defined wifi events */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
							   int32_t event_id, void *event_data)
{
	//EventGroupHandle_t wifi_event = ctx;

	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		FlashOn = FlashOff = 100;
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		/* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
		FlashOn = FlashOff = 100;
		xEventGroupClearBits(wifi_event_group, CONNECTED_AP);
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		ESP_LOGE(TAG, "Wifi Disconnected.");
		vTaskDelay(10);
		if (!getAutoWifi() && (wifiInitDone))
		{
			ESP_LOGE(TAG, "reboot");
			vTaskDelay(10);
			esp_restart();
		}
		else
		{
			if (wifiInitDone) // a completed init done
			{
				ESP_LOGE(TAG, "Connection tried again");
				//clientDisconnect("Wifi Disconnected.");
				clientSilentDisconnect();
				vTaskDelay(100);
				clientSaveOneHeader("Wifi Disconnected.", 18, METANAME);
				vTaskDelay(100);
				while (esp_wifi_connect() == ESP_ERR_WIFI_SSID)
					vTaskDelay(10);
			}
			else
			{
				ESP_LOGE(TAG, "Try next AP");
				vTaskDelay(10);
			} // init failed?
		}
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
	{
		xEventGroupSetBits(wifi_event_group, CONNECTED_AP);
		ESP_LOGI(TAG, "Wifi connected");
		if (wifiInitDone)
		{
			clientSaveOneHeader("Wifi Connected.", 18, METANAME);
			vTaskDelay(200);
			autoPlay();
		} // retry
		else
		{
			wifiInitDone = true;
		}
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		FlashOn = 5;
		FlashOff = 395;
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	}
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
	{
		FlashOn = 5;
		FlashOff = 395;

		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		xEventGroupSetBits(wifi_event_group, CONNECTED_AP);
		wifiInitDone = true;
	}
}

static void unParse(char *str)
{
	int i;
	if (str == NULL)
		return;
	for (i = 0; i < strlen(str); i++)
	{
		if (str[i] == '\\')
		{
			str[i] = str[i + 1];
			str[i + 1] = 0;
			if (str[i + 2] != 0)
				strcat(str, str + i + 2);
		}
	}
}

static void start_wifi()
{
	ESP_LOGI(TAG, "Starting WiFi");

	setAutoWifi();

	char ssid[SSIDLEN];
	char pass[PASSLEN];

	static bool first_pass = false;

	static bool initialized = false;
	if (!initialized)
	{
		esp_netif_init();
		wifi_event_group = xEventGroupCreate();
		ESP_ERROR_CHECK(esp_event_loop_create_default());
		ap = esp_netif_create_default_wifi_ap();
		assert(ap);
		sta = esp_netif_create_default_wifi_sta();
		assert(sta);
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
		ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
		ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
		ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
		//ESP_ERROR_CHECK( esp_wifi_start() );
		initialized = true;
	}
	ESP_LOGI(TAG, "WiFi init done!");

	if (g_device->current_ap == APMODE)
	{
		if (strlen(g_device->ssid1) != 0)
		{
			g_device->current_ap = STA1;
		}
		else
		{
			if (strlen(g_device->ssid2) != 0)
			{
				g_device->current_ap = STA2;
			}
			else
				g_device->current_ap = APMODE;
		}
		saveDeviceSettings(g_device);
	}

	while (1)
	{
		if (first_pass)
		{
			ESP_ERROR_CHECK(esp_wifi_stop());
			vTaskDelay(5);
		}
		switch (g_device->current_ap)
		{
		case STA1: //ssid1 used
			strcpy(ssid, g_device->ssid1);
			strcpy(pass, g_device->pass1);
			esp_wifi_set_mode(WIFI_MODE_STA);
			break;
		case STA2: //ssid2 used
			strcpy(ssid, g_device->ssid2);
			strcpy(pass, g_device->pass2);
			esp_wifi_set_mode(WIFI_MODE_STA);
			break;

		default: // other: AP mode
			g_device->current_ap = APMODE;
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		}

		if (g_device->current_ap == APMODE)
		{
			printf("WIFI GO TO AP MODE\n");
			wifi_config_t ap_config = {
				.ap = {
					.ssid = "WifiKaradio",
					.authmode = WIFI_AUTH_OPEN,
					.max_connection = 2,
					.beacon_interval = 200},
			};
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
			ESP_LOGE(TAG, "The default AP is  WifiKaRadio. Connect your wifi to it.\nThen connect a webbrowser to 192.168.4.1 and go to Setting\nMay be long to load the first time.Be patient.");

			vTaskDelay(1);
			ESP_ERROR_CHECK(esp_wifi_start());
		}
		else
		{
			printf("WIFI TRYING TO CONNECT TO SSID %d\n", g_device->current_ap);
			wifi_config_t wifi_config =
				{
					.sta =
						{
							.bssid_set = 0,
						},
				};
			strcpy((char *)wifi_config.sta.ssid, ssid);
			strcpy((char *)wifi_config.sta.password, pass);
			unParse((char *)(wifi_config.sta.ssid));
			unParse((char *)(wifi_config.sta.password));
			if (strlen(ssid) /*&&strlen(pass)*/)
			{
				if (CONNECTED_BIT > 1)
				{
					esp_wifi_disconnect();
				}
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
				//	ESP_LOGI(TAG, "connecting %s, %d, %s, %d",ssid,strlen((char*)(wifi_config.sta.ssid)),pass,strlen((char*)(wifi_config.sta.password)));
				ESP_LOGI(TAG, "connecting %s", ssid);
				ESP_ERROR_CHECK(esp_wifi_start());
			}
			else
			{
				g_device->current_ap++;
				g_device->current_ap %= 3;

				if (getAutoWifi() && (g_device->current_ap == APMODE))
				{
					g_device->current_ap = STA1; // if autoWifi then wait for a reconnection to an AP
					printf("Wait for the AP\n");
				}
				else
				{
					printf("Empty AP. Try next one\n");
				}

				saveDeviceSettings(g_device);
				continue;
			}
		}

		/* Wait for the callback to set the CONNECTED_AP in the event group. */
		if ((xEventGroupWaitBits(wifi_event_group, CONNECTED_AP, false, true, 2000) & CONNECTED_AP) == 0)
		//timeout . Try the next AP
		{
			g_device->current_ap++;
			g_device->current_ap %= 3;
			saveDeviceSettings(g_device);
			printf("\ndevice->current_ap: %d\n", g_device->current_ap);
		}
		else
			break; //
		first_pass = true;
	}
}

void start_network()
{
	//	struct device_settings *g_device;

	esp_netif_ip_info_t info;
	wifi_mode_t mode;
	ip4_addr_t ipAddr;
	ip4_addr_t mask;
	ip4_addr_t gate;
	uint8_t dhcpEn = 0;

	IP4_ADDR(&ipAddr, 192, 168, 4, 1);
	IP4_ADDR(&gate, 192, 168, 4, 1);
	IP4_ADDR(&mask, 255, 255, 255, 0);

	esp_netif_dhcpc_stop(sta); // Don't run a DHCP client

	switch (g_device->current_ap)
	{
	case STA1: //ssid1 used
		IP4_ADDR(&ipAddr, g_device->ipAddr1[0], g_device->ipAddr1[1], g_device->ipAddr1[2], g_device->ipAddr1[3]);
		IP4_ADDR(&gate, g_device->gate1[0], g_device->gate1[1], g_device->gate1[2], g_device->gate1[3]);
		IP4_ADDR(&mask, g_device->mask1[0], g_device->mask1[1], g_device->mask1[2], g_device->mask1[3]);
		dhcpEn = g_device->dhcpEn1;
		break;
	case STA2: //ssid2 used
		IP4_ADDR(&ipAddr, g_device->ipAddr2[0], g_device->ipAddr2[1], g_device->ipAddr2[2], g_device->ipAddr2[3]);
		IP4_ADDR(&gate, g_device->gate2[0], g_device->gate2[1], g_device->gate2[2], g_device->gate2[3]);
		IP4_ADDR(&mask, g_device->mask2[0], g_device->mask2[1], g_device->mask2[2], g_device->mask2[3]);
		dhcpEn = g_device->dhcpEn2;
		break;
	}

	ip4_addr_copy(info.ip, ipAddr);
	ip4_addr_copy(info.gw, gate);
	ip4_addr_copy(info.netmask, mask);

	ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));

	if (mode == WIFI_MODE_AP)
	{
		xEventGroupWaitBits(wifi_event_group, CONNECTED_AP, false, true, 3000);
		ip4_addr_copy(info.ip, ipAddr);
		esp_netif_set_ip_info(ap, &info);

		esp_netif_ip_info_t ap_ip_info;
		ap_ip_info.ip.addr = 0;
		while (ap_ip_info.ip.addr == 0)
		{
			esp_netif_get_ip_info(ap, &ap_ip_info);
		}
	}
	else // mode STA
	{
		if (dhcpEn) // check if ip is valid without dhcp
		{
			esp_netif_dhcpc_start(sta); //  run a DHCP client
		}
		else
		{
			ESP_ERROR_CHECK(esp_netif_set_ip_info(sta, &info));
			dns_clear_servers(false);
			IP_SET_TYPE((ip_addr_t *)&info.gw, IPADDR_TYPE_V4); // mandatory
			((ip_addr_t *)&info.gw)->type = IPADDR_TYPE_V4;
			dns_setserver(0, (ip_addr_t *)&info.gw);
			dns_setserver(1, (ip_addr_t *)&info.gw); // if static ip	check dns
		}

		// wait for ip
		if ((xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 3000) & CONNECTED_BIT) == 0) //timeout
		{																									// enable dhcp and restart
			if (g_device->current_ap == 1)
			{
				g_device->dhcpEn1 = 1;
			}
			else
			{
				g_device->dhcpEn2 = 1;
			}
			saveDeviceSettings(g_device);
			esp_restart();
		}

		vTaskDelay(1);

		// retrieve the current ip
		esp_netif_ip_info_t sta_ip_info;
		sta_ip_info.ip.addr = 0;
		while (sta_ip_info.ip.addr == 0)
		{
			esp_netif_get_ip_info(sta, &sta_ip_info);
		}

		const ip_addr_t *ipdns0 = dns_getserver(0);
		ESP_LOGW(TAG, "DNS: %s  \n", ip4addr_ntoa((struct ip4_addr *)&ipdns0));

		if (dhcpEn) // if dhcp enabled update fields
		{
			esp_netif_get_ip_info(sta, &info);
			switch (g_device->current_ap)
			{
			case STA1: //ssid1 used
				ip4addr_aton((const char *)&g_device->ipAddr1,(ip4_addr_t *)&info.ip);
				ip4addr_aton((const char *)&g_device->gate1,(ip4_addr_t *)&info.gw);
				ip4addr_aton((const char *)&g_device->mask1,(ip4_addr_t *)&info.netmask);
				break;

			case STA2: //ssid2 used
				ip4addr_aton((const char *)&g_device->ipAddr2,(ip4_addr_t *)&info.ip);
				ip4addr_aton((const char *)&g_device->gate2,(ip4_addr_t *)&info.gw);
				ip4addr_aton((const char *)&g_device->mask2,(ip4_addr_t *)&info.netmask);
				break;
			}
		}
		saveDeviceSettings(g_device);
		esp_netif_set_hostname(sta, "karadio32");
	}
	ip4_addr_copy(ipAddr, info.ip);
	strcpy(localIp, ip4addr_ntoa(&ipAddr));
	ESP_LOGW(TAG, "IP: %s\n\n", localIp);

	if (g_device->lcd_type != LCD_NONE)
		lcd_welcome(localIp, "IP found");
	vTaskDelay(10);
}

//blinking led and timer isr
void timerTask(void *p)
{
	//	struct device_settings *device;
	uint32_t cCur;
	bool stateLed = false;
	bool isEsplay;
	gpio_num_t gpioLed;
	//	int uxHighWaterMark;

	initTimers();
	isEsplay = option_get_esplay();

	// led mode
	ledStatus = !(g_device->options & T_LED);
	ledPolarity = g_device->options & T_LEDPOL;
	gpioLed = getLedGpio();
	gpio_get_ledgpio(&gpioLed);
	setLedGpio(gpioLed);
	if (gpioLed != GPIO_NONE)
	{
		gpio_output_conf(gpioLed);
		gpio_set_level(getLedGpio(), false ^ ledPolarity);

	}

	cCur = FlashOff * 10;
	queue_event_t evt;

	while (1)
	{
		// read and treat the timer queue events
		//		int nb = uxQueueMessagesWaiting(event_queue);
		//		if (nb >29) printf(" %d\n",nb);
		while (xQueueReceive(event_queue, &evt, 0))
		{
			switch (evt.type)
			{
			case TIMER_SLEEP:
				clientDisconnect("Timer"); // stop the player
				break;
			case TIMER_WAKE:
				clientConnect(); // start the player
				break;
			default:
				break;
			}
		}
		if (ledStatus)
		{
			if (ctimeMs >= cCur)
			{
				gpioLed = getLedGpio();

				if (stateLed)
				{
					stateLed = false;
					cCur = FlashOff * 10;
				}
				else
				{
					stateLed = true;
					cCur = FlashOn * 10;
				}
				if (getLedGpio() != GPIO_NONE)
					gpio_set_level(getLedGpio(), ledPolarity ^ stateLed);
				ctimeMs = 0;
			}
		}

		vTaskDelay(10);

		if (isEsplay)				  // esplay board only
			rexp = i2c_keypad_read(); // read the expansion
	}
	//	printf("t0 end\n");

	vTaskDelete(NULL); // stop the task (never reached)
}

void uartInterfaceTask(void *pvParameters)
{
	char tmp[255];
	int d;
	uint8_t c;
	int t;
	//	struct device_settings *device;
	uint32_t uspeed;
	int uxHighWaterMark;

	uspeed = g_device->uartspeed;
	uart_config_t uart_config0 = {
		.baud_rate = uspeed,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE, //UART_HW_FLOWCTRL_CTS_RTS,
		.rx_flow_ctrl_thresh = 0,
	};
	uart_param_config(UART_NUM_0, &uart_config0);
	uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);

	for (t = 0; t < sizeof(tmp); t++)
		tmp[t] = 0;
	t = 0;

	while (1)
	{
		while (1)
		{
			d = uart_read_bytes(UART_NUM_0, &c, 1, 100);
			if (d > 0)
			{
				if ((char)c == '\r')
					break;
				if ((char)c == '\n')
					break;
				tmp[t] = (char)c;
				t++;
				if (t == sizeof(tmp) - 1)
					t = 0;
			}
			//else printf("uart d: %d, T= %d\n",d,t);
			//switchCommand() ;  // hardware panel of command
		}
		checkCommand(t, tmp);

		uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
		ESP_LOGD("uartInterfaceTask", striWATERMARK, uxHighWaterMark, xPortGetFreeHeapSize());

		for (t = 0; t < sizeof(tmp); t++)
			tmp[t] = 0;
		t = 0;
	}
}

// In STA mode start a station or start in pause mode.
// Show ip on AP mode.
void autoPlay()
{
	char apmode[50];
	sprintf(apmode, "at IP %s", localIp);
	if (g_device->current_ap == APMODE)
	{
		clientSaveOneHeader("Configure the AP with the web page", 34, METANAME);
		clientSaveOneHeader(apmode, strlen(apmode), METAGENRE);
	}
	else
	{
		clientSaveOneHeader(apmode, strlen(apmode), METANAME);
		if ((audio_output_mode == VS10xx) && (get_vsVersion() < 3))
		{
			clientSaveOneHeader("Invalid audio output. VS10xx not found", 38, METAGENRE);
			ESP_LOGE(TAG, "Invalid audio output. VS10xx not found");
			vTaskDelay(200);
		}

		setCurrentStation(g_device->currentstation);
		if ((g_device->autostart == 1) && (g_device->currentstation != 0xFFFF))
		{
			kprintf("autostart: playing:%d, currentstation:%d\n", g_device->autostart, g_device->currentstation);
			vTaskDelay(50); // wait a bit
			playStationInt(g_device->currentstation);
		}
		else
			clientSaveOneHeader("Ready", 5, METANAME);
	}
}

/********************************************************************
 ***********************   Main entry point     *********************
 ********************************************************************/
void app_main()
{
	uint32_t uspeed;
	xTaskHandle pxCreatedTask;
	esp_err_t err;

	ESP_LOGW(TAG, "starting app_main()");
	ESP_LOGI(TAG, "RAM left: %u", esp_get_free_heap_size());

	const esp_partition_t *running = esp_ota_get_running_partition();
	ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
			 running->type, running->subtype, running->address);
	// Initialize NVS.
	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES)
	{
		// OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	// Check if we are in large Sram config
	if (xPortGetFreeHeapSize() > 0x80000)
		bigRam = true;

	partitions_init(); // init partition table
	ESP_LOGI(TAG, "Partition init done...");

	if (g_device->cleared != 0xAABB)
	{
		ESP_LOGE(TAG, "Device config not ok. Try to restore");
		free(g_device);
		restoreDeviceSettings(); // try to restore the config from the saved one
		g_device = getDeviceSettings();
		if (g_device->cleared != 0xAABB)
		{
			ESP_LOGE(TAG, "Device config not cleared. Clear it.");
			free(g_device);
			eeEraseAll();
			g_device = getDeviceSettings();
			g_device->cleared = 0xAABB;		   //marker init done
			g_device->uartspeed = 115200;	  // default
			g_device->audio_output_mode = I2S; // default
			option_get_audio_output(&(g_device->audio_output_mode));
			g_device->trace_level = ESP_LOG_ERROR; //default
			g_device->vol = 100;				   //default
			g_device->led_gpio = GPIO_NONE;
			saveDeviceSettings(g_device);
		}
		else
			ESP_LOGE(TAG, "Device config restored");
	}

	copyDeviceSettings(); // copy in the safe partion
	
	// init softwares
	telnetinit();
	websocketinit();

	// log level
	setLogLevel(g_device->trace_level);

	//time display
	uint8_t ddmm;
	option_get_ddmm(&ddmm);
	setDdmm(ddmm ? 1 : 0);

	//SPI init for the vs1053 and lcd if spi.

	uint8_t spi_no;
	gpio_num_t miso;
	gpio_num_t mosi;
	gpio_num_t sclk;
	gpio_num_t sda;
	gpio_num_t scl;
	gpio_num_t rsti2c;

	uint8_t rt;
	option_get_lcd_info(&g_device->lcd_type, &rt);

	gpio_get_spi_bus(&spi_no, &miso, &mosi, &sclk);
	if (miso != GPIO_NONE && mosi != GPIO_NONE && sclk != GPIO_NONE && spi_no > 0 && spi_no < 3)
	{
		Spi_init();
	}
	else
	{
		gpio_get_i2c(&scl, &sda, &rsti2c);
		if (scl == GPIO_NONE || sda == GPIO_NONE)
		{
			g_device->lcd_type = LCD_NONE;
		}
	}
	// output mode one of  I2S, I2S_MERUS, DAC_BUILT_IN, PDM, VS1053, A1S
	audio_output_mode = g_device->audio_output_mode;
	ESP_LOGI(TAG, "audio_output_mode %d\nOne of I2S=0, I2S_MERUS, DAC_BUILT_IN, PDM, VS1053, A1S", audio_output_mode);

	if (audio_output_mode == 4)
	{
		init_vs_hw(); //init vs1053 if in mode sel
	}

	// ESP_LOGE(TAG,"Corrupt1 %d",heap_caps_check_integrity(MALLOC_CAP_DMA,1));

	ESP_LOGI(TAG, "LCD Type %d", g_device->lcd_type); // lcd init

	setRotat(rt); // lcd rotation

	lcd_init(g_device->lcd_type);

	//Initialize the SPI RAM chip communications and see if it actually retains some bytes. If it
	//doesn't, warn user.
	if (bigRam)
	{
		setSPIRAMSIZE(1024 * 1024); // room in psram
		ESP_LOGI(TAG, "Set Song buffer to 1024k");
	}
	else
	{
		if (audio_output_mode == VS10xx)
			setSPIRAMSIZE(50 * 1024); // more free heap
		ESP_LOGI(TAG, "Set Song buffer to 50k");
	}

	if (!spiRamFifoInit())
	{
		ESP_LOGE(TAG, "SPI RAM chip fail!");
		esp_restart();
	}

	//uart speed
	uspeed = g_device->uartspeed;
	uspeed = checkUart(uspeed);
	uart_set_baudrate(UART_NUM_0, uspeed);
	ESP_LOGI(TAG, "Set baudrate at %d", uspeed);
	if (g_device->uartspeed != uspeed)
	{
		g_device->uartspeed = uspeed;
		saveDeviceSettings(g_device);
	}
	// Version infos
	ESP_LOGI(TAG, "Release %s, Revision %s", RELEASE, REVISION);
	ESP_LOGI(TAG, "SDK %s", esp_get_idf_version());
	ESP_LOGI(TAG, "Heap size: %d", xPortGetFreeHeapSize());

	if (g_device->lcd_type != LCD_NONE)
	{
		lcd_welcome("", "");
		lcd_welcome("", "STARTING");
	}

	// volume
	setIvol(g_device->vol);
	ESP_LOGI(TAG, "Volume set to %d", g_device->vol);

	// queue for events of the sleep / wake and Ms timers
	event_queue = xQueueCreate(30, sizeof(queue_event_t));
	// led blinks
	xTaskCreatePinnedToCore(timerTask, "timerTask", 2100, NULL, PRIO_TIMER, &pxCreatedTask, CPU_TIMER);
	ESP_LOGI(TAG, "%s task: %x", "t0", (unsigned int)pxCreatedTask);

	//-----------------------------
	// start the network
	//-----------------------------
	/* init wifi & network*/
	start_wifi();
	start_network();

	//-----------------------------------------------------
	//init softwares
	//-----------------------------------------------------

	clientInit();

	err = mdns_init(); // initialize mDNS service
	if (err)
		ESP_LOGE(TAG, "mDNS Init failed: %d", err);
	else
		ESP_LOGI(TAG, "mDNS Init ok");

	//set hostname and instance name
	if ((strlen(g_device->hostname) == 0) || (strlen(g_device->hostname) > HOSTLEN))
	{
		strcpy(g_device->hostname, "karadio32");
	}
	ESP_LOGI(TAG, "mDNS Hostname: %s", g_device->hostname);
	err = mdns_hostname_set(g_device->hostname);
	if (err)
		ESP_LOGE(TAG, "Hostname Init failed: %d", err);

	ESP_ERROR_CHECK(mdns_instance_name_set(g_device->hostname));
	ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
	ESP_ERROR_CHECK(mdns_service_add(NULL, "_telnet", "_tcp", 23, NULL, 0));

	// init player config
	player_config = (player_t *)calloc(1, sizeof(player_t));
	player_config->command = CMD_NONE;
	player_config->decoder_status = UNINITIALIZED;
	player_config->decoder_command = CMD_NONE;
	player_config->buffer_pref = BUF_PREF_SAFE;
	player_config->media_stream = calloc(1, sizeof(media_stream_t));

	audio_player_init(player_config);
	renderer_init(create_renderer_config());

	// LCD Display infos
	if (g_device->lcd_type != LCD_NONE)
		lcd_welcome(localIp, "STARTED");
	vTaskDelay(10);
	ESP_LOGI(TAG, "RAM left %d", esp_get_free_heap_size());

	//start tasks of KaRadio32
	xTaskCreatePinnedToCore(uartInterfaceTask, "uartInterfaceTask", 2400, NULL, PRIO_UART, &pxCreatedTask, CPU_UART);
	ESP_LOGI(TAG, "%s task: %x", "uartInterfaceTask", (unsigned int)pxCreatedTask);
	vTaskDelay(1);
	xTaskCreatePinnedToCore(clientTask, "clientTask", 3000, NULL, PRIO_CLIENT, &pxCreatedTask, CPU_CLIENT);
	ESP_LOGI(TAG, "%s task: %x", "clientTask", (unsigned int)pxCreatedTask);
	vTaskDelay(1);
	xTaskCreatePinnedToCore(serversTask, "serversTask", 3100, NULL, PRIO_SERVER, &pxCreatedTask, CPU_SERVER);
	ESP_LOGI(TAG, "%s task: %x", "serversTask", (unsigned int)pxCreatedTask);
	vTaskDelay(1);
	xTaskCreatePinnedToCore(task_addon, "task_addon", 2200, NULL, PRIO_ADDON, &pxCreatedTask, CPU_ADDON);
	ESP_LOGI(TAG, "%s task: %x", "task_addon", (unsigned int)pxCreatedTask);

	/*	if (RDA5807M_detection())
	{
		xTaskCreatePinnedToCore(rda5807Task, "rda5807Task", 2500, NULL, 3, &pxCreatedTask,1);  //
		ESP_LOGI(TAG, "%s task: %x","rda5807Task",(unsigned int)pxCreatedTask);
	}
*/
	vTaskDelay(60); // wait tasks init
	ESP_LOGI(TAG, " Init Done");

	setIvol(g_device->vol);
	kprintf("READY. Type help for a list of commands\n");

	//autostart
	autoPlay();
	// All done.
}
