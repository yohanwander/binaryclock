/**
  * Library for PWM control of the 74HC595 shift register.
  * Created by Timo Denk (www.timodenk.com), 2017.
  * Additional information is available at https://timodenk.com/blog/shiftregister-pwm-library/
  * Released into the public domain.
  */
//Pour le leds
#include "ShiftRegisterPWM.h"
ShiftRegisterPWM sr(2, 255); // first parameter number of 74HC second parameter resolution (from 0 to 255)
//Pour le RTC
#include "RTClib.h"
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
bool RTC=true;

// pour le son
int HPpin = 5;
// pour le tilt
int Pintilt = 13;
//pour lecapteur de lmumiere
int Lumsensor= A7;


// pour les boutons tactiles
#include <CapacitiveSensor.h>
CapacitiveSensor   Btop = CapacitiveSensor(8,A2);  // fleche haut      
CapacitiveSensor   Bmiddle = CapacitiveSensor(8,A1);  // rond       
CapacitiveSensor   Bbottom = CapacitiveSensor(8,A0);  //flechebas   
CapacitiveSensor   Bcross = CapacitiveSensor(8,12);  // croix



void setup()
{
  // on ouvre le port serie:
  Serial.begin(9600);  
  //on decalre la sortie du haut oparleur en output
  pinMode(HPpin, OUTPUT); 
  digitalWrite(HPpin, LOW); //on evite le gresillement
  pinMode(Pintilt,INPUT_PULLUP);
  pinMode(Lumsensor,INPUT); 


  // setup of the 74HC595
  pinMode(2, OUTPUT); // sr data pin
  pinMode(3, OUTPUT); // sr clock pin
  pinMode(4, OUTPUT); // sr latch pin
  sr.interrupt(ShiftRegisterPWM::UpdateFrequency::Fast);

  // setup du RTC
    if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    RTC=false;
    }

  
}

void loop(){

// test du son 

tone(HPpin , 440,250);
delay(300);
noTone(HPpin );
tone(HPpin , 349,250);
delay(300);
noTone(HPpin );
digitalWrite(HPpin, LOW);
  
// test des leds 
    for (uint8_t j = 0; j < 16; j++) { // on parcours les leds une a une
      for (uint8_t i = 0; i < 16; i++) {sr.set(i, 0);}; // on etein toutes les leds
      sr.set(j,128); // on allume juste la j 
      Serial.print( "test de la led n ");
      Serial.println(j);
      delay(200);
    }
      Serial.print( "fading test ");
    for (uint8_t j = 0; j < 255; j++) { // on parcours les leds une a une
      for (uint8_t i = 0; i < 16; i++) { // on etein toutes les leds
      sr.set(i,j); // on allume juste la j 
    }
    delay(1);
    }

// test du RTC
  if (RTC) {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, lets set the time!");
      // If the RTC have lost power it will sets the RTC to the date & time this sketch was compiled in the following line
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    DateTime now = rtc.now();
      Serial.print(now.year(), DEC);
      Serial.print('/');
      Serial.print(now.month(), DEC);
      Serial.print('/');
      Serial.print(now.day(), DEC);
      Serial.print(" (");
      Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
      Serial.print(") ");
      Serial.print(now.hour(), DEC);
      Serial.print(':');
      Serial.print(now.minute(), DEC);
      Serial.print(':');
      Serial.print(now.second(), DEC);
      Serial.println();
      Serial.print("Temperature: ");
      Serial.print(rtc.getTemperature());
      Serial.println(" C");
      Serial.println();
  }
 // test du tilit
  for (uint8_t j = 0; j < 300; j++){
    Serial.print("tiltvalue ");
    Serial.println( digitalRead(Pintilt));
    Serial.print("lum sensor value ");
    Serial.println( analogRead(Lumsensor));
    delay(100);
    }  

//// test des capterus capacitifs.
// long tstart=millis();
// long attente=100000;
// while(millis()-tstart< attente){
//    long total1 =  Btop.capacitiveSensor(30);
//    long total2 =  Bbottom.capacitiveSensor(30);
//    long total3 =  Bmiddle.capacitiveSensor(30);
//    long total4 =  Bcross.capacitiveSensor(30);
//
//    Serial.print(total1);  Serial.print("\t");                // print sensor output 1
//    Serial.print(total2);  Serial.print("\t");            // print sensor output 2
//    Serial.print(total3);  Serial.print("\t");            // print sensor output 3
//    Serial.println(total4);                                // print sensor output 3
//
//    delay(1);                             // arbitrary delay to limit data to serial port 
// }
}
    
