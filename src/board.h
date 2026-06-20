/* xBuddy pin map (see docs/HARDWARE.md). Ports/pins from Prusa-Firmware-Buddy. */
#pragma once
#include "stm32f4xx.h"

/* Display ILI9488 on SPI6 (AF5): SCK PG13, MISO PG12, MOSI PG14 */
#define DISP_SPI        SPI6
#define DISP_SCK_PORT   GPIOG
#define DISP_SCK_PIN    13
#define DISP_MISO_PORT  GPIOG
#define DISP_MISO_PIN   12
#define DISP_MOSI_PORT  GPIOG
#define DISP_MOSI_PIN   14
#define DISP_SPI_AF     5
/* Display control pins */
#define DISP_CS_PORT    GPIOD
#define DISP_CS_PIN     11
#define DISP_DC_PORT    GPIOD   /* a.k.a. RS */
#define DISP_DC_PIN     15
#define DISP_RST_PORT   GPIOG
#define DISP_RST_PIN    4

/* External flash W25Q (8 MB) on SPI5 (AF5): SCK PF7, MISO PF8, MOSI PF9 */
#define FLASH_SPI       SPI5
#define FLASH_SCK_PORT  GPIOF
#define FLASH_SCK_PIN   7
#define FLASH_MISO_PORT GPIOF
#define FLASH_MISO_PIN  8
#define FLASH_MOSI_PORT GPIOF
#define FLASH_MOSI_PIN  9
#define FLASH_SPI_AF    5
#define FLASH_CS_PORT   GPIOF
#define FLASH_CS_PIN    2

/* Rotary encoder + button (the printer knob) */
#define ENC_A_PORT      GPIOD   /* jogWheelEN1 */
#define ENC_A_PIN       13
#define ENC_B_PORT      GPIOD   /* jogWheelEN2 */
#define ENC_B_PIN       12
#define ENC_BTN_PORT    GPIOG   /* jogWheelENC, active low */
#define ENC_BTN_PIN     3

/* ILI9488 addressing dimensions used on the CORE One. The CORE One needs MADCTL
 * 0x40 + a transposed blit to render upright (the MK4 uses MADCTL 0xE0 / 480x320);
 * with 0x40 the address window spans 320 along CASET and 480 along RASET. */
#define DISP_W 320
#define DISP_H 480
