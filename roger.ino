#include <RogerEnum.h>

#include <NewPing.h>
#include <NewTone.h>
#include <NewServo.h>

int outputPins[] = {9, 10, 11, 12, 13};  // Definer pins til de 5 lysene
int buzzerPin = 7;
int servoPinLeft = 6;
int servoPinRight = 3;
int sensorTrigPinLeft = 4;
int sensorEchoPinLeft = 5;
int sensorTrigPinRight = 2;
int sensorEchoPinRight = 8;
int buttonPin = A0;

// Ledige pins: 1, A0-5

int regularDelay = 500;  // For hvert lys skal vi vente et halvt sekund før vi går videre, altså 500ms
int startFreq = 400;     // Lyden spiller av 400Hz

bool enabled = false;
bool hasStarted = true;
bool enableLogging = false;
int previousButtonState = HIGH;

unsigned int cm, cmLeft, cmRight;
unsigned int maxCm = 35;
unsigned int sensorLED = 0;

int servoValueLeft, servoValueRight;

// Tidsforskjell variabler for å senere måle tid
unsigned long startAvoid, startReverse, startBeep, startEndTurn;
unsigned int maxAvoidTime = 8000, reverseTime = 2000, keepTurning = 1000;

// Tiden mellom hvert pip mens boten kjører i klar bane
unsigned int beepInterval = 1000, beepDuration = 100;

NewServo servoLeft;
NewServo servoRight;
NewPing sonarLeft(sensorTrigPinLeft, sensorEchoPinLeft, maxCm);
NewPing sonarRight(sensorTrigPinRight, sensorEchoPinRight, maxCm);

State state = Drive;
DriveState dir;

void setup() {
  // put your setup code here, to run once:
  if (enableLogging) {
    Serial.begin(9600);
  }

  // Åpne pin til buzzer (lyden)
  pinMode(buzzerPin, OUTPUT);

  // Åpne pin til knapp
  pinMode(buttonPin, INPUT);

  // Åpne pins til servo
  servoLeft.attach(servoPinLeft);
  servoRight.attach(servoPinRight);

  // Åpne alle definerte pins til lys
  for (int i = 0; i < sizeof(outputPins); i++) {
    int pin = outputPins[i];  // Definer pin verdien vi skal endre på
    pinMode(pin, OUTPUT);
  }

  // Sett opp pins til sensor
  pinMode(sensorTrigPinLeft, OUTPUT);  // Trigger pin skal starte måling av sensor, så vi sender output til denne
  pinMode(sensorEchoPinLeft, INPUT);   // Echo pin sender strøm i input når sensoren får signalet tilbake
  pinMode(sensorTrigPinRight, OUTPUT);
  pinMode(sensorEchoPinRight, INPUT);
}

void startSequence() {
  // Gå gjennom alle pins, slå de på sekvensielt (og så av igjen), og slå på alle pins ved pin 13
  
  for (int i = 0; i < 5; i++) {
    int pin = outputPins[i];  // Definer pin verdien vi skal endre på

    digitalWrite(pin, HIGH);  // Slå på lyset

    // Dersom vi er på den siste pinnen, slå på alle lys og spill av en lengre lyd med en høyere oktav
    if (pin == 13) {
      NewTone(buzzerPin, startFreq * 2);

      // Slå på alle pins
      for (int j = 0; j < 5; j++) {
        digitalWrite(outputPins[j], HIGH);
      }

      // Vent lengre før vi slår av lyden
      delay(regularDelay * 2);
      noNewTone(buzzerPin);
    }

    // Ved alle andre pins skal vi slå lyset på, spill av en lyd, slå av lyden og så slå av lyset igjen
    else {
      NewTone(buzzerPin, startFreq);
      delay(regularDelay / 2);
      noNewTone(buzzerPin);
      delay(regularDelay / 2);
      digitalWrite(pin, LOW);
    }
  }

  for (int i = 0; i < 5; i++) {
    digitalWrite(outputPins[i], LOW);
  }
}

void handleButton() {
  // Les state som LOW eller HIGH fra analog pin (vi var tom for digitale pins)
  int buttonState = digitalRead(buttonPin);

  if (enableLogging) {
    Serial.print("Button state: ");
    Serial.println(digitalRead(buttonPin));
  }

  // Sjekk at knappen i forrige tidsrom ikke var slått på slik at vi kun sender signalet når vi slipper knappen
  if (previousButtonState == LOW && buttonState == HIGH) {
    if (enabled) {
      noNewTone(buzzerPin);
      for (int i = 0; i < 5; i++) {
        digitalWrite(outputPins[i], LOW);
      }
      enabled = false;
    }
    else {
      enabled = true;
      hasStarted = false;
    }
  }
  previousButtonState = buttonState;
}

void loop() {
  // put your main code here, to run repeatedly:

  // Sjekk knapp states 
  handleButton();

  // Ved start skal vi kjøre hele sekvensen definert i startSequence
  if (enabled) {
    if (!hasStarted) {
      startSequence();
      hasStarted = true;
      startBeep = millis();
    }
    else {
      roger();
    }
  }
}

unsigned long deltatime(unsigned long ms) {
  // Returner forskjellen mellom nå og definer sist måling
  return millis() - ms;
}

void drive(DriveState driveState) {
  // Siden servoene er omvendt plassert vil svinging kreve like verdier, mens framover og bakover krever verdier som er motsatte
  switch (driveState) {
    case Right: servoValueLeft = 0; servoValueRight = 0; break;
    case Left: servoValueLeft = 180; servoValueRight = 180; break;
    case Forward: servoValueLeft = 0; servoValueRight = 180; break;
    case Backward: servoValueLeft = 180; servoValueRight = 0; break;
  }

  // Roter hjulene i retningen som ønskes
  servoLeft.write(servoValueLeft);
  servoRight.write(servoValueRight);
}

unsigned int validateCm(unsigned int &cm) {
  // 0 er vanligvis utenfor rekkevidde, slik at vi kan definere 0 som max
  if (cm == 0) {
    cm = maxCm;
  }
}

unsigned int measureDistance() {
  // Mål distanse foran begge sensorer
  delay(10);  // Ikkje kast bort tia mi
  cmLeft = (unsigned int) sonarLeft.ping_cm();
  delay(10);
  cmRight = (unsigned int) sonarRight.ping_cm();

  // Konverter verdiene til max dersom de er 0 (MER INFO: validateCm)
  validateCm(cmLeft);
  validateCm(cmRight);

  // Logg hele skiten
  if (enableLogging) {
    Serial.print("(");
    Serial.print(cmLeft);
    Serial.print(", "); 
    Serial.print(cmRight);
    Serial.print(") -> ");
  }

  // Returner den minste verdien
  return min(cmLeft, cmRight);
}

void handleDrive() {
    if (sensorLED == 4) {
      if (deltatime(startBeep) < 40) {
        NewTone(buzzerPin, startFreq + cm * 3);
      }
      else if (deltatime(startBeep) > beepDuration + 40) {
        noNewTone(buzzerPin);
      }
    }
    else {
      NewTone(buzzerPin, pow(cm, 2) * 2);
    }
    
    drive(Forward);
}

void handleAvoid() {  
  // Sving unna. Retningen er definert i roger() funskjonen
  drive(dir);

  int delta = deltatime(startAvoid);

  // Bråk
  NewTone(buzzerPin, sqrt(delta) * 6);

  // Fortsett å kjøre når det grønne lyset er på (unngikk hinderet der)
  if (sensorLED == 4) {                                                                                   
    if (state == Avoid) {
      startEndTurn = millis();
      state = AvoidTurning;
    }
    if (deltatime(startEndTurn) > keepTurning) {
      state = Drive;
    }
  }
  else if ((state == Avoid) && (delta > maxAvoidTime))  {
    startReverse = millis();
    state = Reverse;
    noNewTone(buzzerPin);
  }
  else {
    if (state != Avoid) {
      state = Avoid;
    }
  }
}

void handleReverse() {
  int delta = (deltatime(startReverse));

  // Bråk v2
  NewTone(buzzerPin, (sqrt(reverseTime) * 6) - (sqrt(delta) * 6));

  // Kjør bakover til vi er ferdig
  if (delta < reverseTime) {
    drive(Backward);
  }
  else {
    state = Drive;
    noNewTone(buzzerPin);
  }
}

void roger() {
  cm = measureDistance();

  // Logg avstand til neste objekt dersom vi har slått på logging
  if (enableLogging) {
    Serial.println(cm);
  }

  // Slå av lyset som var slått på tidligere
  digitalWrite(outputPins[sensorLED], LOW);

  // Velg LED basert på avstand fra sensor
  sensorLED = map(cm, 1, maxCm, 0, 4);

  // Vent 2 µs og slå på lyset relativt til distanse
  delayMicroseconds(2);
  digitalWrite(outputPins[sensorLED], HIGH);

  // Reset beep timer om nødvendig
  if (deltatime(startBeep) > beepInterval) {
    startBeep = millis();
  }

  // Kjør framover dersom ingenting er nærmere enn 10cm, første gult lys
  // Endrer dermed til avoid state for å oppnå dette, men state endres kun tilbake i handleAvoid funksjonen
  if ((state == Drive) && (sensorLED <= 1)) {
    // Velg retning *når* vi begynner å svinge
    if (cmRight > cmLeft) {
      dir = Right;
    }
    else {
      dir = Left;
    }

    // Sett startAvoid slik at vi kan holde telling på tid fra Avoid startet
    startAvoid = millis();
    state = Avoid;
  }

  // Velg handling basert på state
  switch (state) {
    case Drive: handleDrive(); break;
    case Avoid: handleAvoid(); break;
    case AvoidTurning: handleAvoid(); break;
    case Reverse: handleReverse(); break;
  }
}

