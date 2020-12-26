/*
 * VS10xx.h
 *
 *  Created on: 25-04-2011
 *      Author: Przemyslaw Stasiak
 *
 * Mofified for KaRadio32
 *		Author: KaraWin
 * Mofified for KaRadio32
 *		Author: Pilnikov
 */
#pragma once
#ifndef VS10xx_H_
#define VS10xx_H_

#include "esp_system.h"
#include <string.h>
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <math.h>
#include "app_main.h"
#include "audio_player.h"
#include "spiram_fifo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define min(a, b) (((a) < (b)) ? (a) : (b))
#define SET 0
#define RESET 1

// gpio are defined in gpio.h


#define RXNE    0x01
#define TXE     0x02
#define BSY     0x80

#define VS_WRITE_COMMAND 	0x02
#define VS_READ_COMMAND 	0x03
#define SCI_MODE        	0x00
#define SCI_STATUSVS      	0x01
#define SCI_BASS        	0x02
#define SCI_CLOCKF      	0x03
#define SCI_DECODE_TIME 	0x04
#define SCI_AUDATA      	0x05
#define SCI_WRAM        	0x06
#define SCI_WRAMADDR    	0x07
#define SCI_HDAT0       	0x08
#define SCI_HDAT1       	0x09
#define SCI_AIADDR      	0x0a
#define SCI_VOL         	0x0b
#define SCI_AICTRL0     	0x0c
#define SCI_AICTRL1     	0x0d
#define SCI_AICTRL2     	0x0e
#define SCI_AICTRL3     	0x0f
#define SM_DIFF         	0x01
#define SM_JUMP         	0x02
#define SM_LAYER12			0x02
#define SM_RESET        	0x04
#define SM_CANCEL           0x08
#define SM_OUTOFWAV     	0x08
#define SM_PDOWN        	0x10
#define SM_TESTS        	0x20
#define SM_STREAM       	0x40
#define SM_PLUSV        	0x80
#define SM_DACT         	0x100
#define SM_SDIORD       	0x200
#define SM_SDISHARE     	0x400
#define SM_SDINEW       	0x800
#define SM_ADPCM        	0x1000
#define SM_ADPCM_HP     	0x2000
#define SM_LINE1            0x4000
#define para_endFillByte    0x1E06


//public functions

void Spi_init();

int     get_vsVersion();
bool 	vsHW_init();
void 	vsI2SRATE(uint8_t speed);
void    vsInfo();
void	vsStart();
void	vsI2SRate(uint8_t speed);
int 	vsSendMusicBytes(uint8_t* music,uint16_t quantity);
void 	vsSoftwareReset();
uint16_t	vsGetBitrate();
uint16_t	vsGetSampleRate();
uint16_t	vsGetDecodeTime();
void	vsflush_cancel(uint8_t mode);// 0 only fillbyte  1 before play    2 close play

//Volume control
void	vsDisableAnalog(void);
uint8_t vsGetVolume();
uint8_t vsGetVolumeLinear();
void	vsSetVolume(uint8_t xMinusHalfdB);
void 	vsVolumeUp(uint8_t xHalfdB);
void	vsVolumeDown(uint8_t xHalfdB);
//Treble control
int8_t	vsGetTreble();
void	vsSetTreble(int8_t xOneAndHalfdB);
void	vsSetTrebleFreq(uint8_t xkHz);
int8_t	vsGetTrebleFreq(void);
//Bass control
uint8_t	vsGetBass();
void	vsSetBass(uint8_t xdB);
void	vsSetBassFreq(uint8_t xTenHz);
uint8_t	vsGetBassFreq(void);
// Spacial
uint8_t	vsGetSpatial();
void vsSetSpatial(uint8_t num);
// reduce the chip consumption
void vsLowPower();
// normal chip consumption
void vsHighPower();
//private functions
void ControlReset(uint8_t State);
void vsWriteSci8(uint8_t addr, uint8_t highbyte, uint8_t lowbyte);
void vsWriteSci(uint8_t addr, uint16_t value);
void vsWriteScichar(spi_device_handle_t ivsspi, uint8_t *cbyte, uint16_t len);
uint16_t vsReadSci(uint8_t addr);
void vsResetChip();

uint16_t MaskAndShiftRight(uint16_t Source, uint16_t Mask, uint16_t Shift);

uint8_t vscheckDREQ();

void vsregtest();
void vsLoadPlugin(const uint16_t *d, uint16_t len);

//void vsPluginLoad();

void vsTask(void *pvParams) ;

#endif /* VS10xx_H_ */
