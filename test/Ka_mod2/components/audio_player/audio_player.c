/*
 * audio_player.c
 *
 *  Created on: 12.03.2017
 *      Author: michaelboeckling
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "audio_player.h"
#include "spiram_fifo.h"
#include "freertos/task.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_system.h"
#include "esp_log.h"

#include "webclient.h"
#include "vs10xx.h"
#include "app_main.h"

#define TAG "audio_player"
 //#define PRIO_MAD configMAX_PRIORITIES - 4


static player_t* player_instance = NULL;
static component_status_t player_status = UNINITIALIZED;



static int t;

/* Writes bytes into the FIFO queue, starts decoder task if necessary. */
int audio_stream_consumer(const char* recv_buf, ssize_t bytes_read,
	void* user_data)
{
	player_t* player = user_data;
	// don't bother consuming bytes if stopped
	if (player->command == CMD_STOP) {
		clientSilentDisconnect();
		return -1;
	}

	if (bytes_read > 0) {
		spiRamFifoWrite(recv_buf, bytes_read);
	}


	int bytes_in_buf = spiRamFifoFill();
	uint8_t fill_level = (bytes_in_buf * 100) / spiRamFifoLen();

	//  seems 4k is enough to prevent initial buffer underflow
	//	uint8_t min_fill_lvl = player->buffer_pref == BUF_PREF_FAST ? 40 : 90;
	//	bool buffer_ok = fill_level > min_fill_lvl;

	t = (t + 1) & 255;
	if (t == 0) {
		ESP_LOGI(TAG, "Buffer fill %u%%, %d // %d bytes", fill_level, bytes_in_buf, spiRamFifoLen());
	}

	return 0;
}

void audio_player_init(player_t* player)
{
	player_instance = player;
	player_status = INITIALIZED;
}

void audio_player_destroy()
{
	player_status = UNINITIALIZED;
}

void audio_player_start()
{
	player_instance->media_stream->eof = false;
	player_instance->command = CMD_START;
	player_status = RUNNING;

	ESP_LOGI(TAG, "RAM left %d", esp_get_free_heap_size());
	TaskFunction_t task_func = vsTask;
	char* task_name = (char*)"vsTask";
	uint16_t stack_depth = 3000;
	int priority = PRIO_VS1053;
	xTaskCreatePinnedToCore(task_func, task_name, stack_depth, NULL, priority, NULL, 1); ///////////////////////////
	spiRamFifoReset();
	ESP_LOGI(TAG, "decoder task created: %s", task_name);
}

void audio_player_stop()
{
	player_instance->command = CMD_STOP;
	player_instance->media_stream->eof = true;
	player_instance->command = CMD_NONE;
	player_status = STOPPED;

}

component_status_t get_player_status()
{
	return player_status;
}

