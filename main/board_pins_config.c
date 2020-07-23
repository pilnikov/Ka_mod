/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_log.h"

#include <string.h>
#include "audio_error.h"
#include "pin_conf.h"
#include "ac101.h"
#include "driver/gpio.h"
#include "gpio.h"

static const char *TAG = "A1S";

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    gpio_num_t scl;
    gpio_num_t sda;
    gpio_num_t rsti2c;

    gpio_get_i2c(&scl, &sda, &rsti2c);

    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    if (port == I2C_NUM_0)
    {
        i2c_config->sda_io_num = sda;
        i2c_config->scl_io_num = scl;
        ESP_LOGI(TAG, "i2c port configured!!!!");
    }
    else
    {
        i2c_config->sda_io_num = GPIO_NONE;
        i2c_config->scl_io_num = GPIO_NONE;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_i2s_pins(i2s_port_t port, i2s_pin_config_t *i2s_config)
{
    gpio_num_t bclk;
    gpio_num_t lrck;
    gpio_num_t i2sdata;

    gpio_get_i2s(&lrck, &bclk, &i2sdata);

    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == I2S_NUM_0)
    {
        i2s_config->bck_io_num = bclk;
        i2s_config->ws_io_num = lrck;
        i2s_config->data_out_num = i2sdata;
        i2s_config->data_in_num = I2S_PIN_NO_CHANGE; //GPIO_NUM_35
        ESP_LOGI(TAG, "i2s port configured!!!!");
    }
    else
    {
        memset(i2s_config, GPIO_NONE, sizeof(i2s_pin_config_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// input-output pins

int8_t get_headphone_detect_gpio(void)
{
    return HEADPHONE_DETECT;
}

int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO;
}
