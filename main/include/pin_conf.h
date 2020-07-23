#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

#define PA_ENABLE_GPIO            GPIO_NUM_21 ///////////!!!!!!!!!!!!!/////////////
#define HEADPHONE_DETECT          GPIO_NUM_5  ////////////////////////

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config);
esp_err_t get_i2s_pins(i2s_port_t port, i2s_pin_config_t *i2s_config);
int8_t get_headphone_detect_gpio(void);
int8_t get_pa_enable_gpio(void);


