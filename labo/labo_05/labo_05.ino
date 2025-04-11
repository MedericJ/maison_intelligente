#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>

#define MOTOR_INTERFACE_TYPE 4

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int trigPin = 9;
const int echoPin = 8;
long duration;
float distanceCm;
unsigned long previousDistMillis = 0;
const unsigned long distInterval = 50;

unsigned int shorterDistance = 30;
unsigned int longerDistance = 60;

const String numDA = "6291623";
const String laboName = "Labo 4B";

#define IN_1 4
#define IN_2 5
#define IN_3 6
#define IN_4 7

AccelStepper myStepper(MOTOR_INTERFACE_TYPE, IN_1, IN_3, IN_2, IN_4);

const int stepsPerRevolution = 2048;
int motorAngle = 90;
int targetAngle = 90;
long targetStep = 0;
const int minAngle = 10;
const int maxAngle = 170;
const int stepsPerDegree = stepsPerRevolution / 360;

unsigned long previousSerialMillis = 0;
const unsigned long serialInterval = 100;

unsigned long previousLcdMillis = 0;
const unsigned long lcdInterval = 100;

//Alarm et LED RGB
const int buzzerPin = 10;
const int redPin = 11;
const int bluePin = 12;
bool alarmActive = false;
unsigned long lastDetectionTime = 0;
unsigned long gyroLastToggleTime = 0;
bool isRed = false;
const unsigned long gyroToggleInterval = 100;
const unsigned long gyroToggleTime = 3000;
const int minDistanceGyro = 15;
const int buzzerSound1 = 1000;
const int buzzerSound2 = 200;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.clear();

  myStepper.setMaxSpeed(500);
  myStepper.setAcceleration(100);

  lcdStartup();

  pinMode(buzzerPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(redPin, LOW);
  digitalWrite(bluePin, LOW);
}

void lcdStartup() {
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(numDA);
  lcd.setCursor(0, 1);
  lcd.print(laboName);
  delay(2000);
}

//Fonction pour la distance
void distTask() {
  if (millis() - previousDistMillis >= distInterval) {
    previousDistMillis = millis();

    digitalWrite(trigPin, LOW);
    digitalWrite(trigPin, HIGH);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);

    distanceCm = duration * 0.034 / 2;
  }
}

//Fonction pour les états
void stateManager() {
  if (distanceCm < shorterDistance) {
    motorAngle = -1;
  } else if (distanceCm > longerDistance) {
    motorAngle = -2;
  } else {
    targetAngle = map(distanceCm, shorterDistance, longerDistance, minAngle, maxAngle);
    targetStep = targetAngle * stepsPerDegree;

    if (myStepper.currentPosition() != targetStep) {
      myStepper.moveTo(targetStep);
    }
  }
  myStepper.run();
}

//Fonction pour le serial monitor
void serialTask() {
  if (millis() - previousSerialMillis >= serialInterval) {
    previousSerialMillis = millis();
    Serial.print("etd:");
    Serial.print(numDA);
    Serial.print(",dist:");
    Serial.print((int)distanceCm);
    Serial.print(",deg:");

    if (motorAngle == -1) Serial.println("Trop pres");
    else if (motorAngle == -2) Serial.println("Trop loin");
    else Serial.println(motorAngle);
  }
}

//Fonction pour l'affichage lcd
void lcdTask() {
  if (millis() - previousLcdMillis >= lcdInterval) {
    previousLcdMillis = millis();
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Dist : ");
    lcd.print((int)distanceCm);
    lcd.print(" cm");

    lcd.setCursor(0, 1);
    if (distanceCm <= shorterDistance) {
      lcd.print("Obj  : Trop pres");
    } else if (distanceCm >= longerDistance) {
      lcd.print("Obj  : Trop loin");
    } else {
      lcd.print("Obj  : ");
      lcd.print(targetAngle);
      lcd.print(" deg");
    }
  }
}

//Fonction pour buzzer et LED RGB
void alarmTask() {
  unsigned long currentTime = millis();

  if (distanceCm <= minDistanceGyro) {
    lastDetectionTime = currentTime;
    alarmActive = true;

  } else if (currentTime - lastDetectionTime >= gyroToggleTime) {
    alarmActive = false;
  }

  if (alarmActive) {
    tone(buzzerPin, buzzerSound1, buzzerSound2);

    if (currentTime - gyroLastToggleTime >= gyroToggleInterval) {
      gyroLastToggleTime = currentTime;
      isRed = !isRed;
      digitalWrite(redPin, isRed ? HIGH : LOW);
      digitalWrite(bluePin, isRed ? LOW : HIGH);
    }

  } else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(redPin, LOW);
    digitalWrite(bluePin, LOW);
  }
}

void loop() {
  distTask();
  stateManager();
  serialTask();
  lcdTask();
  alarmTask();
}
