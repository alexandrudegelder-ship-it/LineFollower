# H-Bridge proof of concept

Je schema ziet er goed doordacht uit! Het gebruik van een **TB6612FNG** in combinatie met een **MP1584EN** regulator is een slimme keuze voor een robotica-project, vooral vanwege de efficiëntie.

Hier is een uitleg van hoe jouw specifieke configuratie werkt en waarom deze opbouw technisch goed in elkaar zit:

### 1. De Voedingslijn: Van Lipo naar 6V

In je tweede schema (Zie pdf schema) zie ik dat je een **11.1V bron** (waarschijnlijk een 3S LiPo batterij) via een XT60-connector aansluit op de **MP1584EN** (de "Regulator Motor").

* **De MP1584EN:** Dit is een *Step-Down (Buck) converter*. Hij zet de 11.1V efficiënt om naar **6V**. Het grote voordeel hiervan is dat je motoren een constante spanning krijgen, zelfs als de batterijspanning langzaam zakt tijdens het rijden.
* **Condensator C9 (1000uF):** Deze grote condensator aan de uitgang is essentieel. Hij vangt de "klappen" op wanneer de motoren plotseling veel stroom trekken (bijvoorbeeld bij het wegrijden), waardoor je systeem stabiel blijft en niet reset.

### 2. De TB6612FNG Dubbele H-Brug

In de schema (zie pdf) zien we de verbindingen naar de motor driver:

* **VM (Pin 1):** Deze krijgt de **6V** van je regulator. Dit is de krachtstroom die rechtstreeks naar de motoren gaat.
* **VCC (Pin 2):** Deze is verbonden met de **5V** van je Arduino. Dit voedt de interne logica van de chip. Het scheiden van VM (motoren) en VCC (logica) is cruciaal om ruis van de motoren weg te houden bij je microcontroller.
* **STBY (Pin 12):** Ik zie dat je deze via een **10kΩ weerstand (R1)** aan de 5V hebt gelegd. Dit is erg slim; het zorgt ervoor dat de driver altijd "aan" staat (niet in standby-modus) tenzij je hem handmatig laag maakt via je Arduino.

### 3. Aansluiting op de Arduino RF V3.0

De signalen die van je Arduino komen, bepalen wat de motoren doen:

* **AIN1/AIN2 & BIN1/BIN2:** Deze bepalen de **richting** van de motoren (bijv. Pin D4 en D5 voor Motor A).
* **PWMA & PWMB:** Deze zijn verbonden met PWM-pinnen (D3 en D10). Hiermee regel je de **snelheid** door de 6V pulsmatig aan te bieden.

### Waarom dit een goede setup is:

1. **Geen Oververhitting:** Omdat de TB6612FNG MOSFET's gebruikt, verlies je minder spanning dan bij een oude L298N. De 6V die je erin stopt, komt ook bijna volledig bij je motoren aan.
2. **Stroombeveiliging:** De MP1584EN kan tot 3A leveren, wat ruim voldoende is voor de meeste kleine motoren die je met deze H-brug aanstuurt (die meestal rond de 1.2A continu vragen).
3. **Ontkoppeling:** Je hebt zowel bij de 6V (C1, C3) als bij de 5V (C5, C6) condensatoren geplaatst. Dit voorkomt dat elektrische ruis van de motoren je Arduino in de war brengt.

**Tip:** Zorg dat de GND van de MP1584EN en de GND van de Arduino goed met elkaar verbonden zijn (Common Ground), anders kunnen de stuursignalen niet correct worden geïnterpreteerd door de H-brug.
