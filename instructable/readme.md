# Instructable: Linefollower Robot Assembly

Dit stappenplan beschrijft hoe je de robot bouwt met de JLCPCB-modules en de 3D-geprinte mechanische onderdelen.

### Stap 1: Bestellen van Hardware & PCB's

1. **Elektronika:** Exporteer de Gerber-bestanden en de Bill of Materials (BOM) vanuit **EasyEDA**. Bestel de PCB's bij **JLCPCB** en maak gebruik van hun SMT-assemblageservice voor de Startbediening en de Lineaire Sensor PCB.
2. **Mechaniek:** Bestel of 3D-print de volgende onderdelen op basis van de bijgevoegde STL-bestanden:
* `Wiel1.STL` & `Mal voor wiel 2.STL` (Wielen en gietmallen).
* `Trechter.STL` (onderdeel om EDF30 te ondersteunen).
* `staaf.STL` (Structurele verbindingen).



### Stap 2: Mechanische Voorbereiding

1. **Wielen:** Gebruik de 3D-geprinte mallen (`Mal voor wiel 2.STL`) om rubberen of siliconen banden te gieten rondom `Wiel1.STL` voor maximale grip.
2. **Frame:** Monteer de `staaf.STL` onderdelen op het chassis van de robot om de basisstructuur te vormen.
3. **Behuizing:** Plaats de `Trechter.STL` op de voorziene positie voor als frame ondersteuning voor de EDF30 mm.

### Stap 3: Assemblage van de Elektronica

1. **Sensormodule:** Bevestig de kant-en-klare **Lineaire Sensor PCB** aan de voorzijde van de robot, zo dicht mogelijk bij de grond.
2. **Masterbediening:** Monteer de **Startbediening PCB** op een goed bereikbare plaats aan de bovenzijde.
3. **Microcontrollers:** Plaats de twee **Arduino RF Nano V3.0** modules in de headers van de PCB's (indien niet reeds meegeleverd door JLCPCB).

### Stap 4: Bedrading

1. Verbind de motoren met de motordriver-uitgangen op de Slave-unit.
2. Sluit de batterij (bijv. 11.1V LiPo) aan op de voedingsingang van de Master/Slave.
3. Controleer de gedeelde **GND** (massa) over het gehele systeem.

### Stap 5: Compileren en Uploaden

1. **Voorbereiding:** Installeer de `RF24` bibliotheek in de Arduino IDE.
2. **Master (Bediening):** * Open de Master-code.
* Selecteer Board: **Arduino Nano** | Processor: **ATmega328P (Old Bootloader)**.
* Upload via USB naar de Master RF Nano.


3. **Slave (Robot):** * Open de Slave-code.
* Herhaal de instellingen en upload naar de Slave RF Nano aan de onderzijde van de robot.



### Stap 6: Kalibratie en Testen

1. Zet de robot op een witte ondergrond met een zwarte lijn.
2. Druk op de **START** knop op de Masterbediening. De RF-verbinding activeert de Slave.
3. Controleer via de Seriële Monitor of de **CD74HC4067 multiplexer** de sensordata correct doorgeeft (waarden tussen 0-1023).
4. Gebruik de **STOP** knop voor een onmiddellijke interrupt-gebaseerde stilstand.
