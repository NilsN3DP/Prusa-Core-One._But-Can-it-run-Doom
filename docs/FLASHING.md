# Flashen & Testen (xBuddy)

## Voraussetzung
- Custom-FW erlaubt: **Appendix auf dem xBuddy-Board brechen**
  (https://help.prusa3d.com/article/zoiw36imrs-flashing-custom-firmware).
- SWD-Debugger (ST-Link V2/V3) an den Debug-Pins: **SWDIO=PA13, SWCLK=PA14, GND, 3V3**.

## ⭐ CORE One: Flashen per USB-Stick (.bbf, der reguläre Weg)
Der xBuddy hat einen Prusa-Bootloader; eine rohe `.bin` lehnt er ab ("erkennt Firmware nicht").
Wir bauen daher an den Bootloader-Offset `0x08020200` und verpacken als **`.bbf`**.

Voraussetzung: **Appendix gebrochen** (für unsignierte Custom-FW) — erledigt.

1. Bauen (Boot-Variante ist Default) + packen:
   ```
   ninja -C build bringup.elf
   tools/make_bbf.sh build/bringup.bin      # -> build/bringup.bbf
   ```
2. `build/bringup.bbf` ins **Wurzelverzeichnis eines FAT32-USB-Sticks** kopieren.
3. Stick in den **Frontport der CORE One** stecken, Drucker neu starten.
4. Der Bootloader erkennt die `.bbf` (Version 99.0.0, also "neuer") und bietet das Flashen an →
   bestätigen. Danach startet unsere Firmware.

> Die `.bbf` ist als **CORE One** (type 7) signaturfrei gepackt; der Bootloader bleibt erhalten.
> **Zurück zur Original-FW:** offizielle Prusa-`.bbf` per USB flashen — stellt auch die
> GUI-Ressourcen im externen Flash wieder her. Vollständig reversibel.

## (Alternative) Phase-A Bring-up via SWD/ST-Link (roh, ohne Bootloader)
Nur falls du doch einen ST-Link nutzt: mit `-DUSE_BOOTLOADER=OFF` neu konfigurieren
(Image ab `0x08000000`), dann `build/bringup.bin` per SWD flashen.
Image: `build/doom.bin` (lädt nach **0x08000000**, kein Bootloader-Offset im Standalone-Build).

Mit ST-Link (Open-Source `openocd`):
```
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/doom.bin 0x08000000 verify reset exit"
```
Oder `STM32_Programmer_CLI`:
```
STM32_Programmer_CLI -c port=SWD -d build/doom.bin 0x08000000 -v -rst
```
Oder `.hex` (Adresse ist enthalten): `build/doom.hex`.

> ⚠️ Standalone-FW überschreibt die Prusa-Firmware im internen Flash. Zum Zurückkehren
> die offizielle Prusa-FW per USB/`.bbf` neu aufspielen. Der externe SPI-Flash (W25Q)
> wird von Phase A **nur gelesen** (JEDEC-ID), nicht beschrieben.

## Was du sehen solltest (Phase A)
- Oben: **6 Farbbalken** (rot, grün, blau, gelb, cyan, weiß) → Display + Adressierung OK.
- Darunter links ein **Quadrat: grün** wenn der W25Q-Flash mit Winbond-ID (0xEF) antwortet,
  **rot** wenn nicht → SPI5/Flash OK.
- Mittig ein **Block**: Knopf drehen bewegt ihn, Knopf drücken färbt ihn rot → Encoder + Taste OK.

Wenn das alles stimmt, ist die HAL (Display/Flash/Encoder/Clock) korrekt und wir setzen Doom darauf.
Wenn etwas fehlt (z. B. Display schwarz, Farben invertiert, Block bewegt sich falschherum),
bitte melden — das sind kleine, gezielte Anpassungen (Init-Flags, Inversion, Encoder-Richtung).
