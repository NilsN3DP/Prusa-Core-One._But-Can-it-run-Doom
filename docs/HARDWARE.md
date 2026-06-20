# xBuddy Hardware Reference (für DOOM)

Extrahiert aus der offiziellen `Prusa-Firmware-Buddy` (MK4 / MK3.9 / CORE One = "xBuddy" Board).
Dies ist die maßgebliche Quelle für die Standalone-Doom-Firmware.

## MCU & Speicher
- **MCU:** STM32F427VI, Cortex-M4F (FPU, single precision), **168 MHz**
- **Flash (intern):** 2 MB @ 0x08000000
- **RAM:** 192 KB SRAM @ 0x20000000  +  64 KB CCMRAM @ 0x10000000  = **256 KB total**
  - CCMRAM ist **nicht** DMA-fähig (kein Display/Flash-DMA-Puffer dort).
  - Backup-RAM 4 KB @ 0x40024000.
- **Kein externes SDRAM.** ⇒ Doom-Arbeitsset muss < 256 KB bleiben (GBADoom-Ansatz).
- Stack-Top (Prusa): 0x20030000 (Ende der ersten 128 KB). Wir nutzen das volle 192 KB.

## Clock (aus core_init.cpp + stm32f4xx_hal_conf.h)
- **HSE = 12 MHz** (Quarz)
- PLL: PLLM=6, PLLN=168, PLLP=2, PLLQ=7
  - VCO = 12/6 × 168 = 336 MHz → **SYSCLK = 168 MHz**, USB = 336/7 = 48 MHz
- AHB /1 → HCLK 168 MHz
- APB1 /4 → 42 MHz (APB1-Timer 84 MHz)
- APB2 /2 → 84 MHz (APB2-Timer 168 MHz)
- **Flash latency = 5 WS**, Voltage scale 1
- RTC-Quelle: LSE (nicht LSI wie beim MINI)

## Display: ILI9488 (SPI6)
- **Auflösung 480×320**, Pixelformat über SPI **nur RGB666 = 3 Byte/Pixel** (COLMOD 0x66).
  Prusa nutzt MADCTL 0xE0 (mirror XY) ⇒ effektiv Landscape 480(breit)×320(hoch).
- **SPI6** (APB2, 84 MHz): SCK=**PG13**, MISO=**PG12**, MOSI=**PG14**, AF5, Pull-down, very-high speed.
  - Prescaler /2 = 42 MHz, aber Prusa drosselt Befehle/Daten auf /4 = 21 MHz ("reduce baudrate").
  - Mode 0 (CPOL=0, CPHA=0), 8-bit, MSB first, NSS soft.
- Steuer-Pins (GPIO): CS=**PD11**, DC/RS=**PD15**, RST=**PG4** (alle push-pull, high speed).
  - Hinweis: in Prusa-Code ist CS-Toggle für xBuddy ein No-Op; CS wird einmalig low gehalten
    (Display dauerhaft selektiert). Für uns: CS einmal low setzen reicht; DC unterscheidet Cmd/Data.
- Backlight/Helligkeit: **display-intern** über Befehle, kein separater PWM-Pin nötig:
  - 0x51 WRDISBV (brightness), 0x53 WRCTRLD (BCTRL bit5 | BL bit2 einschalten).
- Reset-Sequenz: RST low ≥15 ms, dann high; danach SLPOUT (0x11) + 120 ms warten.
- Befehle: CASET=0x2A, RASET=0x2B, RAMWR=0x2C, MADCTL=0x36, COLMOD=0x3A, DISPON=0x29, SLPOUT=0x11, INVON=0x21.
  - Prusa setzt nach Power-On `INVON` (Display Inversion On) — sonst Farben invertiert.

### ILI9488 Init (neuer Hersteller-Pfad, aus ili9488.cpp)
```
0xF7: A9 51 2C 82        ; Adjust Control 3 (RGB666 loose packet)
0x36: E0                 ; MADCTL (mirror XY)
0x3A: 66                 ; COLMOD = RGB666
0xB1: A0 11              ; Frame Rate Control
0xB4: 02                 ; Display Inversion Control (2-dot)
0xC0: 0F 0F              ; Power Control 1
0xC1: 41                 ; Power Control 2
0xC2: 22                 ; Power Control 3
0xC5: 00 53 80           ; VCOM Control
0xB7: C6                 ; Entry Mode Set
0xE0: 00 08 0C 02 0E 04 30 45 47 04 0C 0A 2E 34 0F   ; Positive Gamma
0xE1: (Negative Gamma — Rest in ili9488.cpp:449+)
0x11 (SLPOUT) + 120ms
0x29 (DISPON)
0x21 (INVON)
```
Einfachere Alternative (alter Hersteller-Pfad, funktioniert auch):
`SLPOUT; 120ms; MADCTL 0xE0; COLMOD 0x66; DISPON; 10ms; clear; INVON`.

## Externer Flash: W25Q (SPI5) — Speicher für die WAD
- **8 MB** (xBuddy), Sektor 4 KB, 64K-Block 0x10000. Standard W25Q-Befehle.
- **SPI5** (APB2, 84 MHz): SCK=**PF7**, MISO=**PF8**, MOSI=**PF9**, AF5, no-pull, very-high.
  - Prescaler /2 = 42 MHz. Mode 0, 8-bit, MSB, NSS soft.
- CS=**PF2** (push-pull, high).
- Lesen: 0x03 (Read Data, 3-Byte-Adresse) oder 0x0B (Fast Read + Dummy). DMA möglich (DMA2_Stream3 RX).
- **Prusa-Belegung des Flash** (nicht überschreiben, wenn Drucker-FW koexistiert):
  Block 65+ = Crash-Dump/PowerPanic/Filesystem. Die unteren ~260 KB + LittleFS-Bereich sind belegt.
  Für DOOM: Wir nutzen den Flash exklusiv (Standalone) ⇒ WAD ab Offset 0x000000 ablegen, oder
  einen freien hohen Bereich wählen. Entscheidung: WAD @ **0x000000** (Standalone, Flash gehört uns).

## Eingabe: Dreh-Encoder + Klick (der Druckerknopf)
- **EN1 = PD13**, **EN2 = PD12** (Quadratur, Pull-up), **Button/ENC = PG3** (Pull-up, aktiv low).
- Mapping-Idee für Doom: Drehen = links/rechts drehen (turn), kurzer Klick = Feuer,
  langer Klick / Doppelklick = Use/Tür, später ggf. Modus-Umschaltung (vor/zurück, Waffe).
  (Nur 1 Encoder + 1 Taste ⇒ Steuerung muss kreativ gemappt werden.)

## Startup / Vector Table
- Startup-Assembler-Referenz: `src/device/stm32f4/startup/stm32f427zitx.s`
- Entry: `Reset_Handler`, ARM Cortex-M4, Thumb.

## SPI-Bus-Übersicht xBuddy (zur Sicherheit)
| SPI  | Verwendung        | SCK  | MISO | MOSI | CS   | AF |
|------|-------------------|------|------|------|------|----|
| SPI2 | Accelerometer     | PB10 | PC2  | PC3  | PA10 | 5  |
| SPI3 | TMC Stepper       | PC10 | PC11 | PC12 | div. | 6  |
| SPI5 | **Ext-Flash W25Q**| PF7  | PF8  | PF9  | PF2  | 5  |
| SPI6 | **Display ILI9488**| PG13 | PG12 | PG14 | PD11 | 5  |

## Flashen (Custom FW)
- Appendix auf dem Board brechen (https://help.prusa3d.com/article/zoiw36imrs).
- ST-Link/SWD über die Debug-Pins (SWDIO=PA13, SWCLK=PA14) oder DFU.
- Standalone-Image startet ab 0x08000000 (kein Prusa-Bootloader-Offset im Standalone-Build).
