#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <U8g2lib.h>

//Classes pour refactorisation
#include "ViseurAutomatique.h"
#include "Alarm.h"

#define MOTOR_INTERFACE_TYPE 4

//LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long previousLcdMillis = 0;
const unsigned long lcdInterval = 100;

U8G2_MAX7219_8X8_F_4W_SW_SPI u8g2(U8G2_R0, 30, 34, 32, U8X8_PIN_NONE);

const int trigPin = 9;
const int echoPin = 8;
long duration;
float distanceCm;
unsigned long previousDistMillis = 0;
const unsigned long distInterval = 50;

//Distances limites
unsigned int shorterDistance = 30;
unsigned int longerDistance = 60;

//Info labo
const String numDA = "6291623";
const String laboName = "Labo 6";

#define IN_1 4
#define IN_2 5
#define IN_3 6
#define IN_4 7

// Moteur pas à pas
const int stepsPerRevolution = 2048;
const int minAngle = 10;
const int maxAngle = 170;

// Alarm et LED RGB
const int buzzerPin = 10;
const int redPin = 11;
const int bluePin = 12;
const unsigned long gyroToggleInterval = 100;
const unsigned long gyroToggleTime = 3000;
int minDistanceGyro = 15;

bool limitsAreValid = true;
const int symbolsDelay = 3000;
const int lcdDelay = 2000;

ViseurAutomatique visor(IN_1, IN_2, IN_3, IN_4, distanceCm);
Alarm alarm(redPin, 0, bluePin, buzzerPin, distanceCm);

//Setup
void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.clear();
  lcdStartup();

  u8g2.begin();
  u8g2.setFont(u8g2_font_open_iconic_check_1x_t);

  visor.setAngleMin(minAngle);
  visor.setAngleMax(maxAngle);
  visor.setPasParTour(stepsPerRevolution);
  visor.setDistanceMinSuivi(shorterDistance);
  visor.setDistanceMaxSuivi(longerDistance);
  visor.activer();

  alarm.setColourA(255, 0, 0);
  alarm.setColourB(0, 0, 255);
  alarm.setVariationTiming(gyroToggleInterval);
  alarm.setDistance(minDistanceGyro);
  alarm.setTimeout(gyroToggleTime);
}

//Fonction démarrage pour LCD
void lcdStartup() {
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(numDA);
  lcd.setCursor(0, 1);
  lcd.print(laboName);
  delay(lcdDelay);
}

//Fonction pour la distance
void distTask() {
  if (millis() - previousDistMillis >= distInterval) {
    previousDistMillis = millis();

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);
    distanceCm = duration * 0.034 / 2;
  }
}

//Fonction pour le LCD
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
      lcd.print((int)visor.getAngle());
      lcd.print(" deg");
    }
  }
}

//Fonction pour commande serial monitor
void parseCommand(const String& input, String& command, String& arg1, String& arg2) {
  int i1 = input.indexOf(';');
  int i2 = input.indexOf(';', i1 + 1);

  if (i1 == -1) {
    command = input;
    return;
  }

  command = input.substring(0, i1);
  if (i2 != -1) {
    arg1 = input.substring(i1 + 1, i2);
    arg2 = input.substring(i2 + 1);
  } else {
    arg1 = input.substring(i1 + 1);
  }
}

//Fonction pour le serial monitor
void serialTask() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    String command, arg1 ,arg2;
    parseCommand(input, command, arg1, arg2);

    if (input.equalsIgnoreCase("gDist")) {
      Serial.println((int)distanceCm);
      showConfirmationSymbol();
      return;

    } else if (command == "cfg" && arg1 == "alm") {
      minDistanceGyro = arg2.toInt();
      alarm.setDistance(minDistanceGyro);
      Serial.println("Configure la distance de détection de l'alarme à " + String(minDistanceGyro) + " cm");
      showConfirmationSymbol();

    } else if (command == "cfg" && (arg1 == "lim_inf" || arg1 == "lim_sup")) {
      int val = arg2.toInt();

      if (arg1 == "lim_inf") {
        if (val >= longerDistance) {
          Serial.println("Erreur - Limite inférieure >= limite supérieure");
          showForbiddenSymbol();
          limitsAreValid = false;
        } else {
          shorterDistance = val;
          visor.setDistanceMinSuivi(shorterDistance);
          Serial.println("Limite inférieure définie à " + String(val) + " cm");
          showConfirmationSymbol();
          limitsAreValid = true;
        }

      } else if (arg1 == "lim_sup") {
        if (val <= shorterDistance) {
          Serial.println("Erreur - Limite supérieure <= limite inférieure");
          showForbiddenSymbol();
          limitsAreValid = false;
        } else {
          longerDistance = val;
          visor.setDistanceMaxSuivi(longerDistance);
          Serial.println("Limite supérieure définie à " + String(val) + " cm");
          showConfirmationSymbol();
          limitsAreValid = true;
        }
      }

    } else if (input.equalsIgnoreCase("testAlarm")) {
      alarm.test();
      Serial.println("Test de l'alarme lancé !");
      showConfirmationSymbol();

    } else if (command == "alm" && arg1 == "on") {
      alarm.turnOn();
      Serial.println("Alarme activée.");
      showConfirmationSymbol();

    } else if (command == "alm" && arg1 == "off") {
      alarm.turnOff();
      Serial.println("Alarme désactivée.");
      showConfirmationSymbol();

    } else {
      showErrorSymbol();
    }
  }
}


void showConfirmationSymbol() {
  u8g2.clearBuffer();
  u8g2.drawLine(1, 4, 3, 6);
  u8g2.drawLine(3, 6, 6, 3);
  u8g2.sendBuffer();
  delay(symbolsDelay);
  u8g2.clear();
}

void showErrorSymbol() {
  u8g2.clearBuffer();
  u8g2.drawLine(1, 1, 6, 6);
  u8g2.drawLine(6, 1, 1, 6);
  u8g2.sendBuffer();
  delay(symbolsDelay);
  u8g2.clear();
}

void showForbiddenSymbol() {
  u8g2.clearBuffer();
  u8g2.drawCircle(3, 3, 3, U8G2_DRAW_ALL);
  u8g2.drawLine(1, 5, 5, 1);
  u8g2.sendBuffer();
  delay(symbolsDelay);
  u8g2.clear();
}

void loop() {
  distTask();
  visor.update();
  alarm.update();
  lcdTask();
  serialTask();
}
