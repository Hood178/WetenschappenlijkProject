# StepperController-documentatie

## Overzicht

`StepperController` is een eenvoudige Python-API die aan een Arduino Nano R4 vertelt hoe een TB6600 steppermotordriver moet worden aangestuurd. Je gebruikt deze om de motor te laten draaien, stoppen, van richting te laten veranderen en een gekozen aantal stappen te laten zetten, zonder dat je zelf met de lage-niveau I2C-details bezig hoeft te zijn.

In simpele woorden: Python stuurt opdrachten naar de Arduino, de Arduino stuurt pulssignalen naar de TB6600 en de TB6600 drijft daarna de steppermotor aan.

**Belangrijkste functies:**
- Motor op afstand bedienen via I2C
- Snelheid instellen in procenten of RPM
- Beweging instellen in stappen, graden of omwentelingen
- Continu laten draaien of voor een vast aantal stappen
- Controleren of de motor nog beweegt
- Automatisch en veilig afsluiten wanneer je `with` gebruikt

---

## Architectuur

### Communicatiemodel

De controller communiceert met een Arduino-slave via I2C met een registergebaseerde interface. Het slave-apparaat bewaart de toestand van de motor (ingeschakeld/uitgeschakeld, richting, snelheid, pulstelling) en verzorgt de timing voor het genereren van pulsen.

```
  Master (Python)                   I2C-bus                  Slave (Arduino)
┌──────────────────┐           ┌──────────────┐           ┌──────────────────┐
│ StepperController├───────────┤  I2C-bus     ├───────────┤ Arduino Nano R4  │
│                  │           │              │           │ + TB6600-driver  │
│ - API op hoog    │           │ Registermap  │           │ - Pulssignalen   │
│   niveau         │           │              │           │ - Pinbediening   │
└──────────────────┘           └──────────────┘           └──────────────────┘
```

### Registermap

Het slave-apparaat biedt de volgende I2C-registers aan:

| Adres | Naam | Type | Grootte | Doel |
|-------|------|------|---------|------|
| 0x00 | REG_ENABLE | R/W | 1 byte | Driver in- of uitschakelen (`0x00`=uitgeschakeld, `0x01`=ingeschakeld + beweging starten) |
| 0x01 | REG_DIRECTION | R/W | 1 byte | Richting van de motor (`0x00`=vooruit, `0x01`=achteruit) |
| 0x02 | REG_PERIOD_US_H | R/W | 1 byte | Hoogste byte van de stapperiode (deel van 16-bit big-endian waarde) |
| 0x03 | REG_PERIOD_US_L | R/W | 1 byte | Laagste byte van de stapperiode (gecombineerd: periode = (H << 8) | L, in µs) |
| 0x04 | REG_PCOUNT_H | R/W | 1 byte | Hoogste byte van de pulstelling (deel van 16-bit big-endian waarde) |
| 0x05 | REG_PCOUNT_L | R/W | 1 byte | Laagste byte van de pulstelling (gecombineerd: telling = (H << 8) | L) |
| 0x06 | MOTION_COMPLETE_FLAG | R | 1 byte | Bewegingsstatus (`0x00`=bezig, `0x01`=klaar/inactief) |

**Bewegingsmodi:**
- **Continu:** zet `REG_PCOUNT` op 0; de motor draait oneindig door totdat hij wordt uitgeschakeld
- **Finit:** zet `REG_PCOUNT` op de gewenste telling; de motor stopt na dat aantal pulsen

---

## Gebruiksvoorbeelden

### Basisinstelling

```python
from stepper_i2c.controller import StepperController

# Controller maken voor slave-adres 0x08 (basis + offset 0)
controller = StepperController(address=0, bus=1)

try:
    # Hier komt jouw besturingscode
    pass
finally:
    controller.close()
```

### Met contextmanager gebruiken (aanbevolen)

```python
from stepper_i2c.controller import StepperController

# Sluit de verbinding automatisch af bij het verlaten van het blok
with StepperController(address=0) as motor:
    motor.start()  # Blijf draaien op 50% snelheid, met de klok mee
    motor.stop()   # Zet de motor uit
```

### Snelheidsregeling

```python
with StepperController(address=0) as motor:
    # Snelheid instellen als percentage (0-100)
    motor.set_speed_percent(75.0)  # 75% van de maximale snelheid
    
    # Snelheid instellen in RPM
    motor.set_speed_rpm(300)  # 300 omwentelingen per minuut
    
    # Snelheid relatief aanpassen
    motor.change_speed(+10)  # 10% sneller
    motor.change_speed(-20)  # 20% trager
    
    # Huidige snelheid ophalen
    current_speed = motor.get_speed_percent()
    print(f"Huidige snelheid: {current_speed}%")
```

### Relatieve beweging (zonder positietracking)

```python
with StepperController(address=0) as motor:
    # Verplaatsen met een exact aantal stappen
    motor.move_steps(steps=100, speed_percent=50.0, clockwise=True)
    
    # Verplaatsen in graden (relatieve rotatie)
    motor.move_degrees(degrees=45.0, speed_percent=50.0, clockwise=True)
    
    # Draaien met volledige omwentelingen
    motor.rotate(revs=2.5, speed_percent=50.0, clockwise=False)
    
    # Continu draaien tot het stoppen
    motor.run_continuous(speed_percent=60.0, clockwise=True)
```

### Richting regelen

```python
with StepperController(address=0) as motor:
    # Met de klok mee draaien
    motor.set_direction(clockwise=True)
    motor.move_steps(100)
    
    # Tegen de klok in draaien
    motor.set_direction(clockwise=False)
    motor.move_steps(100)
```

### Beweging volgen

```python
with StepperController(address=0) as motor:
    # Controleren of de motor aan het bewegen is
    if motor.is_moving():
        print("Motor beweegt")
    else:
        print("Motor staat stil")
    
    # Wachten tot de beweging klaar is (met timeout)
    try:
        motor.move_steps(500, speed_percent=50)
        motor.wait_until_complete(timeout_sec=10.0)
        print("Beweging succesvol afgerond")
    except TimeoutError:
        print("Beweging werd niet binnen de timeout afgerond")
```

### Status van de controller lezen

```python
with StepperController(address=0) as motor:
    state = motor.get_state()
    print(f"Ingeschakeld: {state['enabled']}")
    print(f"Richting: {'CW' if state['clockwise'] else 'CCW'}")
    print(f"Snelheid: {state['speed_percent']}%")
    print(f"Pulstelling: {state['pulse_count']}")
    print(f"Continu-modus: {state['is_continuous']}")
    print(f"Periode (µs): {state['period_us']}")
```

---

## API-referentie

### Initialisatie

#### `__init__(address: int | str, bus: int = 1, steps_per_rev: int = 200, i2c_retry_count: int = 3, i2c_retry_delay: float = 0.05, i2c_retry_backoff: float = 2.0, invert: bool = False)`

Maak een nieuwe StepperController-instantie aan.

**Parameters:**
- `address` (int of str): Lage 4-bit I2C-adresoffset (0-15 of "0000"-"1111" binair). Eindadres I2C = basis (0x20) + offset
- `bus` (int, optioneel): Nummer van de I2C-bus. Standaard: 1
- `steps_per_rev` (int, optioneel): Aantal motostappen per volledige omwenteling. (kan worden ingesteld met schakelaars op de driver-module) Standaard: 200
- `i2c_retry_count` (int, optioneel): Aantal herpogingen bij tijdelijke I2C-fouten. Standaard: 3
- `i2c_retry_delay` (float, optioneel): Eerste vertraging in seconden vóór opnieuw proberen. Standaard: 0.05
- `i2c_retry_backoff` (float, optioneel): Vermenigvuldigingsfactor voor exponentiële vertraging tussen pogingen. Standaard: 2.0
- `invert` (bool, optioneel): Als dit `True` is, worden alle richtingsopdrachten omgekeerd. Handig wanneer de motor fysiek anders is gemonteerd (met de klok mee wordt tegen de klok in). Standaard: `False`

**Voorbeeld:**
```python
motor = StepperController(address=0, bus=1, steps_per_rev=200)
motor = StepperController(address="0101", bus=1)  # Binair adres
motor = StepperController(address=0, invert=True)  # Motor draait in omgekeerde richting
```

---

### Bewegingsregeling

#### `move_steps(steps: int, speed_percent: float = 50.0, clockwise: bool = True)`

Voer een beperkte beweging uit met een exact aantal stappen.

**Parameters:**
- `steps` (int): Aantal motorstappen dat moet worden uitgevoerd
- `speed_percent` (float): Snelheid als percentage (0-100). Standaard: 50%
- `clockwise` (bool): `True` voor met de klok mee, `False` voor tegen de klok in. Standaard: `True`

**Fouten:**
- `ValueError`: Als de parameters ongeldig zijn

**Voorbeeld:**
```python
motor.move_steps(steps=400, speed_percent=75, clockwise=True)
```

---

#### `move_degrees(degrees: float, speed_percent: float = 50.0, clockwise: bool = True)`

Voer een beweging uit in graden rotatie.

**Parameters:**
- `degrees` (float): Aantal graden dat moet worden gedraaid
- `speed_percent` (float): Snelheid als percentage (0-100). Standaard: 50%
- `clockwise` (bool): Rotatierichting. Standaard: `True`

**Voorbeeld:**
```python
motor.move_degrees(degrees=180, speed_percent=50)  # Halve omwenteling
```

---

#### `rotate(revs: float, speed_percent: float = 50.0, clockwise: bool = True)`

Draai een aantal volledige omwentelingen.

**Parameters:**
- `revs` (float): Aantal omwentelingen (mag ook een fractie zijn)
- `speed_percent` (float): Snelheid als percentage (0-100). Standaard: 50%
- `clockwise` (bool): Rotatierichting. Standaard: `True`

**Voorbeeld:**
```python
motor.rotate(revs=2.5, speed_percent=60)  # 2,5 volledige omwentelingen
```

---

#### `run_continuous(speed_percent: float = 50.0, clockwise: bool = True)`

Start continu draaien totdat `stop()` wordt aangeroepen.

**Parameters:**
- `speed_percent` (float): Snelheid als percentage (0-100). Standaard: 50%
- `clockwise` (bool): Rotatierichting. Standaard: `True`

**Voorbeeld:**
```python
motor.run_continuous(speed_percent=75, clockwise=True)
# Motor draait nu oneindig door...
motor.stop()  # Motor stoppen
```

---

#### `start()`

Start de motor met standaardinstellingen (50% snelheid, met de klok mee, continu).

**Voorbeeld:**
```python
motor.start()
motor.stop()
```

---

#### `stop()`

Stop de motor onmiddellijk door de driver-uitgang uit te schakelen.

**Voorbeeld:**
```python
motor.stop()
```

---

### Snelheidsregeling

#### `set_speed_percent(speed_percent: float)`

Stel de motorsnelheid in als percentage van de maximale snelheid.

**Parameters:**
- `speed_percent` (float): Snelheidspercentage (0-100)
  - 0% = traagst (maximale stapperiode = 65535 µs)
  - 100% = snelst (minimale stapperiode = 1000 µs)

**Voorbeeld:**
```python
motor.set_speed_percent(50)   # 50% snelheid
motor.set_speed_percent(100)  # Maximale snelheid
```

---

#### `set_speed_rpm(rpm: float)`

Stel de motorsnelheid in in omwentelingen per minuut.

**Parameters:**
- `rpm` (float): Gewenste RPM (moet > 0 zijn)

**Fouten:**
- `ValueError`: Als RPM ≤ 0 is

**Voorbeeld:**
```python
motor.set_speed_rpm(300)  # 300 RPM
```

---

#### `change_speed(delta_percent: float)`

Pas de snelheid aan met een relatief percentage van de huidige snelheid.

**Parameters:**
- `delta_percent` (float): Snelheidsverandering (-100 tot +100)
  - Positief: sneller
  - Negatief: trager

**Voorbeeld:**
```python
motor.change_speed(+20)   # 20% sneller
motor.change_speed(-10)   # 10% trager
```

---

#### `get_speed_percent() -> float`

Geef de huidige motorsnelheid terug als percentage.

**Retourwaarde:** Huidig snelheidspercentage (0-100)

**Voorbeeld:**
```python
speed = motor.get_speed_percent()
print(f"Huidige snelheid: {speed}%")
```

---

### Richtingsregeling

#### `set_direction(clockwise: bool)`

Stel de rotatierichting van de motor in.

**Parameters:**
- `clockwise` (bool): `True` voor met de klok mee, `False` voor tegen de klok in. Als `invert=True` is ingesteld tijdens initialisatie, wordt deze richting automatisch omgedraaid.

**Voorbeeld:**
```python
motor.set_direction(clockwise=True)   # Met de klok mee (of tegen de klok in als invert=True)
motor.set_direction(clockwise=False)  # Tegen de klok in (of met de klok mee als invert=True)

# Met invert-vlag:
motor_inverted = StepperController(address=0, invert=True)
motor_inverted.set_direction(clockwise=True)  # Motor draait feitelijk tegen de klok in
```

---

### Lage-niveau bediening

#### `enable(state: bool)`

Schakel de stepperdriver-uitgang in of uit.

**Parameters:**
- `state` (bool): `True` om in te schakelen, `False` om uit te schakelen

**Voorbeeld:**
```python
motor.enable(True)   # Driver inschakelen
motor.enable(False)  # Driver uitschakelen
```

---

### Statusbewaking

#### `get_state() -> dict`

Lees de volledige toestand uit van het slave-apparaat.

**Retourwaarde:** Woordenboek met sleutels:
- `enabled` (bool): Driver is ingeschakeld
- `clockwise` (bool): Rotatierichting
- `period_us` (int): Stapperiode in microseconden
- `speed_percent` (float): Snelheid als percentage
- `pulse_count` (int): Ingestelde pulstelling
- `is_continuous` (bool): Of de motor in continu-modus staat

**Voorbeeld:**
```python
state = motor.get_state()
if state['is_continuous']:
    print("Draait in continu-modus")
```

---

#### `is_moving() -> bool`

Controleer of de motor momenteel een beweging uitvoert.

**Retourwaarde:** `True` als de motor ingeschakeld is en de beweging nog niet klaar is, anders `False`

**Voorbeeld:**
```python
while motor.is_moving():
    print("Nog steeds aan het bewegen...")
    time.sleep(0.1)
```

---

#### `wait_until_complete(timeout_sec: float = 30.0) -> bool`

Blokkeer de uitvoering totdat de huidige beweging klaar is of totdat de timeout is bereikt.

**Parameters:**
- `timeout_sec` (float): Maximum aantal seconden om te wachten. Standaard: 30

**Retourwaarde:** `True` als de beweging binnen de timeout klaar is

**Fouten:**
- `TimeoutError`: Als de beweging niet binnen de timeout klaar is

**Voorbeeld:**
```python
motor.move_steps(500)
try:
    motor.wait_until_complete(timeout_sec=10)
    print("Beweging voltooid!")
except TimeoutError:
    print("Timeout bij beweging!")
```

---

### Bronbeheer

#### `close()`

Sluit de I2C-busverbinding en geef de bronnen vrij.

**Voorbeeld:**
```python
motor.close()  # Altijd aanroepen wanneer je klaar bent
```

---

#### Contextmanager: `__enter__()` en `__exit__()`

Gebruik de controller als contextmanager voor automatisch bronbeheer.

**Voorbeeld:**
```python
with StepperController(address=0) as motor:
    motor.move_steps(100)
    # Verbinding wordt hier automatisch gesloten
```

---

## Constanten

De constanten zijn gedefinieerd in `stepper_i2c/constants.py`:
Je hoeft deze normaal niet te wijzigen, maar het kan wel als je dat echt wilt.

```python
BASE_I2C_ADDRESS = 0x20  # Basis-I2C-adres
I2C_BUS = 1              # Standaard I2C-busnummer
STEPS_PER_REV = 200      # Standaard aantal stappen per omwenteling
MIN_PERIOD_US = 1000     # Minimale stapperiode (snelste snelheid)
MAX_PERIOD_US = 65535    # Maximale stapperiode (traagste snelheid)
```

---

## Belangrijke opmerkingen

### Geen positietracking

Deze controller houdt **geen absolute positie** bij. Hij ondersteunt alleen **relatieve beweging**:
- ✅ 100 stappen vooruit bewegen
- ✅ 45 graden met de klok mee draaien
- ✅ Continu draaien tot stoppen
- ❌ Naar een absolute hoek van 85° gaan
- ❌ De huidige positie opvragen

Alle beweging is dus **relatief** ten opzichte van waar de motor op dat moment staat.

### Snelheidsmapping

Het snelheidspercentage wordt omgekeerd gekoppeld aan de stapperiode:
- **0%** = traagste beweging (periode = 65535 µs)
- **50%** = middensnelheid (periode ≈ 33267 µs)
- **100%** = snelste beweging (periode = 1000 µs)

### Richtingsomkering

Als jouw motor fysiek anders gemonteerd is en je wilt dat alle opdrachten in de omgekeerde richting lopen, gebruik dan de parameter `invert`:

```python
# Motor draait tegenovergesteld aan de natuurlijke oriëntatie
motor = StepperController(address=0, invert=True)
motor.set_direction(clockwise=True)    # Motor draait feitelijk tegen de klok in
motor.move_degrees(90, clockwise=True) # Draait -90 graden in plaats van +90
```

Deze omkering geldt voor **alle richtingsgerelateerde opdrachten**:
- `set_direction()`
- `move_steps()`
- `move_degrees()`
- `rotate()`
- `run_continuous()`

### I2C-adressering

Het eind-I2C-adres wordt als volgt berekend:
```
Final Address = BASE_I2C_ADDRESS | (address & 0x0F)
```

Bijvoorbeeld:
- `StepperController(address=0)` maakt verbinding met I2C-adres `0x20`
- `StepperController(address="1000")` maakt verbinding met I2C-adres `0x28` (0x20 | 0x08)
- `StepperController(address=15)` maakt verbinding met I2C-adres `0x2F` (0x20 | 0x0F)

### Bewegingsvoltooiing

Voor beperkte bewegingen kun je de voltooiing volgen met:
```python
motor.move_steps(500)
while motor.is_moving():
    time.sleep(0.01)
print("Beweging klaar!")
```

Of gebruik de blokkerende methode:
```python
motor.move_steps(500)
motor.wait_until_complete(timeout_sec=10)
```

---

## Problemen oplossen

### Motor reageert niet

1. Controleer de I2C-buscommunicatie: `i2cdetect -y 1`
2. Controleer of het juiste busnummer is gekozen (meestal 1)
3. Controleer of de juiste adresoffset is ingesteld (DIP-schakelaars op de Arduino)
4. Controleer de fysieke I2C-aansluitingen (SDA/SCL)

### Beweging te snel/te langzaam

Pas de snelheid aan met `set_speed_percent()` of `set_speed_rpm()`. Let op dat zeer hoge snelheden (>90%) of zeer lage snelheden (<10%) onbetrouwbaar kunnen zijn.

### Timeout-fouten

Als `wait_until_complete()` een timeout geeft, vergroot dan de timeoutwaarde of controleer of de motor mechanisch vastzit.

---