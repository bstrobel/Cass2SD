# Cass2SD - Cassettes to SD Cards - Kassetten zu SD-Karten

Replacement for cassette recorder by an SD card as mass storage device of old East-German 8-bit computers

Ersatz des Kassettenrekorders als Massenspeichergerät für DDR-Computer (KC85/2, KC85/3 und KC87, Z9001) durch eine SD-Karte

_The rest of the README is written in German since the audience of this tool is most probably mainly German speaking._

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

## Bedienphilosophie:
- Nach dem Einschalten und Erkennen der SD-Karte wird das Label, der verfügbare Speicherplatz in MByte
  und der Dateisystemtyp angezeigt.
- Mittels Drehimpulsgeber können nacheinander die Dateien im Verzeichnis angezeigt werden.
- Es wird der Dateiname mit Erweiterung und die Dateigröße angezeigt.
- Aufgrund der beschränkten Anzeigekapazität des Displays kann immer nur eine Datei angezeigt werden.
- Unterverzeichnisse werden als solche gekennzeichnet indem anstatt der Dateigröße "[DIR]" angezeigt wird.
- In jedem Unterverzeichnis wird "[..]" angezeigt, so dass wieder zum nächsthöheren Verzeichnis navigiert werden kann.
- Der Taster im Drehimpulsgeber dient zum Auslösen von Aktionen, abhängig davon was gerade angezeigt wird:
  - **Datei:** Datei wird wiedergegeben (zum KC gesendet).
  - **Verzeichnis:** Wechsel in dieses Verzeichnis
  - **[..]:** Wechsel in nächsthöhere Verzeichnisebene
  - **Sendefortschritt:** Abruch des Sendevorganges
  - **Empfangsfortschritt:** keine Aktion
  - **Startanzeige:** keine Aktion
- Wird etwa 2 Sekunden keine Eingabe getätigt und gerade ein Datei angezeigt, so ändert sich die Anzeige.
  Es werden falls möglich Informationen aus dem FCB der Datei angezeigt, wie Dateiname, Dateityp und Blockanzahl.
- Die **Wiedergabe** der aktuell angezeigten Datei wird automatisch gestartet, falls
  - ein **Z9001, KC85/1, KC87** eine Folge von mehreren Trennbits (Spaces) auf seinem Wiedergabekanal sendet.
  - ein **KC85/3** einen entsprechenden Pegel an Pin 5 der Diodenbuchse anlegt.
- Die **Aufnahme** wird automatisch gestartet nachdem mindestens 5306 Eins-Bits (also ein Dateivorton) erkannt wurden.
  - Gespeichert wird die Aufnahme im aktuell ausgewählten Verzeichnis.
  - Der Dateiname wird aus dem Dateinamen ohne Erweiterung im FCB der empfangenen Datei und der Erweiterung ".TAP" gebildet.
  - Falls eine Datei mit demselben Namen schon existiert, wird sie ohne Rückfrage überschrieben.

## Dateiformate:
- Meine Erkenntnisse hinsichtlich der Dateiformate basieren größtenteils auf Informationen von
  [Volker Pohlers Seite](https://hc-ddr.hucki.net/wiki/doku.php/z9001:kassettenformate) und dem Pascal-Quellcode
  seiner angepassten KCLOAD Version. _And dieser Stelle ein herzliches Dankeschön an Volker für sein informative und gut
  gepflegte Seite!_
- **Gespeichert** wird im verbreiteten TAP-Format mit folgenden Eigenschaften:
  - 16 Byte Header: "[0xC3]KC-TAPE by AF. "
  - Die vom KC gesendeten Blocknummern werden immer mitgespeichert.
  - Die Prüfsumme wird nicht mitgespeichert.
  - Ein Block in der Datei ist damit immer 129 Byte lang.
  - Eine Datei ist immer 16 Bytes + 129 Bytes * Blockanzahl groß.
  - Ziel war es, die Daten so zu speichern, dass sie bei der Wiedergabe wieder genauso hergestellt werden können.
- Die **Wiedergabe** wurde möglichst fehlertolerant gestaltet, was aber angesichts der langen Historie und der vielen entstandenen
  und nicht standardisierten Formate nicht immer erfolgreich funktioniert.
  - Jede Datei wird zuerst auf den TAP-Header überprüft. Wird dieser gefunden, so werden folgende Annahmen getroffen:
    - Der erste Block enthält einen gültigen Standard-FCB oder BASIC-FCB.
	- Blocknummern sind enthalten.
	- Blockprüfsummen sind nicht enthalten.
  - Falls kein TAP-Header gefunden wird, wird zuerst die gesamte Datei überprüft, ob sie fortlaufende Blocknummern enthält.
    Außerdem wird versucht, einen Standard oder einen BASIC-FCB im ersten Block zu finden. Generell funktioniert diese Erkennung
	nicht immer korrekt. Dann sollte die Datei eventuell in einem Hex-Editor angepasst werden.
	
## To Do
- Kompletten Schaltplan zeichnen, da die Schaltung zum großen Teil bisher nur auf dem Steckbrett existiert.
- Leiterplatte entwerfen und fertigen lassen.
- Compilierte Dateien hochladen und Anleitung erstellen, wie sie in einen AVR geladen werden können.
- Möglichkeit implementieren um Einstellungen über ein Menü vorzunehmen und im EEPROM zu speichern.
- Möglichkeit Dateien und Verzeichnisse zu löschen.
- Möglichkeit Verzeichnisse mit einem Standardnamen anzulegen (DIR1, DIR2, ...).
- Noch mehr Details aus dem FCB anzeigen (Anfangs-, End-, Startadresse usw.).
- Weitere 8-Bit Computer unterstützen wie Z1013, Poly880 usw.

## Danksagungen/Thanks/Links
- [Volker Pohlers für deine sehr informative Seite zum Thema DDR-Computer](https://hc-ddr.hucki.net)
- [Ulrich Zander für deine sehr informative Seite vor allem zum Thema Z9001](http://www.sax.de/~zander/index2h.html)
- [Elm Chan for your high quality FatFs library](http://elm-chan.org/fsw/ff/00index_e.html)
- [Peter Fleury für deine LDC Bibliothek und die sehr nützlichen Ausgabefunktionen dazu](http://homepage.hispeed.ch/peterfleury/avr-software.html#libs)