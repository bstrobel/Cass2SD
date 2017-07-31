# Cass2SD - Cassettes to SD Cards - Kassetten zu SD-Karten

Replacement for cassette recorder by an SD card as mass storage device of old East-German 8-bit computers

Ersatz des Kassettenrekorders als Massenspeichergerät für DDR-Computer (KC85/2, KC85/3 und KC87, Z9001) durch eine SD-Karte

_The rest of the README and the Wiki is written in German since the audience of this tool is most probably mainly German speaking._

_Die Kommentare im Quellcode sind in Englisch gehalten, da ich das aus meinem Beruf so gewöhnt bin und sonst
den Text erst im Kopf ins Deutsche übersetzen müsste. Ich weiß, das klingt ein wenig affektiert, ist aber nicht
so gemeint._

## Eigenschaften und Ziele des Projekts

- Der alte Kassettenrecorder soll komplett ersetzt werden, ohne dass Veränderungen an den Computern vorgenommen werden müssen.
- Die Benutzeroberfläche soll einfach und bequem zu handhaben sein. Es sollen aber auch alle relevanten Funktionen und Informationen
  zur Verfügung stehen.
- Getestet wurde mit meinen beiden KCs, einem Z9001 (KC85/1.10) und einem KC85/3.
- Das Projekt wurde mit in meinem Bastelfundus vorhandenen Teilen umgesetzt, als da wären:
  - 8-bit Microcontroller ATMEL ATMega328P-PU
  - LCD 1602 (2 Zeilen zu 16 Zeichen mit HD44780-kompatiblem Controller)
  - LM358 Doppel-OPV für den Eingangsverstärker/Schmitt-Trigger
  - MicroSD Card Adapter für Arduino, welcher schon die notwendigen Pegelwandler von 3,3V auf 5V enthält, aber leider keine Erkennung,
  ob eine Karte gesteckt ist.
  - MCP1702-5002 Festspannungsregler im TO-92 Gehäuse
  - ALPS Drehimpulsgeber mit Taster in der Drehachse aus der EC11 Serie
- Als IDE habe ich AtmelStudio 7 und als Compiler den mitgelieferten avr-gcc genutzt.
- Programmiert und debugt habe ich den Atmel mit einem AVR Dragon.
- Programmiersprache ist C.
- Benutzte OpenSource-Bibliotheken - _meinen allerherzlichsten Dank an die Autoren, ohne deren Arbeit mein Projekt wohl nicht möglich gewesen wäre_:
  - Für die LCD-Anzeige-Routinen habe ich [die Bibliothek von Peter Fleury](http://homepage.hispeed.ch/peterfleury/avr-software.html#libs)  verwendet.
  - Für die FAT- und SD-Karten-SPI-Schnittstelle benutze ich die [FatFs-Bibliothek von Elm Chan](http://elm-chan.org/fsw/ff/00index_e.html)
- Was nicht geht:
  - Dateiliste sortieren. Der AVR hat einfach nicht genug Hauptspeicher dafür. Die Dateien werden genau in der
    Reihenfolge angezeigt, in der sie in der FAT gespeichert sind.
  - Verzeichnisse anlegen.
  - Dateien und Verzeichnisse löschen.  

## [Cass2SD Wiki](https://github.com/bstrobel/Cass2SD/wiki)