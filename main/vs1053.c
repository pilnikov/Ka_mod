/**
***********************************************************************************************************************
  * @file    VS1053.c
  * @author  Piotr Sperka
  * @date    07.08.2015
  * @brief   This file provides VS1053 usage and control functions. Based on VS1003 library by Przemyslaw Stasiak.
  * Copyright 2017 karawin (http://www.karawin.fr) for KaRadio32
  * added control treble, bass and spacialisation
  ***********************************************************************************************************************
*/

/** @addtogroup VS10xx
  * @{
  */

#include "vs10xx.h"
#include "vs10xx-patches-flac.plg"


#define CHUNK 32


#define TAG "VS10xx"
#define SDI_END_FILL_BYTES_FLAC 12288
#define SDI_END_FILL_BYTES       2050
#define PAR_BITRATE_PER_100    0x1e05 /* VS1063 */
#define PAR_VU_METER           0x1e0c /* VS1063 */

int vsVersion = -1; // the version of the chip
//	SS_VER is 0 for VS1001, 1 for VS1011, 2 for VS1002, 3 for VS1003, 4 for VS1053 and VS8053, 5 for VS1033, 7 for VS1103, and 6 for VS1063.

gpio_num_t rst;
gpio_num_t dreq;

static spi_device_handle_t vsspi;  // the evice handle of the vs1053 spi
static spi_device_handle_t hvsspi; // the device handle of the vs1053 spi high speed

xSemaphoreHandle vsSPI = NULL;
xSemaphoreHandle hsSPI = NULL;

uint8_t spi_take_semaphore(xSemaphoreHandle isSPI)
{
	if (isSPI)
		if (xSemaphoreTake(isSPI, portMAX_DELAY))
			return 1;
	return 0;
}

void spi_give_semaphore(xSemaphoreHandle isSPI)
{
	if (isSPI)
		xSemaphoreGive(isSPI);
}

void Spi_init()
{
	esp_err_t ret;
	gpio_num_t miso;
	gpio_num_t mosi;
	gpio_num_t sclk;

	uint8_t spi_no; // the spi bus to use
	if (!vsSPI)
		vsSPI = xSemaphoreCreateMutex();
	if (!hsSPI)
		hsSPI = xSemaphoreCreateMutex();
	;

	gpio_get_spi_bus(&spi_no, &miso, &mosi, &sclk);
	if (miso == GPIO_NONE || mosi == GPIO_NONE || sclk == GPIO_NONE || spi_no > 2)
	{
		ESP_LOGE("SPI", "SPI pin not configured");
		return; //Only VSPI and HSPI are valid spi modules.
	}

	spi_bus_config_t buscfg = {
		.miso_io_num = miso,
		.mosi_io_num = mosi,
		.sclk_io_num = sclk,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.flags = SPICOMMON_BUSFLAG_MASTER
	};
	ret = spi_bus_initialize(spi_no, &buscfg, 1); // dma
	assert(ret == ESP_OK);
}

int get_vsVersion() { return vsVersion; }

int vsHW_init()
{

	int ret = -1;

	gpio_num_t miso;
	gpio_num_t mosi;
	gpio_num_t sclk;
	gpio_num_t xcs;
	gpio_num_t xdcs;

	uint8_t spi_no; // the spi bus to use

	gpio_get_spi_bus(&spi_no, &miso, &mosi, &sclk);
	gpio_get_vs1053(&xcs, &rst, &xdcs, &dreq);

	// if xcs = 0 the vs10xx is not used
	if (xcs == GPIO_NONE)
	{
		vsVersion = 0;
		ESP_LOGE(TAG, "VS10xx not used");
	}
	else
	{
	/*
		uint32_t freq = spi_get_actual_clock(APB_CLK_FREQ, 1400000, 128);
		ESP_LOGI(TAG, "VS10xx LFreq: %d", freq);
		spi_device_interface_config_t devcfg = {
			.clock_speed_hz = freq, //Clock out at x MHz
			.command_bits = 8,
			.address_bits = 8,
			.dummy_bits = 0,
			.duty_cycle_pos = 0,
			.cs_ena_pretrans = 0,
			.cs_ena_posttrans = 1,
			.flags = 0,
			.mode = 0,			 //SPI mode
			.spics_io_num = xcs, //XCS pin
			.queue_size = 1,	 //We want to be able to queue x transactions at a time
			.pre_cb = NULL, //Specify pre-transfer callback to handle D/C line
			.post_cb = NULL };

		//slow speed
		ESP_ERROR_CHECK(spi_bus_add_device(spi_no, &devcfg, &vsspi));
		
		int vsStatus = vsReadSci(SCI_STATUSVS);
		vsVersion = (vsStatus >> 4) & 0x000F; //Mask out only the four version bits
		//0 for VS1001, 1 for VS1011, 2 for VS1002, 3 for VS1003, 4 for VS1053 and VS8053,
		//5 for VS1033, 7 for VS1103, and 6 for VS1063

		vsregtest();

		//high speed
		freq = spi_get_actual_clock(APB_CLK_FREQ, 6100000, 128);
		ESP_LOGI(TAG, "VS10xx HFreq: %d", freq);
		devcfg.clock_speed_hz = freq;
		devcfg.spics_io_num = xdcs; //XDCS pin
		devcfg.command_bits = 0;
		devcfg.address_bits = 0;
		ESP_ERROR_CHECK(spi_bus_add_device(spi_no, &devcfg, &hvsspi));
*/
		/*---- Initialize non-SPI GPIOs ----*/
/*		gpio_config_t gpio_conf;
		gpio_conf.mode = GPIO_MODE_OUTPUT;
		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
		gpio_conf.intr_type = GPIO_INTR_DISABLE;
		gpio_conf.pin_bit_mask = ((uint64_t)(((uint64_t)1) << rst)); //XRST pin
		ESP_ERROR_CHECK(gpio_config(&gpio_conf));

		ControlReset(RESET); //Set xrst pin level to HIGH

		gpio_conf.mode = GPIO_MODE_INPUT;
		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
		gpio_conf.intr_type = GPIO_INTR_DISABLE;
		gpio_conf.pin_bit_mask = ((uint64_t)(((uint64_t)1) << dreq)); //DREQ pin
		ESP_ERROR_CHECK(gpio_config(&gpio_conf));

		ret = vsVersion;
	}
*/
 	/*---- Initialize non-SPI GPIOs ----*/
		gpio_config_t gpio_conf;
		gpio_conf.mode = GPIO_MODE_OUTPUT;
		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
		gpio_conf.intr_type = GPIO_INTR_DISABLE;
		gpio_conf.pin_bit_mask = ((uint64_t)(((uint64_t)1) << rst)); //XRST pin
		ESP_ERROR_CHECK(gpio_config(&gpio_conf));

		ControlReset(RESET); //Set xrst pin level to HIGH

		gpio_conf.mode = GPIO_MODE_INPUT;
		gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
		gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
		gpio_conf.intr_type = GPIO_INTR_DISABLE;
		gpio_conf.pin_bit_mask = ((uint64_t)(((uint64_t)1) << dreq)); //DREQ pin
		ESP_ERROR_CHECK(gpio_config(&gpio_conf));

	//slow speed
		uint32_t freq = 200000;
		ESP_LOGI(TAG, "VS10xx LFreq: %d", freq);
		spi_device_interface_config_t devcfg = {
			.clock_speed_hz = freq, //Clock out at x MHz
			.command_bits = 8,
			.address_bits = 8,
			.dummy_bits = 0,
			.duty_cycle_pos = 0,
			.cs_ena_pretrans = 0,
			.cs_ena_posttrans = 1,
			.flags = 0,
			.mode = 0,			 //SPI mode
			.spics_io_num = xcs, //XCS pin
			.queue_size = 1,	 //We want to be able to queue x transactions at a time
			.pre_cb = NULL, //Specify pre-transfer callback to handle D/C line
			.post_cb = NULL };

		ESP_ERROR_CHECK(spi_bus_add_device(spi_no, &devcfg, &vsspi));
	
		vTaskDelay(20);

		int vsStatus = vsReadSci(SCI_STATUSVS);
		vsVersion = (vsStatus >> 4) & 0x000F; //Mask out only the four version bits

		//high speed
		freq = spi_get_actual_clock(APB_CLK_FREQ, 6100000, 128);
		ESP_LOGI(TAG, "VS10xx HFreq: %d", freq);
		devcfg.clock_speed_hz = freq;
		devcfg.spics_io_num = xdcs; //XDCS pin
		devcfg.command_bits = 0;
		devcfg.address_bits = 0;
		
		ESP_ERROR_CHECK(spi_bus_add_device(spi_no, &devcfg, &hvsspi));

		vsregtest();
		ret = vsVersion;
	}
	return ret;		/* vs version */
}

void ControlReset(uint8_t State)
{
	gpio_set_level(rst, State);
}

uint8_t vscheckDREQ()
{
	return gpio_get_level(dreq);
}

void vsLoadPlugin(const uint16_t* d, uint16_t len)
{
	int i = 0;

	while (i < len) {
		unsigned short addr, n, val;
		addr = d[i++];
		n = d[i++];
		if (n & 0x8000U) { /* RLE run, replicate n samples */
			n &= 0x7FFF;
			val = d[i++];
			while (n--) {
				vsWriteSci(addr, val);
			}
		}
		else {           /* Copy run, copy n samples */
			while (n--) {
				val = d[i++];
				vsWriteSci(addr, val);
			}
		}
	}
}


/*-- System functons for VS10xx --*/
void vsWriteScichar(spi_device_handle_t ivsspi, uint8_t* cbyte, uint16_t len)
{
	esp_err_t ret;
	spi_transaction_t t;

	memset(&t, 0, sizeof(t)); //Zero out the transaction
	t.tx_buffer = cbyte;
	t.length = len * 8;

    uint8_t suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
            taskYIELD();
            suc++;
        }

	spi_take_semaphore(hsSPI);
	ret = spi_device_transmit(ivsspi, &t); //Transmit!
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "err: %d, vsspi_write_char(len: %d)", ret, len);
	spi_give_semaphore(hsSPI);

    suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
            taskYIELD();
            suc++;
        }
}

void vsWriteSci8(uint8_t addr, uint8_t highbyte, uint8_t lowbyte)
{
	spi_transaction_t t;
	esp_err_t ret;

	memset(&t, 0, sizeof(t)); //Zero out the transaction
	t.flags |= SPI_TRANS_USE_TXDATA;
	t.cmd = VS_WRITE_COMMAND;
	t.addr = addr;
	t.tx_data[0] = highbyte;
	t.tx_data[1] = lowbyte;
	t.length = 16;
    
	uint8_t suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
            taskYIELD();
            suc++;
        }

	spi_take_semaphore(vsSPI);
	ret = spi_device_transmit(vsspi, &t); //Transmit!
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "err: %d, vsWriteSci8(%d,%d,%d)", ret, addr, highbyte, lowbyte);
	spi_give_semaphore(vsSPI);

    suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
	{
        taskYIELD();
        suc++;
    }
}

void vsWriteSci(uint8_t addr, uint16_t value)
{
	spi_transaction_t t;
	esp_err_t ret;

	memset(&t, 0, sizeof(t)); //Zero out the transaction
	t.flags |= SPI_TRANS_USE_TXDATA;
	t.cmd = VS_WRITE_COMMAND;
	t.addr = addr;
	t.tx_data[0] = (value >> 8) & 0xff;
	t.tx_data[1] = value & 0xff;
	t.length = 16;

    uint8_t suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
            taskYIELD();
            suc++;
        }

	spi_take_semaphore(vsSPI);
	ret = spi_device_transmit(vsspi, &t); //Transmit!
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "err: %d, vsWriteSci(%d,%d)", ret, addr, value);
	spi_give_semaphore(vsSPI);

	suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
			taskYIELD();
			suc++;
		}	
}

uint16_t vsReadSci(uint8_t addressbyte)
{
	uint16_t result;
	spi_transaction_t t;
	esp_err_t ret;

	memset(&t, 0, sizeof(t)); //Zero out the transaction
	t.length = 16;
	t.flags |= SPI_TRANS_USE_RXDATA;
	t.cmd = VS_READ_COMMAND;
	t.addr = addressbyte;

    uint8_t suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
            taskYIELD();
            suc++;
        }

	spi_take_semaphore(vsSPI);
	ret = spi_device_transmit(vsspi, &t); //Transmit!
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "err: %d, vsReadSci(%d), read: %d", ret, addressbyte, (uint32_t)*t.rx_data);
	result = (((t.rx_data[0] & 0xFF) << 8) | ((t.rx_data[1]) & 0xFF));
	spi_give_semaphore(vsSPI);

    suc = 1;
	while (vscheckDREQ() == 0 && suc < 10)
		{
            taskYIELD();
            suc++;
        }

	return result;
}

void vsResetChip()
{
	ControlReset(SET);
	vTaskDelay(30);
	ControlReset(RESET);

	vsDisableAnalog();
}

uint16_t MaskAndShiftRight(uint16_t Source, uint16_t Mask, uint16_t Shift)
{
	return ((Source & Mask) >> Shift);
}

/*
  Read 16-bit value from addr.
*/
uint16_t ReadVS10xxMem(uint16_t addr) {
	vsWriteSci(SCI_WRAMADDR, addr);
	return vsReadSci(SCI_WRAM);
}

enum AudioFormat {
	afUnknown,
	afRiff,
	afOggVorbis,
	afMp1,
	afMp2,
	afMp3,
	afAacMp4,
	afAacAdts,
	afAacAdif,
	afFlac,
	afWma,
} audioFormat = afUnknown;

const char* afName[] = {
  "unknown",
  "RIFF",
  "Ogg",
  "MP1",
  "MP2",
  "MP3",
  "AAC MP4",
  "AAC ADTS",
  "AAC ADIF",
  "FLAC",
  "WMA",
};

void vsInfo()
{
	uint16_t sampleRate;
	uint16_t hehtoBitsPerSec;
	uint16_t h1 = vsReadSci(SCI_HDAT1);

	if (h1 == 0x7665) {
		audioFormat = afRiff;
	}
	else if (h1 == 0x4154) {
		audioFormat = afAacAdts;
	}
	else if (h1 == 0x4144) {
		audioFormat = afAacAdif;
	}
	else if (h1 == 0x574d) {
		audioFormat = afWma;
	}
	else if (h1 == 0x4f67) {
		audioFormat = afOggVorbis;
	}
	else if (h1 == 0x664c) {
		audioFormat = afFlac;
	}
	else if (h1 == 0x4d34) {
		audioFormat = afAacMp4;
	}
	else if ((h1 & 0xFFE6) == 0xFFE2) {
		audioFormat = afMp3;
	}
	else if ((h1 & 0xFFE6) == 0xFFE4) {
		audioFormat = afMp2;
	}
	else if ((h1 & 0xFFE6) == 0xFFE6) {
		audioFormat = afMp1;
	}
	else {
		audioFormat = afUnknown;
	}

	sampleRate = vsReadSci(SCI_AUDATA);
	hehtoBitsPerSec = ReadVS10xxMem(PAR_BITRATE_PER_100);

	ESP_LOGI(TAG, "\r%1ds %1.1f kb/s %dHz %s %s h = 0x%X",
		vsReadSci(SCI_DECODE_TIME),
		hehtoBitsPerSec * 0.1,
		sampleRate & 0xFFFE, (sampleRate & 1) ? "stereo" : "mono",
		afName[audioFormat], h1
	);

} /* REPORT_ON_SCREEN */

void vsregtest()
{
	int vsStatus = vsReadSci(SCI_STATUSVS);
	int vsMode = vsReadSci(SCI_MODE);
	int vsClock = vsReadSci(SCI_CLOCKF);
	ESP_LOGI(TAG, "SCI_Status = 0x%X", vsStatus);
	ESP_LOGI(TAG, "SCI_Mode (0x4800) = 0x%X", vsMode);
	ESP_LOGI(TAG, "SCI_ClockF = 0x%X", vsClock);
	ESP_LOGI(TAG, "VS10xx Version = %d", vsVersion);
	//The 1053B should respond with 4. VS1001 = 0, VS1011 = 1, VS1002 = 2, VS1003 = 3, VS1054 = 4 VS1063 = 6
}

void vsI2SRate(uint8_t speed)
{ // 0 = 48kHz, 1 = 96kHz, 2 = 128kHz
	if (speed > 2)
		speed = 0;
	if (vsVersion < 3)
		return;
	vsWriteSci(SCI_WRAMADDR, 0xc040);	 //address of GPIO_ODATA is 0xC017
	vsWriteSci(SCI_WRAM, 0x0008 | speed); //
	vsWriteSci(SCI_WRAMADDR, 0xc040);	 //address of GPIO_ODATA is 0xC017
	vsWriteSci(SCI_WRAM, 0x000C | speed); //
	ESP_LOGI(TAG, "I2S Speed: %d", speed);
}
void vsDisableAnalog()
{
	// disable analog output
	vsWriteSci(SCI_VOL, 0xFFFF);
}

// reduce the chip consumption
void vsLowPower()
{
	vsWriteSci(SCI_CLOCKF, 0x0000); //
}

// normal chip consumption
void vsHighPower()
{
	if (vsVersion == 4)								// only 1053
		vsWriteSci(SCI_CLOCKF, 0xB800); // SC_MULT = x1, SC_ADD= x1
	else
		vsWriteSci(SCI_CLOCKF, 0xb000);
}

/*--- Start Vs ----*/
void vsStart()
{
	vsResetChip();

	if (vsVersion == 0)
		ESP_LOGE(TAG, "NO VS10xx detected");
	else
	{
		ESP_LOGI(TAG, "VS10xx detected. Version: %d", vsVersion);

		// these 4 lines makes board to run on mp3 mode, no soldering required anymore
		vsWriteSci(SCI_WRAMADDR, 0xc017); //address of GPIO_DDR is 0xC017
		vsWriteSci(SCI_WRAM, 0x0003);	 //GPIO_DDR=3
		vsWriteSci(SCI_WRAMADDR, 0xc019); //address of GPIO_ODATA is 0xC019
		vsWriteSci(SCI_WRAM, 0x0000);	 //GPIO_ODATA=0
		vTaskDelay(150);

		if (vsVersion == 4)						// only 1053b
			vsWriteSci(SCI_CLOCKF, 0x8800);     // SC_MULT = x3.5, SC_ADD= x1
		else
			vsWriteSci(SCI_CLOCKF, 0xB000);

		while (vscheckDREQ() == 0)
			taskYIELD();

		// enable I2C dac output of the vs1053
		if (vsVersion == 4 || vsVersion == 6) // only 1053 & 1063
		{
			vsWriteSci(SCI_WRAMADDR, 0xc017); 
			vsWriteSci(SCI_WRAM, 0x00F0);	 
			vsI2SRate(g_device->i2sspeed);

			// plugin patch
			if ((g_device->options & T_PATCH) == 0)
			{
				uint16_t len = 0;
				if (vsVersion == 4) { // only 1053
					len = sizeof(patch1053) / sizeof(patch1053[0]);
					vsLoadPlugin(patch1053, len);
				}
				if (vsVersion == 6) { // only 1063
					len = sizeof(patch1063) / sizeof(patch1063[0]);
					vsLoadPlugin(patch1063, len);
				}
				ESP_LOGI(TAG, "VS10xx patch is loaded %d \n", len);
			}
		}
		vTaskDelay(5);
		ESP_LOGI(TAG, "volume: %d", g_device->vol);
		setIvol(g_device->vol);
		vsSetVolume(g_device->vol);
		vsSetTreble(g_device->treble);
		vsSetBass(g_device->bass);
		vsSetTrebleFreq(g_device->freqtreble);
		vsSetBassFreq(g_device->freqbass);
		vsSetSpatial(g_device->spacial);
	}
}

int vsSendMusicBytes(uint8_t* music, uint16_t quantity)
{
	if (quantity == 0)
		return 0;
	int oo = 0;
	//	while(vscheckDREQ() == 0);taskYIELD ();
	while (quantity)
	{
		//		if(vscheckDREQ())
		{
			int t = quantity;
			if (t > CHUNK)
				t = CHUNK;
			vsWriteScichar(hvsspi, &music[oo], t);
			oo += t;
			quantity -= t;
		} // else taskYIELD ();
	}
	return oo;
}

void vsSoftwareReset()
{
	vsWriteSci8(SCI_MODE, (SM_SDINEW | SM_LINE1) >> 8, SM_RESET);
	vsWriteSci8(SCI_MODE, (SM_SDINEW | SM_LINE1) >> 8, SM_LAYER12); //mode
}

// Get volume and convert it in log one
uint8_t vsGetVolume()
{
	uint8_t i, j;
	uint8_t value = vsReadSci(SCI_VOL) & 0x00FF;
	for (i = 0; i < 255; i++)
	{
		j = (log10(255 / ((float)i + 1)) * 105.54571334); // magic no?
														  // printf("i=%d  j=%d value=%d\n",i,j,value);
		if (value == j)
			return i;
	}
	return 127;
}

// rough volume
uint8_t vsGetVolumeLinear()
{
	return vsReadSci(SCI_VOL) & 0x00FF;
}

/**
 * Function sets the same volume level to both channels.
 * @param xMinusHalfdB describes damping level as a multiple
 * 		of 0.5dB. Maximum volume is 0 and silence is 0xFEFE.
 * convert the log one to rough one and set it invs1053
 */
void vsSetVolume(uint8_t xMinusHalfdB)
{
	uint8_t value = (log10(255 / ((float)xMinusHalfdB + 1)) * 105.54571334);
	//printf("setvol: %d\n",value);
	if (value == 255)
		value = 254;
	//printf("xMinusHalfdB=%d  value=%d\n",xMinusHalfdB,value);
	vsWriteSci8(SCI_VOL, value, value);
}

/**
 * Functions returns level of treble enhancement.
 * @return Returned value describes enhancement in multiplies
 * 		of 1.5dB. 0 value means no enhancement, 8 max (12dB).
 */
int8_t vsGetTreble()
{
	int8_t treble = (vsReadSci(SCI_BASS) & 0xF000) >> 12;
	if ((treble & 0x08))
		treble |= 0xF0; // negative value
	return (treble);
}

/**
 * Sets treble level.
 * @note If xOneAndHalfdB is greater than max value, sets treble
 * 		to maximum.
 * @param xOneAndHalfdB describes level of enhancement. It is a multiplier
 * 		of 1.5dB. 0 - no enhancement, -8 minimum -12dB , 7 - maximum, 10.5dB.
 * @return void
 */
void vsSetTreble(int8_t xOneAndHalfdB)
{
	uint16_t bassReg = vsReadSci(SCI_BASS);

	if ((xOneAndHalfdB <= 7) && (xOneAndHalfdB >= -8))
		vsWriteSci8(SCI_BASS, MaskAndShiftRight(bassReg, 0x0F00, 8) | (xOneAndHalfdB << 4), bassReg & 0x00FF);
}

/**
 * Sets low limit frequency of treble enhancer.
 * @note new frequency is set only if argument is valid.
 * @param xkHz The lowest frequency enhanced by treble enhancer.
 * 		Values from 0 to 15 (in kHz)
 * @return void
 */
void vsSetTrebleFreq(uint8_t xkHz)
{
	uint16_t bassReg = vsReadSci(SCI_BASS);
	if (xkHz <= 15)
		vsWriteSci8(SCI_BASS, MaskAndShiftRight(bassReg, 0xF000, 8) | xkHz, bassReg & 0x00FF);
}
int8_t vsGetTrebleFreq()
{
	return ((vsReadSci(SCI_BASS) & 0x0F00) >> 8);
}

/**
 * Returns level of bass boost in dB.
 * @return Value of bass enhancement from 0 (off) to 15(dB).
 */
uint8_t vsGetBass()
{
	return ((vsReadSci(SCI_BASS) & 0x00F0) >> 4);
}

/**
 * Sets bass enhancement level (in dB).
 * @note If xdB is greater than max value, bass enhancement is set to its max (15dB).
 * @param xdB Value of bass enhancement from 0 (off) to 15(dB).
 * @return void
 */
void vsSetBass(uint8_t xdB)
{
	uint16_t bassReg = vsReadSci(SCI_BASS);
	if (xdB <= 15)
		vsWriteSci8(SCI_BASS, (bassReg & 0xFF00) >> 8, (bassReg & 0x000F) | (xdB << 4));
	else
		vsWriteSci8(SCI_BASS, (bassReg & 0xFF00) >> 8, (bassReg & 0x000F) | 0xF0);
}

/**
 * Sets low limit frequency of bass enhancer.
 * @note new frequency is set only if argument is valid.
 * @param xTenHz The lowest frequency enhanced by bass enhancer.
 * 		Values from 2 to 15 ( equal to 20 - 150 Hz).
 * @return void
 */
void vsSetBassFreq(uint8_t xTenHz)
{
	uint16_t bassReg = vsReadSci(SCI_BASS);
	if (xTenHz >= 2 && xTenHz <= 15)
		vsWriteSci8(SCI_BASS, MaskAndShiftRight(bassReg, 0xFF00, 8), (bassReg & 0x00F0) | xTenHz);
}

uint8_t vsGetBassFreq()
{
	return ((vsReadSci(SCI_BASS) & 0x000F));
}

uint8_t vsGetSpatial()
{
	if (vsVersion < 3)
		return 0;
	uint16_t spatial = (vsReadSci(SCI_MODE) & 0x0090) >> 4;
	return ((spatial & 1) | ((spatial >> 2) & 2));
}

void vsSetSpatial(uint8_t num)
{
	if (vsVersion < 3)
		return;
	uint16_t spatial = vsReadSci(SCI_MODE);
	if (num <= 3)
	{
		num = (((num << 2) & 8) | (num & 1)) << 4;
		vsWriteSci8(SCI_MODE, MaskAndShiftRight(spatial, 0xFF00, 8), (spatial & 0x006F) | num);
	}
}

uint16_t vsGetDecodeTime()
{
	return vsReadSci(SCI_DECODE_TIME);
}

uint16_t vsGetBitrate()
{
	uint16_t bitrate = (vsReadSci(SCI_HDAT0) & 0xf000) >> 12;
	uint8_t ID = (vsReadSci(SCI_HDAT1) & 0x18) >> 3;
	uint16_t res;
	if (ID == 3)
	{
		res = 32;
		while (bitrate > 13)
		{
			res += 64;
			bitrate--;
		}
		while (bitrate > 9)
		{
			res += 32;
			bitrate--;
		}
		while (bitrate > 5)
		{
			res += 16;
			bitrate--;
		}
		while (bitrate > 1)
		{
			res += 8;
			bitrate--;
		}
	}
	else
	{
		res = 8;

		while (bitrate > 8)
		{
			res += 16;
			bitrate--;
		}
		while (bitrate > 1)
		{
			res += 8;
			bitrate--;
		}
	}
	return res;
}

uint16_t vsGetSampleRate()
{
	return (vsReadSci(SCI_AUDATA) & 0xFFFE);
}

/* to start and stop a new stream */
void vsflush_cancel(uint8_t mode)
{   // 0 only fillbyte  1 before play    2 cancel play
	int8_t endFillByte;
	int16_t y;
	uint8_t buf[513];
	
	if (mode != 2)
	{
		vsWriteSci8(SCI_WRAMADDR, MaskAndShiftRight(para_endFillByte, 0xFF00, 8), (para_endFillByte & 0x00FF));
		endFillByte = (int8_t)vsReadSci(SCI_WRAM) & 0xFF;
		for (y = 0; y < 513; y++)
			buf[y] = endFillByte;
	}

	if (mode != 0) //set CANCEL
	{
		uint16_t spimode = vsReadSci(SCI_MODE) | SM_CANCEL;
		// set CANCEL
		vsWriteSci8(SCI_MODE, MaskAndShiftRight(spimode, 0xFF00, 8), (spimode & 0x00FF));
		// wait CANCEL
		y = 0;
		while (vsReadSci(SCI_MODE) & SM_CANCEL)
		{
			if (mode == 1)
				vsSendMusicBytes(buf, CHUNK); //1
			else
				vTaskDelay(1); //2
							   //		printf ("Wait CANCEL clear\n");
			if (y++ > 200)
			{
				if (mode == 1)
					vsStart();
				break;
			}
		}
		vsWriteSci8(SCI_WRAMADDR, MaskAndShiftRight(para_endFillByte, 0xFF00, 8), (para_endFillByte & 0x00FF));
		endFillByte = (int8_t)vsReadSci(SCI_WRAM) & 0xFF;
		for (y = 0; y < 513; y++)
			buf[y] = endFillByte;
	}
	for (y = 0; y < 5; y++)
			vsSendMusicBytes(buf, 512); // 4*513 = 2052
}

//IRAM_ATTR
void vsTask(void* pvParams)
{
	#define VSTASKBUF 1024
	portBASE_TYPE uxHighWaterMark;
	uint8_t b[VSTASKBUF];
	uint16_t size, s;

	player_t* player = pvParams;

	while (player->decoder_command != CMD_STOP)
	{
		size = min(VSTASKBUF, spiRamFifoFill());

		if (size > 0)
		{
			spiRamFifoRead((char*)b, size);
			s = 0;
			while (s < size)
			{
				s += vsSendMusicBytes(b + s, size - s);
			}
		}
		else
		{
			ESP_LOGE(TAG, "Music buffer is emty - Nothing playing :(");
			vTaskDelay(10);
		}
		vTaskDelay(5);
	}

	spiRamFifoReset();
	player->decoder_status = STOPPED;
	player->decoder_command = CMD_NONE;
	ESP_LOGD(TAG, "Decoder vs10xx stopped.\n");
	uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
	ESP_LOGI(TAG, "watermark: %x  %d", uxHighWaterMark, uxHighWaterMark);
	vTaskDelete(NULL);
}
