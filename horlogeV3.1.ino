
//Pour le leds
#include "ShiftRegisterPWM.h"
ShiftRegisterPWM sr(2, 255); // first parameter number of 74HC second parameter resolution (from 0 to 255)
//Pour le RTC
#include "RTClib.h"
RTC_DS3231 rtc;
int RTCint=A3;

// pour suivre le temps
#include <TimeLib.h>



// pour le son
int HPpin = 5;
// pour le tilt
int Pintilt = 13;
float tiltaverage;
int nb_av=50;
int tiltValue;
bool eteint;
//pour lecapteur de lmumiere
int Lumsensor= A7;


// pour les boutons tactiles
#include <CapacitiveSensor.h>
CapacitiveSensor   Btop = CapacitiveSensor(8,A2);  // fleche haut      
CapacitiveSensor   Bmiddle = CapacitiveSensor(8,A1);  // rond       
CapacitiveSensor   Bbottom = CapacitiveSensor(8,A0);  //flechebas   
CapacitiveSensor   Bcross = CapacitiveSensor(8,12);  // croix


// diodes affichage
bool D0=true;
bool D1=false;
bool D2=false;
int lumon=255;
int lumoff=1;
int lastlumon=255;
int lastlumoff=1;


int lumino;
int n=10; // ponderation pour la moyenne de la mesure de lumière
unsigned long nowmili,lastime;
long tempslastmaj;  // variable permettant de stocker le moment de la mise a jour
int deltamaj=86400; // temps entre deux mise a jour de lhorloge en secondes


void setup() {
  // put your setup code here, to run once:
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
    while (! rtc.begin()) { Serial.println("Couldn't find RTC"); }
    //bipde start
      tone(HPpin , 440,100);      delay(120);      noTone(HPpin );      tone(HPpin , 349,100);      delay(120);      noTone(HPpin );      digitalWrite(HPpin, LOW);
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, lets set the time!");
      // If the RTC have lost power it will sets the RTC to the date & time this sketch was compiled in the following line
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    settime();
    lastime=millis();
    tempslastmaj=now();
}



void loop() {
  while (nowmili-lastime <1000){ // on attend une seconde
    testsensors();
    nowmili=millis();
    }
   leddisplay(); // on affiche sur le display
   lastime=nowmili;
   //disptime();
   
   if((now()-tempslastmaj)>deltamaj){
    tempslastmaj=now();
    settime();
   }



  
}
  // put your main code here, to run repeatedly:

//
void leddisplay(){
  D0=!D0;
  sr.set(9,D0*lumon+!D0*lumoff);
  sr.set(1,D1*lumon);
  sr.set(0,D2*lumon);
  // on gere les heures
  bool val=0b01 & hour();  sr.set(7,val*lumon+!val*lumoff);
  val=0b10 & hour();  sr.set(6,val*lumon+!val*lumoff);
  val=0b100 & hour();  sr.set(5,val*lumon+!val*lumoff);
  val=0b1000 & hour();  sr.set(4,val*lumon+!val*lumoff);
  val=0b10000 & hour();  sr.set(3,val*lumon+!val*lumoff);
  val=0b100000 & hour();  sr.set(2,val*lumon+!val*lumoff);
  // puis les minutes
  val=0b01 & minute();  sr.set(10,val*lumon+!val*lumoff);
  val=0b10 & minute();  sr.set(11,val*lumon+!val*lumoff);
  val=0b100 & minute();  sr.set(12,val*lumon+!val*lumoff);
  val=0b1000 & minute();  sr.set(13,val*lumon+!val*lumoff);
  val=0b10000 & minute();  sr.set(14,val*lumon+!val*lumoff);
  val=0b100000 & minute();  sr.set(15,val*lumon+!val*lumoff);
}
//
//
void settime(){
  DateTime now = rtc.now();
  setTime(now.hour(),now.minute(),now.second(),now.day(),now.month(),now.year());
}

void disprtctime(){
    DateTime now = rtc.now();
    Serial.println("temps RTC");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");
}

void disptime(){
  Serial.println("temps arduino");
    Serial.print(hour(), DEC);
    Serial.print('/');
    Serial.print(minute(), DEC);
    Serial.print('/');
    Serial.print(second(), DEC);
    Serial.print('/');
    Serial.print(year(), DEC);
    Serial.print('/');
    Serial.print(month(), DEC);
    Serial.print('/');
    Serial.println(day(), DEC);
}

void testsensors(){
  lumino = (lumino*(n-1) + analogRead(Lumsensor))/n; // on viens smoothe la valeur
    if(!eteint){
      if (lumino < 700){ lumon=12;}
      else{lumon=map(lumino,700,1023,12,255);}
    }
  
  // on test le tilt
   tiltaverage=(tiltaverage*nb_av+digitalRead(Pintilt))/(nb_av+1); // on fait une moyenne glissante pour eviter clignotmenet  
    if(!eteint && tiltaverage>0.6){
    lastlumon=lumon;
    lastlumoff=lumoff;
    lumon=0;
    lumoff=0;
    eteint=true;
    } // utilisation du tilt  
      if(eteint && tiltaverage < 0.4){
    eteint=false;  
    lumon=lastlumon;
    lumoff=lastlumoff;
  }
  
  



  
}
