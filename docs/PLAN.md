# Can It Run Doom — Stand & Plan (xBuddy)

## Was steht (verifiziert)
- **Toolchain:** arm-none-eabi-gcc 13.2.1 + ninja, kompiliert/linkt Cortex-M4F. ✅
- **Hardware komplett reverse-engineered** aus der Prusa-FW → `docs/HARDWARE.md`. ✅
- **Phase-A Bring-up-Firmware** (`build/doom.bin`): testet Display (ILI9488), externen Flash
  (W25Q SPI), Encoder + Taste, 168-MHz-Clock. Flashbar, am echten Board verifizierbar. ✅
- **Reversibilität geklärt:** voll reversibel (interner Flash = Prusa-FW neu flashen; externer
  W25Q wird von Prusa-FW beim Boot selbst regeneriert; Kalibrierung liegt auf separatem
  I²C-EEPROM, wird nie angefasst). ✅
- **GBADoom** geklont + analysiert. ✅

## Der harte technische Befund
GBADoom greift auf WAD-Daten **per Pointer in ein memory-mapped Array** zu
(`W_CacheLumpNum` → `&doom_iwad[filepos]`), und der Renderer liest diese Pointer
**pro Frame** direkt. Folgen für den xBuddy:
- Kein SDRAM, externer W25Q über reines SPI **nicht** memory-mappbar.
- Einzig memory-mapbarer Speicher = **interner 2-MB-Flash**.
- ⇒ Die WAD **muss in den internen Flash** und dort komplett liegen.
- GBADooms Shareware-WAD ist **3,84 MB** → passt **nicht** in 2 MB.

**Konsequenz:** Voll-WAD vom USB-Stick / externem Flash funktioniert mit GBADoom **nicht**
(würde ein Neuschreiben des Renderers erfordern — unrealistisch). Der einzige saubere Weg
auf dieser Hardware ist eine **reduzierte WAD (≤ ~1,6 MB)**, fest mit der Firmware in den
internen Flash gebacken → **ein** `.bin` per ST-Link, voll reversibel.

Warum nicht „voll + extern": Engines, die aus Datei/Flash streamen (Vanilla/Chocolate/PrBoom),
brauchen ein Zone-Heap von mehreren MB → passt nicht in 256 KB RAM. GBADoom passt in 256 KB,
verlangt dafür die mmap-WAD. Beides zugleich (volle WAD aus externem Flash **und** 256 KB RAM)
gibt es nicht ohne eine eigene Low-RAM-Streaming-Engine von Grund auf (Forschungsaufwand).

## Empfohlener Weg
**GBADoom → STM32F427-Port mit reduzierter WAD im internen Flash.**
1. GBADoom-Engine-Code für unser Target kompilieren (GBA-HAL raus), Code-Größe messen → echtes WAD-Budget.
2. Plattform-Layer schreiben: Video → ILI9488 (Doom 240×160 bzw. 320×200 → 480×320 skaliert,
   Palette → RGB666), Eingabe → Encoder/Taste, Timing → SysTick.
3. Reduzierte WAD (≤ ~1,6 MB) erzeugen (WAD-Tooling: SLADE/deutex/omgifol — noch zu installieren),
   als C-Array einbetten.
4. Bauen, in 2 MB Flash / 256 KB RAM einpassen, `.bin` liefern. Du flashst + testest.

## Offene Punkte
- WAD-Cutting-Tooling auf diesem Rechner installieren.
- Engine-Codegröße messen (bestimmt, wie viel WAD-Inhalt reinpasst → wie viele Level/Sprites).
- Erwartung: **reduziertes Doom** (1 Level / Teil der Sprites), nicht die volle Episode — wegen 2-MB-Limit.
