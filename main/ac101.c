// code from https://github.com/donny681/esp-adf/blob/master/components/audio_hal/driver/AC101/AC101.c

#include <string.h>
#include <esp_log.h>
#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#include "audio_hal.h"
// #include "board.h"
#include "pin_conf.h"
#include "ac101.h"

#define AC101_TAG "AC101"
#define IIC_PORT I2C_NUM_1

static audio_board_handle_t board_handle = 0;

audio_board_handle_t a101_board_init(void)
{
	if (board_handle)
	{
		ESP_LOGW(AC101_TAG, "The board has already been initialized!");
		return board_handle;
	}
	board_handle = (audio_board_handle_t)audio_calloc(1, sizeof(struct audio_board_handle));
	AUDIO_MEM_CHECK(AC101_TAG, board_handle, return NULL);
	board_handle->audio_hal = a101_codec_init();
	return board_handle;
}

audio_hal_handle_t a101_codec_init(void)
{
	audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
	audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_AC101_CODEC_HANDLE);
	AUDIO_NULL_CHECK(AC101_TAG, codec_hal, return NULL);
	return codec_hal;
}

audio_board_handle_t a101_board_get_handle(void)
{
	return board_handle;
}

esp_err_t a101_board_deinit(audio_board_handle_t audio_board)
{
	esp_err_t ret = ESP_OK;
	ret |= audio_hal_deinit(audio_board->audio_hal);
	free(audio_board);
	board_handle = NULL;
	return ret;
}

/*
 * operate function of codec
 */
audio_hal_func_t AUDIO_CODEC_AC101_CODEC_HANDLE = {
	.audio_codec_initialize = AC101_init,
	.audio_codec_deinitialize = AC101_deinit,
	.audio_codec_ctrl = AC101_ctrl_state,
	.audio_codec_config_iface = AC101_config_i2s,
	.audio_codec_set_volume = AC101_set_voice_volume,
	.audio_codec_get_volume = AC101_get_voice_volume,
};

#define AC_ASSERT(a, format, b, ...)                \
	if ((a) != 0)                                   \
	{                                               \
		ESP_LOGE(AC101_TAG, format, ##__VA_ARGS__); \
		return b;                                   \
	}

static i2c_config_t es_i2c_cfg = {
	.mode = I2C_MODE_MASTER,
	.sda_pullup_en = GPIO_PULLUP_ENABLE,
	.scl_pullup_en = GPIO_PULLUP_ENABLE,
	.master.clk_speed = 100000};

static esp_err_t AC101_Write_Reg(uint8_t reg_addr, uint16_t data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	esp_err_t ret = 0;
	uint8_t send_buff[4];
	send_buff[0] = (AC101_ADDR << 1);
	send_buff[1] = reg_addr;
	send_buff[2] = (data >> 8) & 0xff;
	send_buff[3] = data & 0xff;
	ret |= i2c_master_start(cmd);
	ret |= i2c_master_write(cmd, send_buff, 4, ACK_CHECK_EN);
	ret |= i2c_master_stop(cmd);
	ret |= i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static esp_err_t i2c_example_master_read_slave(uint8_t DevAddr, uint8_t reg, uint8_t *data_rd, size_t size)
{
	if (size == 0)
	{
		return ESP_OK;
	}
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (DevAddr << 1) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (DevAddr << 1) | READ_BIT, ACK_CHECK_EN); // check or not
	i2c_master_read(cmd, data_rd, size, ACK_VAL);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static uint16_t AC101_read_Reg(uint8_t reg_addr)
{
	uint16_t val = 0;
	uint8_t data_rd[2];
	i2c_example_master_read_slave(AC101_ADDR, reg_addr, data_rd, 2);
	val = (data_rd[0] << 8) + data_rd[1];
	return val;
}

static int i2c_init()
{
	int res = 0;
	get_i2c_pins(I2C_NUM_0, &es_i2c_cfg);
	res |= i2c_param_config(I2C_NUM_0, &es_i2c_cfg);
	res |= i2c_driver_install(I2C_NUM_0, es_i2c_cfg.mode, 0, 0, 0);
	AC_ASSERT(res, "i2c_init error", -1);
	return res;
}

void set_codec_clk(audio_hal_iface_samples_t sampledata)
{
	uint16_t sample;
	switch (sampledata)
	{
	case AUDIO_HAL_08K_SAMPLES:
		sample = SAMPLE_RATE_8000;
		break;
	case AUDIO_HAL_11K_SAMPLES:
		sample = SAMPLE_RATE_11052;
		break;
	case AUDIO_HAL_16K_SAMPLES:
		sample = SAMPLE_RATE_16000;
		break;
	case AUDIO_HAL_22K_SAMPLES:
		sample = SAMPLE_RATE_22050;
		break;
	case AUDIO_HAL_24K_SAMPLES:
		sample = SAMPLE_RATE_24000;
		break;
	case AUDIO_HAL_32K_SAMPLES:
		sample = SAMPLE_RATE_32000;
		break;
	case AUDIO_HAL_44K_SAMPLES:
		sample = SAMPLE_RATE_44100;
		break;
	case AUDIO_HAL_48K_SAMPLES:
		sample = SAMPLE_RATE_48000;
		break;
	case AUDIO_HAL_96K_SAMPLES:
		sample = SAMPLE_RATE_96000;
		break;
	case AUDIO_HAL_192K_SAMPLES:
		sample = SAMPLE_RATE_192000;
		break;
	default:
		sample = SAMPLE_RATE_44100;
	}
	AC101_Write_Reg(I2S_SR_CTRL, sample);
}

esp_err_t AC101_init(audio_hal_codec_config_t *codec_cfg)
{

	if (i2c_init() < 0)
		return -1;

	esp_err_t res;
	res = AC101_Write_Reg(CHIP_AUDIO_RS, 0x123);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	if (ESP_OK != res)
	{
		ESP_LOGE(AC101_TAG, "reset failed!");
		return res;
	}
	else
	{
		ESP_LOGI(AC101_TAG, "reset succeed");
	}
	res |= AC101_Write_Reg(SPKOUT_CTRL, 0xe880);

	// Enable the PLL from 256*44.1KHz MCLK source
	res |= AC101_Write_Reg(PLL_CTRL1, 0x014f);
	// res |= AC101_Write_Reg(PLL_CTRL2, 0x83c0);
	res |= AC101_Write_Reg(PLL_CTRL2, 0x8600);

	// Clocking system
	res |= AC101_Write_Reg(SYSCLK_CTRL, 0x8b08);
	res |= AC101_Write_Reg(MOD_CLK_ENA, 0x800c);
	res |= AC101_Write_Reg(MOD_RST_CTRL, 0x800c);
	res |= AC101_Write_Reg(I2S_SR_CTRL, 0x7000); // sample rate
	// AIF config
	res |= AC101_Write_Reg(I2S1LCK_CTRL, 0x8850);	// BCLK/LRCK
	res |= AC101_Write_Reg(I2S1_SDOUT_CTRL, 0xc000); //
	res |= AC101_Write_Reg(I2S1_SDIN_CTRL, 0xc000);
	res |= AC101_Write_Reg(I2S1_MXR_SRC, 0x2200); //

	res |= AC101_Write_Reg(ADC_SRCBST_CTRL, 0xccc4);
	res |= AC101_Write_Reg(ADC_SRC, 0x2020);
	res |= AC101_Write_Reg(ADC_DIG_CTRL, 0x8000);
	res |= AC101_Write_Reg(ADC_APC_CTRL, 0xbbc3);

	// Path Configuration
	res |= AC101_Write_Reg(DAC_MXR_SRC, 0xcc00);
	res |= AC101_Write_Reg(DAC_DIG_CTRL, 0x8000);
	res |= AC101_Write_Reg(OMIXER_SR, 0x0081);
	res |= AC101_Write_Reg(OMIXER_DACA_CTRL, 0xf080); //}

	// Enable Speaker output
	res |= AC101_Write_Reg(0x58, 0xeabd);

	ESP_LOGI(AC101_TAG, "init done");
	AC101_pa_power(true);
	return res;
}

int AC101_get_spk_volume(void)
{
	int res;
	res = AC101_read_Reg(SPKOUT_CTRL);
	res &= 0x1f;
	return res * 2;
}

esp_err_t AC101_set_spk_volume(uint8_t volume)
{
	if (volume > 0x3f)
		volume = 0x3f;
	volume = volume / 2;

	uint16_t res;
	esp_err_t ret;

	res = AC101_read_Reg(SPKOUT_CTRL);
	res &= (~0x1f);
	volume &= 0x1f;
	res |= volume;
	ret = AC101_Write_Reg(SPKOUT_CTRL, res);
	return ret;
}

int AC101_get_earph_volume(void)
{
	int res;
	res = AC101_read_Reg(HPOUT_CTRL);
	return (res >> 4) & 0x3f;
}

esp_err_t AC101_set_earph_volume(uint8_t volume)
{
	if (volume > 0x3f)
		volume = 0x3f;

	uint16_t res, tmp;
	esp_err_t ret;
	res = AC101_read_Reg(HPOUT_CTRL);
	tmp = ~(0x3f << 4);
	res &= tmp;
	volume &= 0x3f;
	res |= (volume << 4);
	ret = AC101_Write_Reg(HPOUT_CTRL, res);
	return ret;
}

esp_err_t AC101_set_output_mixer_gain(ac_output_mixer_gain_t gain, ac_output_mixer_source_t source)
{
	uint16_t regval, temp, clrbit;
	esp_err_t ret;
	regval = AC101_read_Reg(OMIXER_BST1_CTRL);
	switch (source)
	{
	case SRC_MIC1:
		temp = (gain & 0x7) << 6;
		clrbit = ~(0x7 << 6);
		break;
	case SRC_MIC2:
		temp = (gain & 0x7) << 3;
		clrbit = ~(0x7 << 3);
		break;
	case SRC_LINEIN:
		temp = (gain & 0x7);
		clrbit = ~0x7;
		break;
	default:
		return -1;
	}
	regval &= clrbit;
	regval |= temp;
	ret = AC101_Write_Reg(OMIXER_BST1_CTRL, regval);
	return ret;
}

esp_err_t AC101_start(ac_module_t mode)
{

	esp_err_t res = 0;
	if (mode == AC_MODULE_LINE)
	{
		res |= AC101_Write_Reg(0x51, 0x0408);
		res |= AC101_Write_Reg(0x40, 0x8000);
		res |= AC101_Write_Reg(0x50, 0x3bc0);
	}
	if (mode == AC_MODULE_ADC || mode == AC_MODULE_ADC_DAC || mode == AC_MODULE_LINE)
	{
		// I2S1_SDOUT_CTRL
		// res |= AC101_Write_Reg(PLL_CTRL2, 0x8120);
		res |= AC101_Write_Reg(0x04, 0x800c);
		res |= AC101_Write_Reg(0x05, 0x800c);
		// res |= AC101_Write_Reg(0x06, 0x3000);
	}
	if (mode == AC_MODULE_DAC || mode == AC_MODULE_ADC_DAC || mode == AC_MODULE_LINE)
	{
		// Enable Headphoe output 
		res |= AC101_Write_Reg(OMIXER_DACA_CTRL, 0xff80);
		res |= AC101_Write_Reg(HPOUT_CTRL, 0xc3c1);
		res |= AC101_Write_Reg(HPOUT_CTRL, 0xcb00);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		res |= AC101_Write_Reg(HPOUT_CTRL, 0xfbc0);

		// Enable Speaker output
		res |= AC101_Write_Reg(SPKOUT_CTRL, 0xeabd);
		vTaskDelay(10 / portTICK_PERIOD_MS);
		AC101_set_voice_volume(30);
	}

	return res;
}

esp_err_t AC101_stop(ac_module_t mode)
{
	esp_err_t res = 0;
	res |= AC101_Write_Reg(HPOUT_CTRL, 0x01);	// disable earphone
	res |= AC101_Write_Reg(SPKOUT_CTRL, 0xe880); // disable speaker
	return res;
}

esp_err_t AC101_deinit(void)
{

	return AC101_Write_Reg(CHIP_AUDIO_RS, 0x123); // soft reset
}

esp_err_t AC101_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
	int res = 0;
	int es_mode_t = 0;

	switch (mode)
	{
	case AUDIO_HAL_CODEC_MODE_ENCODE:
		es_mode_t = AC_MODULE_ADC;
		break;
	case AUDIO_HAL_CODEC_MODE_LINE_IN:
		es_mode_t = AC_MODULE_LINE;
		break;
	case AUDIO_HAL_CODEC_MODE_DECODE:
		es_mode_t = AC_MODULE_DAC;
		break;
	case AUDIO_HAL_CODEC_MODE_BOTH:
		es_mode_t = AC_MODULE_ADC_DAC;
		break;
	default:
		es_mode_t = AC_MODULE_DAC;
		ESP_LOGW(AC101_TAG, "Codec mode not support, default is decode mode");
		break;
	}
	if (AUDIO_HAL_CTRL_STOP == ctrl_state)
	{
		res = AC101_stop(es_mode_t);
	}
	else
	{
		res = AC101_start(es_mode_t);
	}
	return res;
}

esp_err_t AC101_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
	esp_err_t res = 0;
	int bits = 0;
	int fmat = 0;
	int sample = 0;
	uint16_t regval;
	switch (iface->bits) //0x10
	{
	// case AUDIO_HAL_BIT_LENGTH_8BITS:
	// bits = BIT_LENGTH_8_BITS;
	// break;
	case AUDIO_HAL_BIT_LENGTH_16BITS:
		bits = BIT_LENGTH_16_BITS;
		break;
	case AUDIO_HAL_BIT_LENGTH_24BITS:
		bits = BIT_LENGTH_24_BITS;
		break;
	default:
		bits = BIT_LENGTH_16_BITS;
	}

	switch (iface->fmt) // 0x10
	{
	case AUDIO_HAL_I2S_NORMAL:
		fmat = 0x0;
		break;
	case AUDIO_HAL_I2S_LEFT:
		fmat = 0x01;
		break;
	case AUDIO_HAL_I2S_RIGHT:
		fmat = 0x02;
		break;
	case AUDIO_HAL_I2S_DSP:
		fmat = 0x03;
		break;
	default:
		fmat = 0x00;
		break;
	}

	switch (iface->samples)
	{
	case AUDIO_HAL_08K_SAMPLES:
		sample = SAMPLE_RATE_8000;
		break;
	case AUDIO_HAL_11K_SAMPLES:
		sample = SAMPLE_RATE_11052;
		break;
	case AUDIO_HAL_16K_SAMPLES:
		sample = SAMPLE_RATE_16000;
		break;
	case AUDIO_HAL_22K_SAMPLES:
		sample = SAMPLE_RATE_22050;
		break;
	case AUDIO_HAL_24K_SAMPLES:
		sample = SAMPLE_RATE_24000;
		break;
	case AUDIO_HAL_32K_SAMPLES:
		sample = SAMPLE_RATE_32000;
		break;
	case AUDIO_HAL_44K_SAMPLES:
		sample = SAMPLE_RATE_44100;
		break;
	case AUDIO_HAL_48K_SAMPLES:
		sample = SAMPLE_RATE_48000;
		break;
	case AUDIO_HAL_96K_SAMPLES:
		sample = SAMPLE_RATE_96000;
		break;
	case AUDIO_HAL_192K_SAMPLES:
		sample = SAMPLE_RATE_192000;
		break;
	default:
		sample = SAMPLE_RATE_44100;
	}
	regval = AC101_read_Reg(I2S1LCK_CTRL);
	regval &= 0xffc3;
	regval |= (iface->mode << 15);
	regval |= (bits << 4);
	regval |= (fmat << 2);
	res |= AC101_Write_Reg(I2S1LCK_CTRL, regval);
	res |= AC101_Write_Reg(I2S_SR_CTRL, sample);
	return res;
}

esp_err_t AC101_i2s_config_clock(ac_i2s_clock_t *cfg)
{
	esp_err_t res = 0;
	uint16_t regval = 0;
	regval = AC101_read_Reg(I2S1LCK_CTRL);
	regval &= 0xe03f;
	regval |= (cfg->bclk_div << 9);
	regval |= (cfg->lclk_div << 6);
	res = AC101_Write_Reg(I2S1LCK_CTRL, regval);
	return res;
}

esp_err_t AC101_set_voice_volume(int volume)
{
	esp_err_t res;
	res = AC101_set_earph_volume(volume);
	res |= AC101_set_spk_volume(volume);
	return res;
}

esp_err_t AC101_get_voice_volume(int *volume)
{
	*volume = AC101_get_earph_volume();
	return 0;
}

void AC101_pa_power(bool enable)
{
	gpio_config_t io_conf;
	memset(&io_conf, 0, sizeof(io_conf));
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = BIT(PA_ENABLE_GPIO);
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);
	if (enable)
	{
		gpio_set_level(PA_ENABLE_GPIO, 1);
	}
	else
	{
		gpio_set_level(PA_ENABLE_GPIO, 0);
	}
}
