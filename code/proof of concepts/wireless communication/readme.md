# draadloze communicatie proof of concept

# Technische Analyse: Arduino RF Nano V3.0 & Communicatie-architectuur

De **Arduino RF Nano V3.0** is een hybride microcontroller die een ATmega328P (de klassieke Nano) integreert met een **nRF24L01+** 2.4GHz transceiver op één enkele PCB. Deze integratie vereist een slimme toewijzing van pinnen om conflicten tussen interne radiofuncties en externe datacommunicatie te voorkomen.

## 1. Interne Radio-interface (SPI)

De interne nRF24L01+ chip communiceert met de processor via de **SPI-bus**. In het ontwerp van de RF Nano zijn de volgende pinnen intern vast verbonden (hard-wired):

* **D9 (CSN) & D10 (CE):** Deze pinnen sturen de radiochip aan.
* **D11, D12, D13:** De standaard SPI-lijnen (MOSI, MISO, SCK).

Omdat deze pinnen essentieel zijn voor de draadloze werking, mogen ze niet voor andere kritieke taken in het schema worden gebruikt.

## 2. Externe Communicatie via D7 en D8

Voor de bedrade communicatie tussen de bedieningsunit en de sensormodule is er specifiek gekozen voor pinnen **D7** en **D8**.

### Waarom D7 en D8?

In jouw gecorrigeerde schema fungeren deze pinnen als een **dedicated communicatiebus**.

1. **Isolatie van de Radio:** Door D7 en D8 te gebruiken, blijft de interne radio (op D9/D10) volledig ongestoord.
2. **Signaalintegriteit:** In de definitieve versie van het schema zijn de LEDs van deze pinnen verwijderd. Dit is cruciaal; een LED werkt als een ballast die de digitale flank (de overgang tussen 0V en 5V) vertraagt of vervormt, wat bij hogere baudrates tot corruptie van data leidt.
3. **Software Emulatie:** Deze pinnen zijn ideaal voor protocollen zoals *SoftwareSerial*, waarbij we een seriële poort simuleren zonder de hardwarematige USB-communicatie (D0/D1) te belasten.

## 3. Systeemintegratie & Dataflow

De kracht van jouw ontwerp ligt in de taakverdeling van de pinnen:

* **Real-time Inputs (D2/D3):** Gebruik van **Hardware Interrupts** voor de Start/Stop-knoppen. De processor stopt onmiddellijk met zijn huidige taak om deze signalen te verwerken.
* **Datatransport (D7/D8):** Zodra een interrupt wordt gedetecteerd, wordt er een datapakket via de bus op D7/D8 verzonden naar de rest van het systeem.
* **Visuele Feedback (A0-A4):** Door de LEDs naar analoge pinnen te verplaatsen, blijven de snelle digitale lijnen (zoals D7/D8) vrij van elektrische ruis.

### Samenvatting Pinbezetting

| Groep | Pinnen | Functie |
| --- | --- | --- |
| **Interrupts** | D2, D3 | Real-time Start/Stop detectie |
| **Communicatie** | **D7, D8** | Schone data-interface (bedraad) |
| **Interne RF** | D9 t/m D13 | Draadloze 2.4GHz verbinding |
| **Feedback** | A0 t/m A4 | Statusindicatie (LEDs) |

Deze opbouw garandeert een robuust systeem waarbij draadloze signalen, bedrade data en fysieke knoppen elkaar niet in de weg zitten.
