# Cass2SD - Cassettes to SD Cards - Kassetten zu SD-Karten

Replacement for cassette recorder by an SD card as mass storage device of old East-German 8-bit computers

Ersatz des Kassettenrekorders als Massenspeichergerät für DDR-Computer (KC85/2, /3 und KC87, Z9001) durch eine SD-Karte

_The rest of the README is written in German since the audience of this tool is most probably mainly German speaking._

## Eigenschaften und Ziel des Projekts

- Der alte Kassettenrecorder soll komplett ersetzt werden, ohne dass Veränderungen an den Computern vorgenommen werden müssen.
- Getestet wurde mit meinen beiden KCs, einem Z9001 (KC85/1.10) und einem KC85/3.
- Das Projekt wurde mit in meinem Bastelfundus vorhandenen Teilen umgesetzt, als da wären:
  - 8-bit Microcontroller ATMEL ATMega328P-PU
  - LCD 1602 (2 Zeilen zu 16 Zeichen mit HD44780-kompatiblem Controller)
  - LM358 Doppel-OPV für den Eingangsverstärker/Schmitttrigger
  - MicroSD Card Adapter für Arduino, welcher schon die notwendigen Pegelwandler von 3,3V auf 5V enthält, aber leider keine Erkennung,
  ob eine Karte gesteckt ist.
  - MCP1702-5002 Festspannungsregler im TO-92 Gehäuse
  - ALPS Drehimpulsgeber mit Taster in der Drehachse aus der EC11 Serie
- Als IDE habe ich AtmelStudio 7 und als Compiler den mitgelieferten avr-gcc genutzt.
- Programmiersprache is C.
