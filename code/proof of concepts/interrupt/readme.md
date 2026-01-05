# start/stop interrupt proof of concept

### Projectanalyse: Start/Stop-bediening met Hardware Interrupts

Dit systeem is ontworpen om de correcte werking van een start/stop-functionaliteit aan te tonen via real-time hardware interrupts, waarbij de communicatie via een specifieke bus verloopt.

### 1. De Centrale Verwerking (Arduino RF-NANO)
De **Arduino RF-NANO** vormt het hart van de bediening.
* **Voeding:** Het systeem wordt stabiel gevoed via de **VIN-pin met 9V**.
* **Interne Logica:** De Nano zet dit intern om naar 5V voor de sensoren en knoppen.
* **RF-Gereserveerd:** De pinnen D9 en D10 zijn gereserveerd voor de ingebouwde radio-module.


### 2. Hardware Interrupt Configuratie (Minimale Hardware)
Om een onmiddellijke reactie op gebruikersinvoer te garanderen zonder de hoofdloop te vertragen, wordt gebruikgemaakt van hardware interrupts op de daarvoor bestemde pinnen.
* **START-knop (Pin D2):** Verbonden met een externe **10kΩ pull-up weerstand (R1)** naar 5V+. Dit houdt het signaal "HIGH" tot de knop wordt ingedrukt.
* **STOP-knop (Pin D3):** Eveneens verbonden met een **10kΩ pull-up weerstand (R2)**.
* **Werking:** De pinnen D2 en D3 zijn de enige pinnen op de Nano die hardware interrupts ondersteunen. Door de pull-up configuratie reageert de software op een `FALLING` edge (de overgang van 5V naar 0V bij indrukken).

### 3. Communicatie-interface (D7 en D8)
In het gecorrigeerde schema zijn pinnen **D7** en **D8** volledig gereserveerd voor datacommunicatie.
* **Vrije Lijnen:** Er zijn geen LEDs of weerstanden meer direct op deze pinnen aangesloten, wat essentieel is om signaalruis en spanningsval te voorkomen tijdens dataoverdracht.
* **Protocol:** Deze pinnen zijn nu beschikbaar voor protocollen zoals SoftwareSerial naar externe randapparatuur.

### 4. Visuele Feedback (HMI)

De indicatie-LEDs zijn verplaatst naar de analoge pinnen om de digitale pinnen vrij te maken. Elke LED is voorzien van een **220Ω voorschakelweerstand** ter bescherming.

| Indicator | Pin | Functie |
| ------ | --- |---------|
| **START_LED** | A0 | Bevestiging van actieve status |
| **STOP_LED** | A1 | Bevestiging van stop-commando |
| **KALIBREREN** | A2 | Status van sensor-instellingen |
| **ERROR** | A3 | Foutmelding indicatie |
| **RESET** | A4 | Bevestiging van systeem-reset |

### Technische Samenvatting

Door de **START/STOP** op **D2/D3** te plaatsen en de **communicatie** op **D7/D8**, voldoet de hardware aan de eis van "minimale hardware en software" voor een betrouwbaar, interrupt-gestuurd systeem.
