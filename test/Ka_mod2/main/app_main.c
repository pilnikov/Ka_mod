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


#include "esp_ota_ops.h"
#include "esp_partition.h"


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_system.h"
#include "esp_log.h"


#include "app_main.h"
//#include "eeprom.h"


#include "spiram_fifo.h"

#include "audio_player.h"

/////////////////////////////////////////////////////
///////////////////////////
#include "nvs_flash.h"

#include "webclient.h"
#include "vs10xx.h"

/* The event group allows multiple bits for each event*/
//   are we connected  to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;
//
const int CONNECTED_AP = 0x00000010;

/////////////////////////////////////////////////////



/////////////////////////////////////////////////////


#define TAG "main"

//Priorities of the reader and the decoder thread. bigger number = higher prio
#define PRIO_READER configMAX_PRIORITIES - 3
#define PRIO_MQTT configMAX_PRIORITIES - 3
#define PRIO_CONNECT configMAX_PRIORITIES - 1
#define striWATERMARK "watermark: %d  heap: %d"

/* */
static bool wifiInitDone = false;
static EventGroupHandle_t wifi_event_group;
xQueueHandle event_queue;

//xSemaphoreHandle print_mux;
static uint16_t FlashOn = 5, FlashOff = 5;
player_t* player_config;
//ip
static char localIp[20];
// 4MB sram?
static bool bigRam = false;
// timeout to save volume in flash
static bool divide = false;

static esp_log_level_t s_log_default_level = CONFIG_LOG_BOOTLOADER_LEVEL_ERROR;


// disable 1MS timer interrupt
IRAM_ATTR void noInterrupt1Ms() { timer_disable_intr(TIMERGROUP1MS, msTimer); }
// enable 1MS timer interrupt
IRAM_ATTR void interrupt1Ms() { timer_enable_intr(TIMERGROUP1MS, msTimer); }
//IRAM_ATTR void noInterrupts() {noInterrupt1Ms();}
//IRAM_ATTR void interrupts() {interrupt1Ms();}

IRAM_ATTR char* getIp() { return (localIp); }


IRAM_ATTR bool bigSram() { return bigRam; }
//-----------------------------------
// every 500µs
IRAM_ATTR void msCallback(void* pArg)
{
	int timer_idx = (int)pArg;

	//	queue_event_t evt;
	TIMERG1.hw_timer[timer_idx].update = 1;
	TIMERG1.int_clr_timers.t0 = 1; //isr ack
	if (divide)
	{
	;
	}

	divide = !divide;
	TIMERG1.hw_timer[timer_idx].config.alarm_en = 1;
}

IRAM_ATTR void sleepCallback(void* pArg)
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

IRAM_ATTR void wakeCallback(void* pArg)
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

static xSemaphoreHandle muxDevice;

const esp_partition_t* DEVICE;
const esp_partition_t* DEVICE1;
const esp_partition_t* STATIONS;

void partitions_init(void)
{
	DEVICE = esp_partition_find_first(64, 0, NULL);
	if (DEVICE == NULL)
		ESP_LOGE(TAG, "DEVICE Partition not found");
	DEVICE1 = esp_partition_find_first(66, 0, NULL);
	if (DEVICE1 == NULL)
		ESP_LOGE(TAG, "DEVICE1 Partition not found");
	STATIONS = esp_partition_find_first(65, 0, NULL);
	if (STATIONS == NULL)
		ESP_LOGE(TAG, "STATIONS Partition not found");
	muxDevice = xSemaphoreCreateMutex();

	g_device.current_ap = 1; // 0 = AP mode, else STA mode: 1 = ssid1, 2 = ssid2
	g_device.treble = 50;
	g_device.bass = 50;
	g_device.freqtreble = 50;
	g_device.freqbass = 50;
	g_device.spacial = 0;
	g_device.currentstation = 0;  // 
	g_device.autostart = 1; // 0: stopped, 1: playing
	strcpy(g_device.ssid1, "Home");
	strcpy(g_device.pass1, "44332221111");
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

void stopSleep()
{
	ESP_LOGD(TAG, "stopDelayDelay\n");
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP, sleepTimer));
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
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP, sleepTimer, sleepCallback, (void*)sleepTimer, 0, NULL));
	/*Configure timer wake*/
	ESP_ERROR_CHECK(timer_init(TIMERGROUP, wakeTimer, &config));
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP, wakeTimer));
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP, wakeTimer, wakeCallback, (void*)wakeTimer, 0, NULL));
	/*Configure timer 1MS*/
	config.auto_reload = TIMER_AUTORELOAD_EN;
	config.divider = TIMER_DIVIDER1MS;
	ESP_ERROR_CHECK(timer_init(TIMERGROUP1MS, msTimer, &config));
	ESP_ERROR_CHECK(timer_pause(TIMERGROUP1MS, msTimer));
	ESP_ERROR_CHECK(timer_isr_register(TIMERGROUP1MS, msTimer, msCallback, (void*)msTimer, 0, NULL));
	/* start 1MS timer*/
	ESP_ERROR_CHECK(timer_set_counter_value(TIMERGROUP1MS, msTimer, 0x00000000ULL));
	ESP_ERROR_CHECK(timer_set_alarm_value(TIMERGROUP1MS, msTimer, TIMERVALUE1MS(1)));
	ESP_ERROR_CHECK(timer_enable_intr(TIMERGROUP1MS, msTimer));
	ESP_ERROR_CHECK(timer_set_alarm(TIMERGROUP1MS, msTimer, TIMER_ALARM_EN));
	ESP_ERROR_CHECK(timer_start(TIMERGROUP1MS, msTimer));

}

//////////////////////////////////////////////////////////////////


/******************************************************************************
 * FunctionName : checkUart
 * Description  : Check for a valid uart baudrate
 * Parameters   : baud
 * Returns      : baud
*******************************************************************************/
uint32_t checkUart(uint32_t speed)
{
	uint32_t valid[] = { 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 76880, 115200, 230400 };
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
		vsStart();

	ESP_LOGE(TAG, "VS HW initialized");
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
		if (wifiInitDone) // a completed init done
			{
				ESP_LOGE(TAG, "Connection tried again");
				//clientDisconnect("Wifi Disconnected.");
				clientSilentDisconnect();
				vTaskDelay(100);
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
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
	{
		xEventGroupSetBits(wifi_event_group, CONNECTED_AP);
		ESP_LOGI(TAG, "Wifi connected");
		if (wifiInitDone)
		{
			printf("Wifi Connected.");
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

	if (g_device.current_ap == APMODE)
	{
		if (strlen(g_device.ssid1) != 0)
		{
			g_device.current_ap = STA1;
		}
		else
		{
			if (strlen(g_device.ssid2) != 0)
			{
				g_device.current_ap = STA2;
			}
			else
				g_device.current_ap = APMODE;
		}
	}

	while (1)
	{
		if (first_pass)
		{
			ESP_ERROR_CHECK(esp_wifi_stop());
			vTaskDelay(5);
		}
		switch (g_device.current_ap)
		{
		case STA1: //ssid1 used
			strcpy(ssid, g_device.ssid1);
			strcpy(pass, g_device.pass1);
			esp_wifi_set_mode(WIFI_MODE_STA);
			break;
		case STA2: //ssid2 used
			strcpy(ssid, g_device.ssid2);
			strcpy(pass, g_device.pass2);
			esp_wifi_set_mode(WIFI_MODE_STA);
			break;

		default: // other: AP mode
			g_device.current_ap = APMODE;
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
		}

		if (g_device.current_ap == APMODE)
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
			printf("WIFI TRYING TO CONNECT TO SSID %d\n", g_device.current_ap);
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
				g_device.current_ap++;
				g_device.current_ap %= 3;

				printf("Empty AP. Try next one\n");

				continue;
			}
		}

		/* Wait for the callback to set the CONNECTED_AP in the event group. */
		if ((xEventGroupWaitBits(wifi_event_group, CONNECTED_AP, false, true, 2000) & CONNECTED_AP) == 0)
		//timeout . Try the next AP
		{
			g_device.current_ap++;
			g_device.current_ap %= 3;
			printf("\ndevice->current_ap: %d\n", g_device.current_ap);
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

	IP4_ADDR(&ipAddr, 192, 168, 4, 1);
	IP4_ADDR(&gate, 192, 168, 4, 1);
	IP4_ADDR(&mask, 255, 255, 255, 0);


	switch (g_device.current_ap)
	{
	case STA1: //ssid1 used
		IP4_ADDR(&ipAddr, g_device.ipAddr1[0], g_device.ipAddr1[1], g_device.ipAddr1[2], g_device.ipAddr1[3]);
		IP4_ADDR(&gate, g_device.gate1[0], g_device.gate1[1], g_device.gate1[2], g_device.gate1[3]);
		IP4_ADDR(&mask, g_device.mask1[0], g_device.mask1[1], g_device.mask1[2], g_device.mask1[3]);
		break;
	case STA2: //ssid2 used
		IP4_ADDR(&ipAddr, g_device.ipAddr2[0], g_device.ipAddr2[1], g_device.ipAddr2[2], g_device.ipAddr2[3]);
		IP4_ADDR(&gate, g_device.gate2[0], g_device.gate2[1], g_device.gate2[2], g_device.gate2[3]);
		IP4_ADDR(&mask, g_device.mask2[0], g_device.mask2[1], g_device.mask2[2], g_device.mask2[3]);
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
		esp_netif_dhcpc_start(sta); //  run a DHCP client
	
		// wait for ip
		if ((xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 3000) & CONNECTED_BIT) == 0) //timeout
		{																									// enable dhcp and restart
			if (g_device.current_ap == 1)
			{
				g_device.dhcpEn1 = 1;
			}
			else
			{
				g_device.dhcpEn2 = 1;
			}
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

		if (true) // if dhcp enabled update fields
		{
			esp_netif_get_ip_info(sta, &info);
			switch (g_device.current_ap)
			{
			case STA1: //ssid1 used
				ip4addr_aton((const char *)&g_device.ipAddr1,(ip4_addr_t *)&info.ip);
				ip4addr_aton((const char *)&g_device.gate1,(ip4_addr_t *)&info.gw);
				ip4addr_aton((const char *)&g_device.mask1,(ip4_addr_t *)&info.netmask);
				break;

			case STA2: //ssid2 used
				ip4addr_aton((const char *)&g_device.ipAddr2,(ip4_addr_t *)&info.ip);
				ip4addr_aton((const char *)&g_device.gate2,(ip4_addr_t *)&info.gw);
				ip4addr_aton((const char *)&g_device.mask2,(ip4_addr_t *)&info.netmask);
				break;
			}
		}
		esp_netif_set_hostname(sta, "karadio32");
	}
	ip4_addr_copy(ipAddr, info.ip);
	strcpy(localIp, ip4addr_ntoa(&ipAddr));
	printf("IP: %s\n\n", localIp);

}

//blinking led and timer isr
void timerTask(void* p)
{
	initTimers();

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

		vTaskDelay(10);
	}

	vTaskDelete(NULL); // stop the task (never reached)
}


// Start playing.
void autoPlay()
{
	ESP_LOGE(TAG,"autostart: playing:%d, currentstation:%d\n", g_device.autostart, g_device.currentstation);
	vTaskDelay(50); // wait a bit
	playStationInt(g_device.currentstation);
}


/********************************************************************
 ***********************   Main entry point     *********************
 ********************************************************************/
void app_main()
{
	xTaskHandle pxCreatedTask;
	esp_err_t err;

	ESP_LOGE(TAG, "starting app_main()");
	ESP_LOGE(TAG, "RAM left: %u", esp_get_free_heap_size());

	const esp_partition_t* running = esp_ota_get_running_partition();
	ESP_LOGE(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
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
	ESP_LOGE(TAG, "Partition init done...");

	// init softwares


	void setLogLevel(esp_log_level_t level)
	{
		esp_log_level_set("*", level);
		s_log_default_level = level;
		g_device.trace_level = level;
	}

	// log level
	//setLogLevel(g_device.trace_level);
	setLogLevel(ESP_LOG_ERROR);

	//time display

	//SPI init for the vs1053 and lcd if spi.


	Spi_init();
	init_vs_hw(); //init vs1053 if in mode sel

	//Initialize the SPI RAM chip communications and see if it actually retains some bytes. If it
	//doesn't, warn user.
	if (bigRam)
	{
		setSPIRAMSIZE(420 * 1024); // room in psram
		ESP_LOGI(TAG, "Set Song buffer to 420k");
	}
	else
	{
		setSPIRAMSIZE(50 * 1024); // more free heap
		ESP_LOGI(TAG, "Set Song buffer to 50k");
	}

	if (!spiRamFifoInit())
	{
		ESP_LOGE(TAG, "SPI RAM chip fail!");
		esp_restart();
	}

	// Version infos
	ESP_LOGE(TAG, "Release %s, Revision %s", RELEASE, REVISION);
	ESP_LOGE(TAG, "SDK %s", esp_get_idf_version());
	ESP_LOGE(TAG, "Heap size: %d", xPortGetFreeHeapSize());

	// queue for events of the sleep / wake and Ms timers
	event_queue = xQueueCreate(30, sizeof(queue_event_t));
	// led blinks
	xTaskCreatePinnedToCore(timerTask, "timerTask", 2100, NULL, PRIO_TIMER, &pxCreatedTask, CPU_TIMER);
	ESP_LOGE(TAG, "%s task: %x", "t0", (unsigned int)pxCreatedTask);

	//-----------------------------
	// start the network
	//-----------------------------
	/* init wifi & network*/
	start_wifi();
	ESP_LOGE(TAG, "Wifi Started!!! True start Network");
	start_network();

	//-----------------------------------------------------
	//init softwares
	//-----------------------------------------------------

	ESP_LOGE(TAG, "Network Started!!! True start client");
	clientInit();

	// init player config
	player_config = (player_t*)calloc(1, sizeof(player_t));
	player_config->command = CMD_START;
	player_config->buffer_pref = BUF_PREF_SAFE;
	player_config->media_stream = calloc(1, sizeof(media_stream_t));

	audio_player_init(player_config);

	ESP_LOGE(TAG, "RAM left %d", esp_get_free_heap_size());

	//start tasks of KaRadio32
	xTaskCreatePinnedToCore(clientTask, "clientTask", 3000, NULL, PRIO_CLIENT, &pxCreatedTask, CPU_CLIENT);
	ESP_LOGE(TAG, "%s task: %x", "clientTask", (unsigned int)pxCreatedTask);

	vTaskDelay(60); // wait tasks init
	ESP_LOGE(TAG, "Init Done Start AutoPlay\n");

	//autostart
	autoPlay();
	// All done.
}
