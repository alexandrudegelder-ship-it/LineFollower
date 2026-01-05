# Sensoren proof of concept

---

# Lineaire Sensor Module (PCB-ontwerp)

Dit onderdeel van het project demonstreert de onafhankelijke uitlezing van meerdere sensoren via een analoge multiplexer, waarbij de focus ligt op een maximaal dynamisch bereik en minimale software-overhead.

# 1. Hardware-architectuur

De module is opgebouwd rond de **CD74HC4067**, een 16-kanaals analoge multiplexer/demultiplexer. Hiermee kunnen tot 16 individuele "KATI"-sensoren worden uitgelezen via slechts één analoge ingang op de microcontroller.

### Onafhankelijke Uitlezing (6 Sensoren)

Hoewel de PCB 16 kanalen ondersteunt, is het systeem ontworpen om aan te tonen dat minimaal 6 sensoren volledig onafhankelijk van elkaar functioneren.

* **Multiplexing:** Door de adres-pinnen (S0 t/m S3) aan te sturen, wordt fysiek slechts één sensorpad tegelijk verbonden met de gemeenschappelijke `SIG`-lijn. Dit elimineert overspraak (cross-talk) tussen de sensoren.
* **Signaalconditionering:** Elke sensor is opgenomen in een spanningsdeler-netwerk met een precisieweerstand van **10kΩ (R21-R36)** en een **100nF condensator (C2-C17)** voor ruisonderdrukking.

# 2. Optimalisatie van het AD-Bereik

Om de 10-bit resolutie (0-1023) van de Arduino ADC optimaal te benutten, is de hardware als volgt afgestemd:

* **Spanningsbereik:** De sensoren werken op een **5V VCC**. De weerstandswaarden in de spanningsdeler zijn zo gekozen dat de spanningszwaai bij variatie van de lineaire sensor het volledige bereik tussen 0V en 5V nadert.
* **Resolutie:** Hierdoor wordt elke stap van de ADC (ongeveer 4.88mV per eenheid) effectief gebruikt, wat resulteert in een hoge precisie zonder dat er softwarematige versterking nodig is.

# 3. Minimale Software-implementatie

De software is bewust minimalistisch gehouden om de pure hardware-werking aan te tonen, zonder complexe bewerkingen zoals kalibratie of interpolatie.

### Logica:

1. **Adressering:** De microcontroller stuurt een 4-bits binair getal naar pinnen S0-S3 om de gewenste sensor (kanaal 0 t/m 5) te selecteren.
2. **Data-acquisitie:** Er wordt direct een `analogRead()` uitgevoerd op de `SIG`-lijn.
3. **Output:** De ruwe ADC-waarde wordt onmiddellijk doorgestuurd, wat bewijst dat de data direct en onbewerkt van de specifieke PCB-locatie afkomstig is.

# 4. Pinbezetting (Samenvatting)

| Component | Pin Type | Verbinding |
| --- | --- | --- |
| **Multiplexer SIG** | Analoog Input | Arduino A0 |
| **Adres S0 - S3** | Digitaal Output | Arduino D-pinnen |
| **VCC** | Voeding | 5V |
| **KATI 1-6** | Sensor Inputs | Kanalen C0 - C5 |

---
