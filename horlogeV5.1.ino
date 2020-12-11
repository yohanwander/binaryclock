// programme de gestion de lhorloge V4
//05/11/2020 Yohan Wanderoild
/*
 *changement du temps de captage tactile
 *14 melodies
*/


// decommenter la ligne suivante pour renvoyer les données sur le port serie
//#define debugging

//DEclaration des shift register pour les leds
#include "ShiftRegisterPWM.h"
ShiftRegisterPWM sr(2, 255); // first parameter number of 74HC second parameter resolution (from 0 to 255)
// diodes affichage
bool D0=true; // diode affichage secondes
// gestion de la luminosité des leds
int lumon=255;
int lumoff=0; // luminosité de la led eteinte
int lastlumon=255;
int lastlumoff=1;

//Pour le RTC
#include "RTClib.h"
RTC_DS3231 rtc;
int RTCint=A3;

// on utilise une libvrairie interne pour suivre le temps sans interroger le RTC en permanance
#include <TimeLib.h>
long tempslastmaj;  // variable permettant de stocker le moment de la mise a jour
int deltamaj=86400; // temps entre deux mise a jour de lhorloge en secondes

// pour le son
int HPpin = 5;
#include "pitches.h"
#define REST      0
byte musicnumber=1;

// pour le tilt
int Pintilt = 11;
float tiltaverage;
int nb_av=3;// ponderation de la moyenne glissante pour le tilt, plus le chiffre est grand plus les leds vont mettre du temps a s'eteindre...
int tiltValue;
bool tilt_on; // si tilt_on on eteint

//pour lecapteur de lmumiere
int Lumsensor= A7;
int lumino;
int n=10; // ponderation pour la moyenne de la mesure de lumière
unsigned long nowmili,lastime;

// pour les boutons tactiles
#include <CapacitiveSensor.h>
CapacitiveSensor   Btop = CapacitiveSensor(8,A0);  // fleche haut      
CapacitiveSensor   Bmiddle = CapacitiveSensor(8,A1);  // rond       
CapacitiveSensor   Bbottom = CapacitiveSensor(8,A2);  //flechebas   
CapacitiveSensor   Bcross = CapacitiveSensor(8,12);  // croix
int trigB=90,numsample=8; // valeur a augmenter si les boutons sont trop sensibles
bool atop,abot,amiddle,across; // variable permettant de savoir si les boutons onté été appuyés elles sont mis a jour par refreshbutton
unsigned long tempsappuilong = 1500,tempsretourmenu=120000; ///(1seconde 5 et 120 secondes

// gestion des etats
enum State_enum{NORMAL,CHANGEHOUR1,CHANGEHOUR2,CHANGEHOUR3,CHANGEHOUR4,CHANGEHOUR5,CHANGEHOUR6,DISPLAYALARM1,DISPLAYALARM2,ALARM1,ALARM2,MUSICCHOICE}; // etats mpossible dans la machine a état
uint8_t state = NORMAL; // la variable state permet de savoir dans quel état on est

unsigned long amiddlepressed, acrosspressed, tempssortienormal;// variable permettant de noter le temps ou le bouton O ou croix a commencé a être maintenu
bool amiddlecounting=false,acrosscounting=false; // permet de savoir si le bouton rond ou croix a été maintenu appuyé ou relaché
bool amiddlereleased=false,acrossreleased=false;
struct  { 
  byte Minute; 
  byte Hour; 
}   alarm1, alarm2, Newclock;
uint8_t countalarm=0; //compte le nombre de fois ou la musique est jouée pour l'larme
bool ALARM1_ON=false, ALARM2_ON=false;
// on configure la memoire pour enregistrer les alarmes
#include <EEPROM.h>   // alarm1.hour is in adress 0 alarm1.minute is in adress 2 alarm2.hour is in adress 3 alarm2.minutes is in adress4  

bool clockchanged=false;
bool blinck; // valeur permettant de faire clignoter les leds

// près declaration des fonctions
void state_machine_run();
void refreshbutton();
void refreshsensors();
bool appuilongmiddle();
bool appuilongcross();
void music();
void setalarm();
void adjutsRTC();
void settime();
//void playmelody( int melody[], int sizearray, int tempo);


void setup() {
  // put your setup code here, to run once:
 // on ouvre le port serie:
  Serial.begin(9600);  
  //on decalre la sortie du haut parleur en output
  pinMode(HPpin, OUTPUT); 
  digitalWrite(HPpin, LOW); //on evite le gresillement
  pinMode(Pintilt,INPUT_PULLUP);
  //pinMode(Lumsensor,INPUT); // cette ligne bloque le capacitif touch

  // setup of the 74HC595
  pinMode(2, OUTPUT); // sr data pin
  pinMode(3, OUTPUT); // sr clock pin
  pinMode(4, OUTPUT); // sr latch pin
  sr.interrupt(ShiftRegisterPWM::UpdateFrequency::Fast);

  // setup du RTC
    while (! rtc.begin()) { Serial.println("Couldn't find RTC"); }
    //bipde start
      tone(HPpin , 440,100);      delay(120);      noTone(HPpin );      tone(HPpin , 349,100);      delay(120);      noTone(HPpin );      digitalWrite(HPpin, LOW);
   // mis a jour RTC si besoin  
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, lets set the time!");
      // If the RTC have lost power it will sets the RTC to the date & time this sketch was compiled in the following line
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    settime();
    lastime=millis();
    tempslastmaj=now();
    setalarm();// on viens chercher la valeur des alarmes en memoires
}



void loop() {
  while (nowmili-lastime <1000){ // on attend une seconde
    refreshsensors(); // on met a jour les effets des capteurs
    refreshbutton();  // on mets a jours les boutons
    state_machine_run(); // on verifie les changement d'etats
    nowmili=millis();
    }
    
    D0=!D0;           // led changeant de valeur toutes les secondes
    lastime=nowmili;
    #ifdef debugging
    Serial.print("state =");
    Serial.println(state);
   //disptime();
    #endif
   // mis a jour du RTC tous les deltamaj
   if((now()-tempslastmaj)>deltamaj){
    tempslastmaj=now();
    settime();
   }
  
}

void leddisplay(uint8_t Hour,uint8_t Minute,bool D1,bool D2,bool D0){ // lumon est la valeur de dimmage définie par le capteur de luminosité
  sr.set(9,D0*lumon+!D0*lumoff);
  sr.set(1,D1*lumon);
  sr.set(0,D2*lumon);
  // on gere les heures
  bool val=0b01 & Hour;  sr.set(7,val*lumon+!val*lumoff);
  val=0b10 & Hour;  sr.set(6,val*lumon+!val*lumoff);
  val=0b100 & Hour;  sr.set(5,val*lumon+!val*lumoff);
  val=0b1000 & Hour;  sr.set(4,val*lumon+!val*lumoff);
  val=0b10000 & Hour;  sr.set(3,val*lumon+!val*lumoff);
  val=0b100000 & Hour;  sr.set(2,val*lumon+!val*lumoff);
  // puis les minutes
  val=0b01 & Minute;  sr.set(10,val*lumon+!val*lumoff);
  val=0b10 & Minute;  sr.set(11,val*lumon+!val*lumoff);
  val=0b100 & Minute;  sr.set(12,val*lumon+!val*lumoff);
  val=0b1000 & Minute;  sr.set(13,val*lumon+!val*lumoff);
  val=0b10000 & Minute;  sr.set(14,val*lumon+!val*lumoff);
  val=0b100000 & Minute;  sr.set(15,val*lumon+!val*lumoff);
}

void refreshsensors(){
  lumino = (lumino*(n-1) + analogRead(Lumsensor))/n; // on viens smoothe la valeur
    if(!tilt_on){
      if (lumino < 700){ lumon=lumoff+12;}
      else{lumon=map(lumino,700,1023,lumoff+12,255);}
    }
  
  // on test le tilt
   tiltaverage=(tiltaverage*nb_av+digitalRead(Pintilt))/(nb_av+1); // on fait une moyenne glissante pour eviter clignotmenet  
    if(!tilt_on && tiltaverage>0.6){
    lastlumon=lumon;
    lastlumoff=lumoff;
    lumon=0;
    lumoff=0;
    tilt_on=true;
    } // utilisation du tilt  
      if(tilt_on && tiltaverage < 0.4){
    tilt_on=false;  
    lumon=lastlumon;
    lumoff=lastlumoff;
  }  
}

void refreshbutton(){
    atop =  Btop.capacitiveSensor(numsample)>trigB;
    abot =  Bbottom.capacitiveSensor(numsample)>trigB;
    amiddle =  Bmiddle.capacitiveSensor(numsample)>trigB;
    across =  Bcross.capacitiveSensor(numsample)>trigB;
    #ifdef debugging
        Serial.print(atop);
        Serial.print(abot);
        Serial.print(amiddle);
        Serial.println(across);
    #endif
}


void state_machine_run(){
  blinck=!blinck;
  switch(state){
    case NORMAL: // cas normal on affiche l'heure
      // afficher l'heure
      leddisplay(hour(),minute(),ALARM1_ON,ALARM2_ON,D0); // on affiche l'heure sur le display
        if(atop){
          state=DISPLAYALARM1;
          break; // on a changé d'état on sort de la state machine
        }
        if(abot){
          state=DISPLAYALARM2;
          break; // // on a changé d'état on sort de la state machine
        }
      if(appuilongmiddle()){amiddlereleased=false;state=CHANGEHOUR1;Newclock.Hour=hour(); tempssortienormal=millis(); break;}
      if(appuilongcross()){acrossreleased=false;state=MUSICCHOICE; tempssortienormal=millis(); break;}
      if (ALARM1_ON && alarm1.Hour==hour() && alarm1.Minute==minute()){state=ALARM1; break;} /// afini 
      if (ALARM1_ON && alarm2.Hour==hour() && alarm2.Minute==minute()){state=ALARM2; break;} // a finir
      
      
      break;

      case DISPLAYALARM1:
        leddisplay(alarm1.Hour,alarm1.Minute,ALARM1_ON or D0, false,false); // on affiche sur le display
        if(amiddle){ALARM1_ON=true;EEPROM.write(5, true );break;} // on met en route lalarme si on presse fleche haut et rond en meme temps // et on sauve ca dans la memeoir
        if(across){ALARM1_ON=false;EEPROM.write(5, false);break;}
        if(!atop){state=NORMAL; break;} // si on appuie plus sur la fleche on retourne en etat normal
        break;
        
       case DISPLAYALARM2:
       leddisplay(alarm2.Hour,alarm2.Minute, false , ALARM2_ON or D0,false); // on affiche sur le display
        if(amiddle){ALARM2_ON=true; EEPROM.write(6, true );break;}
        if(across){ALARM2_ON=false; EEPROM.write(6, false); break;}
        if(!abot){state=NORMAL; break;}
        break;

      case CHANGEHOUR1:
            //afficher temps en faisant clignoter les heures au rythme des secondes D0*
          leddisplay(blinck * Newclock.Hour,minute(),false,false,blinck); // probleme ca ne clignote pas .. 
          
          if(atop){
            clockchanged=true;
            Newclock.Hour+=1;            
            }
          if(abot){
            clockchanged=true;
            Newclock.Hour-=1;   
            }
          if(!amiddle){amiddlereleased=true;}
          else{if(amiddlereleased==true){amiddlereleased=false;state=CHANGEHOUR2; Newclock.Minute=minute(); break;}          }
          if(across or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL;clockchanged=false; break;}
          break;

       case CHANGEHOUR2:
      //afficher temps faisant clignoter les minutes
      leddisplay(Newclock.Hour,blinck * Newclock.Minute,false,false,blinck);
      
          if(atop){
            clockchanged=true;
            Newclock.Minute+=1;
            }
          if(abot){
            clockchanged=true;
            Newclock.Minute-=1;
            }
          if(!amiddle){amiddlereleased=true;}
          else{if(amiddlereleased==true){amiddlereleased=false;state=CHANGEHOUR3; adjutsRTC();clockchanged=false;break;}}
          if(across or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL;clockchanged=false; break;}
          break;

      case CHANGEHOUR3:
      //afficher alarm1 en faisant clignoter les heures et la led alram 1
      leddisplay(blinck*alarm1.Hour,alarm1.Minute,blinck,false,blinck);
          if(atop){alarm1.Hour+=1;}
          if(abot){alarm1.Hour-=1;}
          if(!amiddle){amiddlereleased=true;}
          else{if(amiddlereleased==true){amiddlereleased=false;state=CHANGEHOUR4; EEPROM.write(1, alarm1.Hour);  break;}}
          if(across or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL; break;}
          break; 
          
       case CHANGEHOUR4:
      //afficher alarm1 en faisant clignoter les minutes
      leddisplay(alarm1.Hour,blinck*alarm1.Minute,blinck,false,blinck);
          if(atop){alarm1.Minute+=1;}
          if(abot){alarm1.Minute-=1;}
          if(!amiddle){amiddlereleased=true;}
          else{if(amiddlereleased==true){amiddlereleased=false;state=CHANGEHOUR5; EEPROM.write(2, alarm1.Minute); break;}}
          if(across or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL; break;}
          break;

       case CHANGEHOUR5:
      //afficher alarm2 en faisant clignoter les heures
      leddisplay(blinck*alarm2.Hour,alarm2.Minute,false,blinck,blinck);
          if(atop){alarm2.Hour+=1;}
          if(abot){alarm2.Hour-=1;}
          if(!amiddle){amiddlereleased=true;}
          else{if(amiddlereleased==true){amiddlereleased=false;state=CHANGEHOUR6; EEPROM.write(3, alarm2.Hour);break;}}
          if(across or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL; break;}
          break;
       case CHANGEHOUR6:
      //afficher alarm2 en faisant clignoter les minutes
      leddisplay(alarm2.Hour,blinck*alarm2.Minute,false,blinck,blinck);
          if(atop){alarm2.Minute+=1;}
          if(abot){alarm2.Minute-=1;}
          if(!amiddle){amiddlereleased=true;}
          else{if(amiddlereleased==true){amiddlereleased=false;state=NORMAL; EEPROM.write(4, alarm2.Minute); break;}}
          if(across or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL; break;}
          break;
          
       case ALARM1:
       leddisplay(hour(),minute(),blinck,false,D0); // on affiche l'heure sur le display
       //onfait du bruit
       if(atop or abot or amiddle or across or countalarm > 20 ){state=NORMAL;countalarm=0; break;} // quand on appuis sur un bouton on retourne a normalnormal
       music();
       countalarm+=1;
       break;

       case ALARM2:
       leddisplay(hour(),minute(),false,blinck,D0); // on affiche l'heure sur le display
       //onfait du bruit 
       if(atop or abot or amiddle or across or countalarm > 20 ){state=NORMAL;countalarm=0; break;} // quand on appuis sur un bouton on retourne a normal
       music();
       countalarm+=1;
       break;

       case MUSICCHOICE:
          #ifdef debugging
            Serial.print("music =");
            Serial.println(musicnumber);
            if(millis()-tempssortienormal>tempsretourmenu){Serial.println("outoftime");}
          #endif
          leddisplay(musicnumber,0,false,false,false); // on affiche l'heure sur le display
           if(atop){musicnumber+=1; leddisplay(musicnumber,0,false,false,false);tempssortienormal= millis();music();}// on augmente l'indentation du compteur de musique, on viens afficher le numero de musique, on remet a zero le compteur de temps pour la sortie du menu
           if(abot){musicnumber-=1; leddisplay(musicnumber,0,false,false,false);tempssortienormal= millis();music();}
           if(amiddle){EEPROM.write(7,musicnumber);state=NORMAL; break;} // si on appuie sur rond cela sauve la musique enc ours
           if(!across){acrossreleased=true;} // on note si le bouton croix est relaché
           if((across && acrossreleased==true) or (millis()-tempssortienormal>tempsretourmenu)){state=NORMAL; break;}
       break;
  }
}

bool appuilongmiddle(){
        if(amiddle ){
           if(!amiddlecounting){
              amiddlepressed=millis();
              amiddlecounting=true; // on note le temps et commence a compter
              return false;
            }
          else if(amiddlepressed+tempsappuilong<millis()){
             amiddlecounting=false;
             return true;
          }
        }
        else {amiddlecounting=false;}
        return false;
}

bool appuilongcross(){
        if(across){
           if(!acrosscounting){
              acrosspressed=millis();
              acrosscounting=true; // on note le temps et commence a compter
              return false;
            }
          else if(acrosspressed+tempsappuilong<millis()){
             acrosscounting=false;
             return true;
          }
        }
        else {acrosscounting=false;}
        return false;
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


void  setalarm(){
  if(EEPROM.read(0)!=1){ // c'est la premiere fois que l'horloge est configurée, ont met arbitrairement les alamres a 12H12
    EEPROM.write(0, 1);
    EEPROM.write(1, 12);
    EEPROM.write(2, 12);
    EEPROM.write(3, 12);
    EEPROM.write(4, 12);
    EEPROM.write(5, false);
    EEPROM.write(6, false);
    EEPROM.write(7,1);
  }
  alarm1.Hour=EEPROM.read(1);
  alarm1.Minute=EEPROM.read(2);
  alarm2.Hour=EEPROM.read(3);
  alarm2.Minute=EEPROM.read(4);
  ALARM1_ON=EEPROM.read(5);
  ALARM2_ON=EEPROM.read(6);
  musicnumber=EEPROM.read(7);
 }

void adjutsRTC(){ // lheure a été changé on viens modifier le RTC et le calcul en fon du temps
  DateTime now = rtc.now();
  rtc.adjust(DateTime(now.year(),now.month(), now.day(),Newclock.Hour , Newclock.Minute, now.second())); // on met a jopur l'horloge du RTC
  setTime(Newclock.Hour,Newclock.Minute,now.second(),now.day(),now.month(),now.year()); // on remet a jour l'horloge de l'arduino
}

void settime(){
  DateTime now = rtc.now();
  setTime(now.hour(),now.minute(),now.second(),now.day(),now.month(),now.year());
}


// we use const PROGMEM to save space
const PROGMEM   int   melody2[] = {   // Baby Elephant Walk            
 NOTE_C4,-8, NOTE_E4,16, NOTE_G4,8, NOTE_C5,8, NOTE_E5,8, NOTE_D5,8, NOTE_C5,8, NOTE_A4,8,NOTE_FS4,8, NOTE_G4,8, REST,4, REST,2, NOTE_C4,-8, NOTE_E4,16, NOTE_G4,8, NOTE_C5,8, NOTE_E5,8, NOTE_D5,8, NOTE_C5,8, NOTE_A4,8,NOTE_G4,-2, NOTE_A4,8, NOTE_DS4,1,NOTE_A4,8,NOTE_E4,8, NOTE_C4,8, REST,4, REST,2,NOTE_C4,-8, NOTE_E4,16, NOTE_G4,8, NOTE_C5,8, NOTE_E5,8, NOTE_D5,8, NOTE_C5,8, NOTE_A4,8,NOTE_FS4,8, NOTE_G4,8, REST,4, REST,4, REST,8, NOTE_G4,8,NOTE_D5,4, NOTE_D5,4, NOTE_B4,8, NOTE_G4,8, REST,8, NOTE_G4,8,         
 NOTE_C5,4, NOTE_C5,4, NOTE_AS4,16, NOTE_C5,16, NOTE_AS4,16, NOTE_G4,16, NOTE_F4,8, NOTE_DS4,8,NOTE_FS4,4, NOTE_FS4,4, NOTE_F4,16, NOTE_G4,16, NOTE_F4,16, NOTE_DS4,16, NOTE_C4,8, NOTE_G4,8, NOTE_AS4,8, NOTE_C5,8, REST,4, REST,2, };
const PROGMEM    int  melody4[] = {  // Hedwig's theme fromn the Harry Potter Movies
 REST, 2, NOTE_D4, 4,  NOTE_G4, -4, NOTE_AS4, 8, NOTE_A4, 4,  NOTE_G4, 2, NOTE_D5, 4,  NOTE_C5, -2,   NOTE_A4, -2,  NOTE_G4, -4, NOTE_AS4, 8, NOTE_A4, 4,  NOTE_F4, 2, NOTE_GS4, 4,  NOTE_D4, -1,   NOTE_D4, 4,  NOTE_G4, -4, NOTE_AS4, 8, NOTE_A4, 4,  NOTE_G4, 2, NOTE_D5, 4,  NOTE_F5, 2, NOTE_E5, 4,  NOTE_DS5, 2, NOTE_B4, 4,  NOTE_DS5, -4, NOTE_D5, 8, NOTE_CS5, 4,  NOTE_CS4, 2, NOTE_B4, 4,  NOTE_G4, -1, NOTE_AS4, 4, NOTE_D5, 2, NOTE_AS4, 4,  NOTE_D5, 2, NOTE_AS4, 4,  NOTE_DS5, 2, NOTE_D5, 4,  NOTE_CS5, 2, NOTE_A4, 4,  NOTE_AS4, -4, NOTE_D5, 8, NOTE_CS5, 4,  NOTE_CS4, 2, NOTE_D4, 4,  NOTE_D5, -1,   REST,4, NOTE_AS4,4, NOTE_D5, 2, NOTE_AS4, 4,  NOTE_D5, 2, NOTE_AS4, 4,  NOTE_F5, 2, NOTE_E5, 4,  NOTE_DS5, 2, NOTE_B4, 4,  NOTE_DS5, -4, NOTE_D5, 8, NOTE_CS5, 4,  NOTE_CS4, 2, NOTE_AS4, 4,  NOTE_G4, -1,};
const PROGMEM     int   melody3[] = {  // Asa branca - Luiz Gonzaga
 NOTE_G4,8, NOTE_A4,8, NOTE_B4,4, NOTE_D5,4, NOTE_D5,4, NOTE_B4,4,NOTE_C5,4, NOTE_C5,2, NOTE_G4,8, NOTE_A4,8, NOTE_B4,4, NOTE_D5,4, NOTE_D5,4, NOTE_C5,4, NOTE_B4,2, REST,8, NOTE_G4,8, NOTE_G4,8, NOTE_A4,8,  NOTE_B4,4, NOTE_D5,4, REST,8, NOTE_D5,8, NOTE_C5,8, NOTE_B4,8, NOTE_G4,4, NOTE_C5,4, REST,8, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8, NOTE_A4,4, NOTE_B4,4, REST,8, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8,  NOTE_G4,2, REST,8, NOTE_G4,8, NOTE_G4,8, NOTE_A4,8,  NOTE_B4,4, NOTE_D5,4, REST,8, NOTE_D5,8, NOTE_C5,8, NOTE_B4,8,  NOTE_G4,4, NOTE_C5,4, REST,8, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8,  NOTE_A4,4, NOTE_B4,4, REST,8, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8,  NOTE_G4,4, NOTE_F5,8, NOTE_D5,8, NOTE_E5,8, NOTE_C5,8, NOTE_D5,8, NOTE_B4,8,  NOTE_C5,8, NOTE_A4,8, NOTE_B4,8, NOTE_G4,8, NOTE_A4,8, NOTE_G4,8, NOTE_E4,8, NOTE_G4,8,  NOTE_G4,4, NOTE_F5,8, NOTE_D5,8, NOTE_E5,8, NOTE_C5,8, NOTE_D5,8, NOTE_B4,8,  NOTE_C5,8, NOTE_A4,8, NOTE_B4,8, NOTE_G4,8, NOTE_A4,8, NOTE_G4,8, NOTE_E4,8, NOTE_G4,8,          NOTE_G4,-2, REST,4 };
const PROGMEM int melody5[] = {  // Game of Thrones
 NOTE_G4,8, NOTE_C4,8, NOTE_DS4,16, NOTE_F4,16, NOTE_G4,8, NOTE_C4,8, NOTE_DS4,16, NOTE_F4,16,   NOTE_G4,8, NOTE_C4,8, NOTE_DS4,16, NOTE_F4,16, NOTE_G4,8, NOTE_C4,8, NOTE_DS4,16, NOTE_F4,16,  NOTE_G4,8, NOTE_C4,8, NOTE_E4,16, NOTE_F4,16, NOTE_G4,8, NOTE_C4,8, NOTE_E4,16, NOTE_F4,16,  NOTE_G4,8, NOTE_C4,8, NOTE_E4,16, NOTE_F4,16, NOTE_G4,8, NOTE_C4,8, NOTE_E4,16, NOTE_F4,16,  NOTE_G4,-4, NOTE_C4,-4,  NOTE_DS4,16, NOTE_F4,16, NOTE_G4,4, NOTE_C4,4, NOTE_DS4,16, NOTE_F4,16,   NOTE_D4,-1,   NOTE_F4,-4, NOTE_AS3,-4,  NOTE_DS4,16, NOTE_D4,16, NOTE_F4,4, NOTE_AS3,-4,  NOTE_DS4,16, NOTE_D4,16, NOTE_C4,-1,    NOTE_G4,-4, NOTE_C4,-4,  NOTE_DS4,16, NOTE_F4,16, NOTE_G4,4, NOTE_C4,4, NOTE_DS4,16, NOTE_F4,16,   NOTE_D4,-1,   NOTE_F4,-4, NOTE_AS3,-4,  NOTE_DS4,16, NOTE_D4,16, NOTE_F4,4, NOTE_AS3,-4,  NOTE_DS4,16, NOTE_D4,16, NOTE_C4,-1,   NOTE_G4,-4, NOTE_C4,-4,  NOTE_DS4,16, NOTE_F4,16, NOTE_G4,4,  NOTE_C4,4, NOTE_DS4,16, NOTE_F4,16,  NOTE_D4,-2,  NOTE_F4,-4, NOTE_AS3,-4,  NOTE_D4,-8, NOTE_DS4,-8, NOTE_D4,-8, NOTE_AS3,-8,  NOTE_C4,-1,  NOTE_C5,-2,  NOTE_AS4,-2,  NOTE_C4,-2,  NOTE_G4,-2,  NOTE_DS4,-2, NOTE_DS4,-4, NOTE_F4,-4,  NOTE_G4,-1,NOTE_C5,-2,  NOTE_AS4,-2,  NOTE_C4,-2,  NOTE_G4,-2,   NOTE_DS4,-2,  NOTE_DS4,-4, NOTE_D4,-4,  NOTE_C5,8, NOTE_G4,8, NOTE_GS4,16, NOTE_AS4,16, NOTE_C5,8, NOTE_G4,8, NOTE_GS4,16, NOTE_AS4,16,  NOTE_C5,8, NOTE_G4,8, NOTE_GS4,16, NOTE_AS4,16, NOTE_C5,8, NOTE_G4,8, NOTE_GS4,16, NOTE_AS4,16,    REST,4, NOTE_GS5,16, NOTE_AS5,16, NOTE_C6,8, NOTE_G5,8, NOTE_GS5,16, NOTE_AS5,16,  NOTE_C6,8, NOTE_G5,16, NOTE_GS5,16, NOTE_AS5,16, NOTE_C6,8, NOTE_G5,8, NOTE_GS5,16, NOTE_AS5,16,  };
const PROGMEM int melody6[] = {   //zelda
 NOTE_AS4,-2,  NOTE_F4,8,  NOTE_F4,8,  NOTE_AS4,8,  NOTE_GS4,16,  NOTE_FS4,16,  NOTE_GS4,-2,  NOTE_AS4,-2,  NOTE_FS4,8,  NOTE_FS4,8,  NOTE_AS4,8,  NOTE_A4,16,  NOTE_G4,16,  NOTE_A4,-2,  REST,1,   NOTE_AS4,4,  NOTE_F4,-4,  NOTE_AS4,8,  NOTE_AS4,16,  NOTE_C5,16, NOTE_D5,16, NOTE_DS5,16,  NOTE_F5,2,  NOTE_F5,8,  NOTE_F5,8,  NOTE_F5,8,  NOTE_FS5,16, NOTE_GS5,16,  NOTE_AS5,-2,  NOTE_AS5,8,  NOTE_AS5,8,  NOTE_GS5,8,  NOTE_FS5,16,  NOTE_GS5,-8,  NOTE_FS5,16,  NOTE_F5,2,  NOTE_F5,4,   NOTE_DS5,-8, NOTE_F5,16, NOTE_FS5,2, NOTE_F5,8, NOTE_DS5,8,   NOTE_CS5,-8, NOTE_DS5,16, NOTE_F5,2, NOTE_DS5,8, NOTE_CS5,8,  NOTE_C5,-8, NOTE_D5,16, NOTE_E5,2, NOTE_G5,8,   NOTE_F5,16, NOTE_F4,16, NOTE_F4,16, NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,8, NOTE_F4,16,NOTE_F4,8,  NOTE_AS4,4,  NOTE_F4,-4,  NOTE_AS4,8,  NOTE_AS4,16,  NOTE_C5,16, NOTE_D5,16, NOTE_DS5,16,  NOTE_F5,2,  NOTE_F5,8,  NOTE_F5,8,  NOTE_F5,8,  NOTE_FS5,16, NOTE_GS5,16,  NOTE_AS5,-2, NOTE_CS6,4,  NOTE_C6,4, NOTE_A5,2, NOTE_F5,4,  NOTE_FS5,-2, NOTE_AS5,4,  NOTE_A5,4, NOTE_F5,2, NOTE_F5,4,  NOTE_FS5,-2, NOTE_AS5,4,  NOTE_A5,4, NOTE_F5,2, NOTE_D5,4,  NOTE_DS5,-2, NOTE_FS5,4,  NOTE_F5,4, NOTE_CS5,2, NOTE_AS4,4,  NOTE_C5,-8, NOTE_D5,16, NOTE_E5,2, NOTE_G5,8,   NOTE_F5,16, NOTE_F4,16, NOTE_F4,16, NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,16,NOTE_F4,8, NOTE_F4,16,NOTE_F4,8};
const PROGMEM int melody7[] = { // tetris
 NOTE_E5, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_C5,8,  NOTE_B4,8,  NOTE_A4, 4,  NOTE_A4,8,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,  NOTE_B4, -4,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,8,  NOTE_A4,4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5, -4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,  NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,  NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,  NOTE_E5, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_C5,8,  NOTE_B4,8,  NOTE_A4, 4,  NOTE_A4,8,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,  NOTE_B4, -4,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,8,  NOTE_A4,4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5, -4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,  NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,  NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,   NOTE_E5,2,  NOTE_C5,2,  NOTE_D5,2,   NOTE_B4,2,  NOTE_C5,2,   NOTE_A4,2,  NOTE_GS4,2,  NOTE_B4,4,  REST,8,   NOTE_E5,2,   NOTE_C5,2,  NOTE_D5,2,   NOTE_B4,2,  NOTE_C5,4,   NOTE_E5,4,  NOTE_A5,2,  NOTE_GS5,2,};
const PROGMEM int melody8[]= {   // Take on me, by A-ha
 NOTE_FS5,8, NOTE_FS5,8,NOTE_D5,8, NOTE_B4,8, REST,8, NOTE_B4,8, REST,8, NOTE_E5,8,   REST,8, NOTE_E5,8, REST,8, NOTE_E5,8, NOTE_GS5,8, NOTE_GS5,8, NOTE_A5,8, NOTE_B5,8,  NOTE_A5,8, NOTE_A5,8, NOTE_A5,8, NOTE_E5,8, REST,8, NOTE_D5,8, REST,8, NOTE_FS5,8,   REST,8, NOTE_FS5,8, REST,8, NOTE_FS5,8, NOTE_E5,8, NOTE_E5,8, NOTE_FS5,8, NOTE_E5,8,  NOTE_FS5,8, NOTE_FS5,8,NOTE_D5,8, NOTE_B4,8, REST,8, NOTE_B4,8, REST,8, NOTE_E5,8,  REST,8, NOTE_E5,8, REST,8, NOTE_E5,8, NOTE_GS5,8, NOTE_GS5,8, NOTE_A5,8, NOTE_B5,8,  NOTE_A5,8, NOTE_A5,8, NOTE_A5,8, NOTE_E5,8, REST,8, NOTE_D5,8, REST,8, NOTE_FS5,8,   REST,8, NOTE_FS5,8, REST,8, NOTE_FS5,8, NOTE_E5,8, NOTE_E5,8, NOTE_FS5,8, NOTE_E5,8,  NOTE_FS5,8, NOTE_FS5,8,NOTE_D5,8, NOTE_B4,8, REST,8, NOTE_B4,8, REST,8, NOTE_E5,8,   REST,8, NOTE_E5,8, REST,8, NOTE_E5,8, NOTE_GS5,8, NOTE_GS5,8, NOTE_A5,8, NOTE_B5,8,    NOTE_A5,8, NOTE_A5,8, NOTE_A5,8, NOTE_E5,8, REST,8, NOTE_D5,8, REST,8, NOTE_FS5,8,   REST,8, NOTE_FS5,8, REST,8, NOTE_FS5,8, NOTE_E5,8, NOTE_E5,8, NOTE_FS5,8, NOTE_E5,8,  };
const PROGMEM int melody9[] = {  // Wiegenlied (Brahms' Lullaby)
 NOTE_G4, 4, NOTE_G4, 4,  NOTE_AS4, -4, NOTE_G4, 8, NOTE_G4, 4,  NOTE_AS4, 4, REST, 4, NOTE_G4, 8, NOTE_AS4, 8,  NOTE_DS5, 4, NOTE_D5, -4, NOTE_C5, 8,  NOTE_C5, 4, NOTE_AS4, 4, NOTE_F4, 8, NOTE_G4, 8,  NOTE_GS4, 4, NOTE_F4, 4, NOTE_F4, 8, NOTE_G4, 8,  NOTE_GS4, 4, REST, 4, NOTE_F4, 8, NOTE_GS4, 8,  NOTE_D5, 8, NOTE_C5, 8, NOTE_AS4, 4, NOTE_D5, 4,  NOTE_DS5, 4, REST, 4, NOTE_DS4, 8, NOTE_DS4, 8,  NOTE_DS5, 2, NOTE_C5, 8, NOTE_GS4, 8,  NOTE_AS4, 2, NOTE_G4, 8, NOTE_DS4, 8,  NOTE_GS4, 4, NOTE_AS4, 4, NOTE_C5, 4,  NOTE_AS4, 2, NOTE_DS4, 8, NOTE_DS4, 8,  NOTE_DS5, 2, NOTE_C5, 8, NOTE_GS4, 8,  NOTE_AS4, 2, NOTE_G4, 8, NOTE_DS4, 8,  NOTE_AS4, 4, NOTE_G4, 4, NOTE_DS4, 4,  NOTE_DS4, 2};
const PROGMEM int melody10[] = {  // Dart Vader theme (Imperial March) - Star wars 
 NOTE_AS4,8, NOTE_AS4,8, NOTE_AS4,8, NOTE_F5,2, NOTE_C6,2,  NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F6,2, NOTE_C6,4,    NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F6,2, NOTE_C6,4,    NOTE_AS5,8, NOTE_A5,8, NOTE_AS5,8, NOTE_G5,2, NOTE_C5,8, NOTE_C5,8, NOTE_C5,8,  NOTE_F5,2, NOTE_C6,2,  NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F6,2, NOTE_C6,4,   NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F6,2, NOTE_C6,4,  NOTE_AS5,8, NOTE_A5,8, NOTE_AS5,8, NOTE_G5,2, NOTE_C5,-8, NOTE_C5,16,   NOTE_D5,-4, NOTE_D5,8, NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F5,8,  NOTE_F5,8, NOTE_G5,8, NOTE_A5,8, NOTE_G5,4, NOTE_D5,8, NOTE_E5,4,NOTE_C5,-8, NOTE_C5,16,  NOTE_D5,-4, NOTE_D5,8, NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F5,8,  NOTE_C6,-8, NOTE_G5,16, NOTE_G5,2, REST,8, NOTE_C5,8,  NOTE_D5,-4, NOTE_D5,8, NOTE_AS5,8, NOTE_A5,8, NOTE_G5,8, NOTE_F5,8,  NOTE_F5,8, NOTE_G5,8, NOTE_A5,8, NOTE_G5,4, NOTE_D5,8, NOTE_E5,4,NOTE_C6,-8, NOTE_C6,16,  NOTE_F6,4, NOTE_DS6,8, NOTE_CS6,4, NOTE_C6,8, NOTE_AS5,4, NOTE_GS5,8, NOTE_G5,4, NOTE_F5,8,  NOTE_C6,1  };
const PROGMEM int melody11[] = { // ode to joy
 NOTE_E4,4,  NOTE_E4,4,  NOTE_F4,4,  NOTE_G4,4,  NOTE_G4,4,  NOTE_F4,4,  NOTE_E4,4,  NOTE_D4,4,  NOTE_C4,4,  NOTE_C4,4,  NOTE_D4,4,  NOTE_E4,4,  NOTE_E4,-4, NOTE_D4,8,  NOTE_D4,2,  NOTE_E4,4,  NOTE_E4,4,  NOTE_F4,4,  NOTE_G4,4,  NOTE_G4,4,  NOTE_F4,4,  NOTE_E4,4,  NOTE_D4,4,  NOTE_C4,4,  NOTE_C4,4,  NOTE_D4,4,  NOTE_E4,4,  NOTE_D4,-4,  NOTE_C4,8,  NOTE_C4,2,  NOTE_D4,4,  NOTE_D4,4,  NOTE_E4,4,  NOTE_C4,4,  NOTE_D4,4,  NOTE_E4,8,  NOTE_F4,8,  NOTE_E4,4, NOTE_C4,4,  NOTE_D4,4,  NOTE_E4,8,  NOTE_F4,8,  NOTE_E4,4, NOTE_D4,4,  NOTE_C4,4,  NOTE_D4,4,  NOTE_G3,2,  NOTE_E4,4,  NOTE_E4,4,  NOTE_F4,4,  NOTE_G4,4,  NOTE_G4,4,  NOTE_F4,4,  NOTE_E4,4,  NOTE_D4,4,  NOTE_C4,4,  NOTE_C4,4,  NOTE_D4,4,  NOTE_E4,4,  NOTE_D4,-4,  NOTE_C4,8,  NOTE_C4,2};
const PROGMEM int melody12[] = {  // Pacman
 NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16,   NOTE_B5, 32, NOTE_FS5, -16, NOTE_DS5, 8, NOTE_C5, 16,  NOTE_C6, 16, NOTE_G6, 16, NOTE_E6, 16, NOTE_C6, 32, NOTE_G6, -16, NOTE_E6, 8,  NOTE_B4, 16,  NOTE_B5, 16,  NOTE_FS5, 16,   NOTE_DS5, 16,  NOTE_B5, 32,    NOTE_FS5, -16, NOTE_DS5, 8,  NOTE_DS5, 32, NOTE_E5, 32,  NOTE_F5, 32,  NOTE_F5, 32,  NOTE_FS5, 32,  NOTE_G5, 32,  NOTE_G5, 32, NOTE_GS5, 32,  NOTE_A5, 16, NOTE_B5, 8};
const PROGMEM int melody13[] = {  // Keyboard cat tempo 160
 REST,1,    REST,1,    NOTE_C4,4, NOTE_E4,4, NOTE_G4,4, NOTE_E4,4,     NOTE_C4,4, NOTE_E4,8, NOTE_G4,-4, NOTE_E4,4,    NOTE_A3,4, NOTE_C4,4, NOTE_E4,4, NOTE_C4,4,    NOTE_A3,4, NOTE_C4,8, NOTE_E4,-4, NOTE_C4,4,    NOTE_G3,4, NOTE_B3,4, NOTE_D4,4, NOTE_B3,4,    NOTE_G3,4, NOTE_B3,8, NOTE_D4,-4, NOTE_B3,4,    NOTE_G3,4, NOTE_G3,8, NOTE_G3,-4, NOTE_G3,8, NOTE_G3,4,     NOTE_G3,4, NOTE_G3,4, NOTE_G3,8, NOTE_G3,4,    NOTE_C4,4, NOTE_E4,4, NOTE_G4,4, NOTE_E4,4,     NOTE_C4,4, NOTE_E4,8, NOTE_G4,-4, NOTE_E4,4,    NOTE_A3,4, NOTE_C4,4, NOTE_E4,4, NOTE_C4,4,NOTE_A3,4, NOTE_C4,8, NOTE_E4,-4, NOTE_C4,4, NOTE_G3,4, NOTE_B3,4, NOTE_D4,4, NOTE_B3,4,NOTE_G3,4, NOTE_B3,8, NOTE_D4,-4, NOTE_B3,4,NOTE_G3,-1,  };
const PROGMEM int melody14[]={  // Badinerie tempo 120
 NOTE_B5,-8, NOTE_D6,16, NOTE_B5,16,  NOTE_FS5,-8, NOTE_B5,16, NOTE_FS5,16, NOTE_D5,-8, NOTE_FS5,16, NOTE_D5,16,  NOTE_B4,4,NOTE_F4,16, NOTE_B4,16, NOTE_D5,16, NOTE_B4,16,  NOTE_CS5,16, NOTE_B4,16, NOTE_CS5,16, NOTE_B4,16, NOTE_AS4,16, NOTE_CS5,16, NOTE_E5,16, NOTE_CS5,16,  NOTE_D5,8, NOTE_B4,8, NOTE_B5,-8, NOTE_D6,16, NOTE_B5,16,  NOTE_FS5,-8, NOTE_B5,16, NOTE_FS5,16, NOTE_D5,-8, NOTE_FS5,16, NOTE_D5,16,  NOTE_B4,4, NOTE_D5,16, NOTE_CS5,-16, NOTE_D5,-8,  NOTE_D5,16, NOTE_CS5,-16, NOTE_D5,-8, NOTE_B5,-8, NOTE_D5,-8,  NOTE_D5,8, NOTE_CS5,-8, NOTE_FS5,-16, /*MI#*/ NOTE_F5,16, NOTE_FS5,-8,  NOTE_FS5,-16, /* MI#??*/NOTE_F5,16, NOTE_FS5,-8, NOTE_D6,-8, NOTE_FS5,-8,  NOTE_FS5,8, /*MI#*/ NOTE_F5,8, NOTE_CS5,16, NOTE_FS5,16, NOTE_A5,16, NOTE_FS5,16,  NOTE_GS5,16, NOTE_FS5,16, NOTE_GS5,16, NOTE_FS5,16, NOTE_F5,16, NOTE_G5,16, NOTE_B5,16, NOTE_G5,16,  NOTE_A5,16, NOTE_GS5,16, NOTE_A5,16, NOTE_G5,16, NOTE_F5,16, NOTE_A5,16, NOTE_FS5,16, NOTE_F5,16,  NOTE_FS5,16, NOTE_B5,16,  NOTE_FS5,16, NOTE_F5,16, NOTE_FS5,16, NOTE_C6,16, NOTE_FS5,16, NOTE_E5,16,  NOTE_FS5,16, NOTE_D6,16, NOTE_FS5,16, NOTE_F5,16, NOTE_FS5,16, NOTE_D6,16, NOTE_C6,16, NOTE_B5,16,  NOTE_C6,16, NOTE_A5,16, NOTE_GS5,16, NOTE_FS5,16, NOTE_A5,8, NOTE_G5,8,  NOTE_FS5,4, REST,4, NOTE_FS5,-8, NOTE_A5,16, NOTE_FS5,16,  NOTE_CS5,-4, NOTE_FS5,16, NOTE_CS5,16, NOTE_A4,-8, NOTE_CS5,16, NOTE_A4,16,  NOTE_F4,4,   NOTE_C5,8, NOTE_B4,8,  NOTE_E5,8, NOTE_DS5,16, NOTE_FS5,16, NOTE_A5,8, NOTE_GS5,16, NOTE_FS5,16,  NOTE_GS5,8, NOTE_D5,8, NOTE_GS5,-8, NOTE_B5,16, NOTE_GS5,8,  NOTE_E5,-8, NOTE_GS5,16, NOTE_E5,16, NOTE_CS5,-8, NOTE_E5,16, NOTE_CS5,16,  NOTE_A4,4, NOTE_A4,16, NOTE_D5,16, NOTE_FS5,16, NOTE_D5,16,  NOTE_E5,16, NOTE_D5,16, NOTE_E5,16, NOTE_D5,16, NOTE_CS5,16, NOTE_E5,16, NOTE_G5,16, NOTE_E5,16,  NOTE_FS5,16, NOTE_E5,16, NOTE_FS5,16, NOTE_E5,16, NOTE_D5,16, NOTE_FS5,16, NOTE_D5,16, NOTE_CS5,16,  NOTE_D5,16, NOTE_G5,16, NOTE_D5,16, NOTE_CS5,16, NOTE_D5,16, NOTE_A5,16, NOTE_D5,16, NOTE_CS5,16,  NOTE_D5,16, NOTE_B5,16, NOTE_D5,16, NOTE_CS5,16, NOTE_D5,16, NOTE_B5,16, NOTE_A5,16, NOTE_G5,16,  NOTE_A5,16, NOTE_FS5,16, NOTE_E5,16, NOTE_D5,16, NOTE_FS5,8, NOTE_E5,16,  NOTE_D5,4, NOTE_FS5,16, NOTE_E5,16, NOTE_FS5,-8,  NOTE_FS5,16, NOTE_E5,16, NOTE_FS5,-8, NOTE_D6,-8, NOTE_FS5,-8,  NOTE_FS5,8, NOTE_E5,8, NOTE_E5,16, NOTE_D5,16, NOTE_E5,-8,  NOTE_E5,16, NOTE_D5,16, NOTE_E5,-8, NOTE_D6,-8, NOTE_E5,-8,  NOTE_E5,8, NOTE_D5,8, NOTE_B5,-8, NOTE_D6,16, NOTE_B5,16,  NOTE_B5,8, NOTE_G5,4, NOTE_G5,4, NOTE_B5,32, NOTE_A5,32, NOTE_G5,32, NOTE_FS5,32,  
 NOTE_E5,4, NOTE_E5,8, NOTE_G5,32, NOTE_FS5,32, NOTE_E5,32, NOTE_D5,32,  NOTE_C5,16, NOTE_E5,16, NOTE_G5,16, NOTE_E5,16, NOTE_CS5,16, NOTE_B4,16, NOTE_CS5,16, NOTE_A4,16,  NOTE_AS4,-8, NOTE_A4,-8, NOTE_G4,8, NOTE_F4,8,  NOTE_A4,8, NOTE_AS4,16, NOTE_CS5,16, NOTE_E5,8, NOTE_D5,16, NOTE_CS5,16,  NOTE_D5,8, NOTE_B4,32, NOTE_CS5,32, NOTE_D5,32, NOTE_E5,32, NOTE_FS5,8, NOTE_D5,16, NOTE_FS5,16,  NOTE_B5,8, NOTE_FS5,8, NOTE_E5,16, NOTE_D5,16, NOTE_CS5,16, NOTE_D5,16,  NOTE_CS5,8, NOTE_B4,4};
const PROGMEM int melody15[] = {  // Jigglypuff's Song tempo 84
  NOTE_D5,-4, NOTE_A5,8, NOTE_FS5,8, NOTE_D5,8,  NOTE_E5,-4, NOTE_FS5,8, NOTE_G5,4,  NOTE_FS5,-4, NOTE_E5,8, NOTE_FS5,4,  NOTE_D5,-2,  NOTE_D5,-4, NOTE_A5,8, NOTE_FS5,8, NOTE_D5,8,  NOTE_E5,-4, NOTE_FS5,8, NOTE_G5,4,  NOTE_FS5,-1,  NOTE_D5,-4, NOTE_A5,8, NOTE_FS5,8, NOTE_D5,8,  NOTE_E5,-4, NOTE_FS5,8, NOTE_G5,4,    NOTE_FS5,-4, NOTE_E5,8, NOTE_FS5,4,  NOTE_D5,-2,  NOTE_D5,-4, NOTE_A5,8, NOTE_FS5,8, NOTE_D5,8,  NOTE_E5,-4, NOTE_FS5,8, NOTE_G5,4,  NOTE_FS5,-1,};
const PROGMEM int melody16[] = {  // Minuet in G - Petzold tempo 140
  NOTE_D5,4, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8, NOTE_C5,8,  NOTE_D5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_E5,4, NOTE_C5,8, NOTE_D5,8, NOTE_E5,8, NOTE_FS5,8,  NOTE_G5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_C5,4, NOTE_D5,8, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8,    NOTE_B4,4, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8,  NOTE_FS4,4, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8, NOTE_G4,8,  NOTE_A4,-2,  NOTE_D5,4, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8, NOTE_C5,8,   NOTE_D5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_E5,4, NOTE_C5,8, NOTE_D5,8, NOTE_E5,8, NOTE_FS5,8,    NOTE_G5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_C5,4, NOTE_D5,8, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8,   NOTE_B4,4, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8,  NOTE_A4,4, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8, NOTE_FS4,8,  NOTE_G4,-2,  NOTE_D5,4, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8, NOTE_C5,8,  NOTE_D5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_E5,4, NOTE_C5,8, NOTE_D5,8, NOTE_E5,8, NOTE_FS5,8,  NOTE_G5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_C5,4, NOTE_D5,8, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8,    NOTE_B4,4, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8,  NOTE_FS4,4, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8, NOTE_G4,8,  NOTE_A4,-2,  NOTE_D5,4, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8, NOTE_C5,8,   NOTE_D5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_E5,4, NOTE_C5,8, NOTE_D5,8, NOTE_E5,8, NOTE_FS5,8,    NOTE_G5,4, NOTE_G4,4, NOTE_G4,4,  NOTE_C5,4, NOTE_D5,8, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8,  NOTE_B4,4, NOTE_C5,8, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8,  NOTE_A4,4, NOTE_B4,8, NOTE_A4,8, NOTE_G4,8, NOTE_FS4,8,  NOTE_G4,-2,  NOTE_B5,4, NOTE_G5,8, NOTE_A5,8, NOTE_B5,8, NOTE_G5,8,  NOTE_A5,4, NOTE_D5,8, NOTE_E5,8, NOTE_FS5,8, NOTE_D5,8,  NOTE_G5,4, NOTE_E5,8, NOTE_FS5,8, NOTE_G5,8, NOTE_D5,8,  NOTE_CS5,4, NOTE_B4,8, NOTE_CS5,8, NOTE_A4,4,  NOTE_A4,8, NOTE_B4,8, NOTE_CS5,8, NOTE_D5,8, NOTE_E5,8, NOTE_FS5,8,  NOTE_G5,4, NOTE_FS5,4, NOTE_E5,4,   NOTE_FS5,4, NOTE_A4,4, NOTE_CS5,4,  NOTE_D5,-2,  NOTE_D5,4, NOTE_G4,8, NOTE_FS5,8, NOTE_G4,4,  NOTE_E5,4,  NOTE_G4,8, NOTE_FS4,8, NOTE_G4,4,  NOTE_D5,4, NOTE_C5,4, NOTE_B4,4,  NOTE_A4,8, NOTE_G4,8, NOTE_FS4,8, NOTE_G4,8, NOTE_A4,4,  NOTE_D4,8, NOTE_E4,8, NOTE_FS4,8, NOTE_G4,8, NOTE_A4,8, NOTE_B4,8,  NOTE_C5,4, NOTE_B4,4, NOTE_A4,4,  NOTE_B4,8, NOTE_D5,8, NOTE_G4,4, NOTE_FS4,4,  NOTE_G4,-2, };
const PROGMEM int melody17[] = {  // The Lick tempo 108 
  NOTE_D4,8, NOTE_E4,8, NOTE_F4,8, NOTE_G4,8, NOTE_E4,4, NOTE_C4,8, NOTE_D4,1,};
const PROGMEM int melody18[] = { // Star Trek Intro tempo 80
NOTE_D4, -8, NOTE_G4, 16, NOTE_C5, -4,   NOTE_B4, 8, NOTE_G4, -16, NOTE_E4, -16, NOTE_A4, -16,  NOTE_D5, 2,};
const PROGMEM int melody19[] = { // Vamire Killer, from castlevania tempo 130
  NOTE_E5,16, NOTE_E5,8, NOTE_D5,16, REST,16, NOTE_CS5,-4, NOTE_E4,8, NOTE_FS4,16, NOTE_G4,16, NOTE_A4,16,   NOTE_B4,-8, NOTE_E4,-8, NOTE_B4,8, NOTE_A4,16, NOTE_D5,-4,   NOTE_E5,16, NOTE_E5,8, NOTE_D5,16, REST,16, NOTE_CS5,-4, NOTE_E4,8, NOTE_FS4,16, NOTE_G4,16, NOTE_A4,16,  NOTE_B4,-8, NOTE_E4,-8, NOTE_B4,8, NOTE_A4,16, NOTE_D4,-4,  REST,8, NOTE_E5,8, REST,16, NOTE_B5,16, REST,8, NOTE_AS5,16, NOTE_B5,16, NOTE_AS5,16, NOTE_G5,16, REST,4,  NOTE_B5,8, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_AS5,16, NOTE_A5,16, REST,16, NOTE_B5,16, NOTE_G5,16, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_B5,16, NOTE_A5,16, NOTE_G5,16,  REST,8, NOTE_E5,8, REST,16, NOTE_B5,16, REST,8, NOTE_AS5,16, NOTE_B5,16, NOTE_AS5,16, NOTE_G5,16, REST,4,  NOTE_B5,8, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_AS5,16, NOTE_A5,16, REST,16, NOTE_B5,16, NOTE_G5,16, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_B5,16, NOTE_A5,16, NOTE_G5,16,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,-8, NOTE_E4,8,   NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,-8, REST,8,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,-8, NOTE_E4,8,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_CS5,-8, NOTE_DS5,8,  NOTE_E5,16, NOTE_E5,16, NOTE_E4,16, NOTE_E4,-2,  NOTE_C4,8, NOTE_C4,8, NOTE_E4,16, NOTE_G4,-8, NOTE_D4,8, NOTE_D4,8, NOTE_FS4,16, NOTE_A4,-8,  NOTE_E5,16, NOTE_E5,16, NOTE_E4,16, NOTE_E4,-2,  NOTE_C4,8, NOTE_C4,8, NOTE_E4,16, NOTE_G4,-8, NOTE_D4,8, NOTE_D4,8, NOTE_B3,16, NOTE_D4,-8,  NOTE_E5,16, NOTE_E5,8, NOTE_D5,16, REST,16, NOTE_CS5,-4, NOTE_E4,8, NOTE_FS4,16, NOTE_G4,16, NOTE_A4,16,    NOTE_B4,-8, NOTE_E4,-8, NOTE_B4,8, NOTE_A4,16, NOTE_D5,-4,   NOTE_E5,16, NOTE_E5,8, NOTE_D5,16, REST,16, NOTE_CS5,-4, NOTE_E4,8, NOTE_FS4,16, NOTE_G4,16, NOTE_A4,16,  NOTE_B4,-8, NOTE_E4,-8, NOTE_B4,8, NOTE_A4,16, NOTE_D4,-4,  REST,8, NOTE_E5,8, REST,16, NOTE_B5,16, REST,8, NOTE_AS5,16, NOTE_B5,16, NOTE_AS5,16, NOTE_G5,16, REST,4,  NOTE_B5,8, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_AS5,16, NOTE_A5,16, REST,16, NOTE_B5,16, NOTE_G5,16, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_B5,16, NOTE_A5,16, NOTE_G5,16,  REST,8, NOTE_E5,8, REST,16, NOTE_B5,16, REST,8, NOTE_AS5,16, NOTE_B5,16, NOTE_AS5,16, NOTE_G5,16, REST,4,  NOTE_B5,8, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_AS5,16, NOTE_A5,16, REST,16, NOTE_B5,16, NOTE_G5,16, NOTE_B5,16, NOTE_AS5,16, REST,16, NOTE_B5,16, NOTE_A5,16, NOTE_G5,16,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,-8, NOTE_E4,8,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,-8, REST,8,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,-8, NOTE_E4,8,  NOTE_DS4,-8, NOTE_FS4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_CS5,-8, NOTE_DS5,8,  NOTE_E5,16, NOTE_E5,16, NOTE_E4,16, NOTE_E4,-2,  NOTE_C4,8, NOTE_C4,8, NOTE_E4,16, NOTE_G4,-8, NOTE_D4,8, NOTE_D4,8, NOTE_FS4,16, NOTE_A4,-8,  NOTE_E5,16, NOTE_E5,16, NOTE_E4,16, NOTE_E4,-2,  NOTE_C4,8, NOTE_C4,8, NOTE_E4,16, NOTE_G4,-8, NOTE_D4,8, NOTE_D4,8, NOTE_B3,16, NOTE_D4,-8,};
const PROGMEM int melody20[] = {  // Song of storms - The Legend of Zelda Ocarina of Time.  tempo 108
   NOTE_D4,4, NOTE_A4,4, NOTE_A4,4,  REST,8, NOTE_E4,8, NOTE_B4,2,  NOTE_F4,4, NOTE_C5,4, NOTE_C5,4,  REST,8, NOTE_E4,8, NOTE_B4,2,  NOTE_D4,4, NOTE_A4,4, NOTE_A4,4,  REST,8, NOTE_E4,8, NOTE_B4,2,  NOTE_F4,4, NOTE_C5,4, NOTE_C5,4,  REST,8, NOTE_E4,8, NOTE_B4,2,  NOTE_D4,8, NOTE_F4,8, NOTE_D5,2,    NOTE_D4,8, NOTE_F4,8, NOTE_D5,2,  NOTE_E5,-4, NOTE_F5,8, NOTE_E5,8, NOTE_E5,8,  NOTE_E5,8, NOTE_C5,8, NOTE_A4,2,  NOTE_A4,4, NOTE_D4,4, NOTE_F4,8, NOTE_G4,8,  NOTE_A4,-2,  NOTE_A4,4, NOTE_D4,4, NOTE_F4,8, NOTE_G4,8,  NOTE_E4,-2,  NOTE_D4,8, NOTE_F4,8, NOTE_D5,2,  NOTE_D4,8, NOTE_F4,8, NOTE_D5,2,  NOTE_E5,-4, NOTE_F5,8, NOTE_E5,8, NOTE_E5,8,  NOTE_E5,8, NOTE_C5,8, NOTE_A4,2,  NOTE_A4,4, NOTE_D4,4, NOTE_F4,8, NOTE_G4,8,  NOTE_A4,2, NOTE_A4,4,  NOTE_D4,1,};
const PROGMEM int birthday[] = {  // Happy Birthday tempo 140
  NOTE_C4,4, NOTE_C4,8,   NOTE_D4,-4, NOTE_C4,-4, NOTE_F4,-4,  NOTE_E4,-2, NOTE_C4,4, NOTE_C4,8,   NOTE_D4,-4, NOTE_C4,-4, NOTE_G4,-4,  NOTE_F4,-2, NOTE_C4,4, NOTE_C4,8,  NOTE_C5,-4, NOTE_A4,-4, NOTE_F4,-4,   NOTE_E4,-4, NOTE_D4,-4, NOTE_AS4,4, NOTE_AS4,8,  NOTE_A4,-4, NOTE_F4,-4, NOTE_G4,-4,  NOTE_F4,-2,};
          
void music(){ // jour la musique de l'alarme
//https://github.com/robsoncouto/arduino-songs
    switch(musicnumber){
        case 1 :
            Serial.println("case1");
            tone(HPpin , 440,100);      delay(120);      noTone(HPpin );      tone(HPpin , 349,100);      delay(120);      noTone(HPpin );      digitalWrite(HPpin, LOW);
            break;
        case 2 :
            playmelody(melody2,sizeof(melody2),120 );
            break;
        case 3 :
            playmelody(melody3,sizeof(melody3),132 );
            break;
        case 4 :
             playmelody(melody4,sizeof(melody4),144 );
            break;
        case 5 :
             playmelody(melody5,sizeof(melody5),85 );
            break;
        case 6 :
             playmelody(melody6,sizeof(melody6),88 );
            break;
        case 7 :
             playmelody(melody7,sizeof(melody7),144 );
            break;
        case 8 :
             playmelody(melody8,sizeof(melody8),140 );
            break;
        case 9 :
             playmelody(melody9,sizeof(melody9),76 );
            break;
        case 10 :
             playmelody(melody10,sizeof(melody10),108 );
            break;
        case 11 :
             playmelody(melody11,sizeof(melody11),114 );
            break;
        case 12 :
             playmelody(melody12,sizeof(melody12),105 );
            break;
        case 13 :
             playmelody(melody13,sizeof(melody13),160 );
            break;
        case 14 :
             playmelody(melody14,sizeof(melody14),120 );
            break;
        case 15 :
             playmelody(melody15,sizeof(melody15),84 );
            break;
        case 16 :
             playmelody(melody16,sizeof(melody16),140 );
            break;
        case 17 :
             playmelody(melody17,sizeof(melody17),108 );
            break;
        case 18 :
             playmelody(melody18,sizeof(melody18),80 );
            break;
        case 19 :
             playmelody(melody19,sizeof(melody19),130 );
            break;
        case 20 :
             playmelody(melody20,sizeof(melody20),108 );
            break;                           
        default :
           Serial.print("default");
           break;
  }
}

void playmelody( int melody[], int sizearray, int tempo){ // le passage de la fonction ne se fait pas.. // ade bgeugger
       int notes = sizearray / sizeof(melody[0]) / 2;
      // this calculates the duration of a whole note in ms
      int wholenote = (60000 * 4) / tempo;
      
      int divider = 0, noteDuration = 0;
      
        // iterate over the notes of the melody.
        // Remember, the array is twice the number of notes (notes + durations)
        for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    // calculates the duration of each note
    divider =  pgm_read_word_near(melody+thisNote + 1);;
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }
              // we only play the note for 90% of the duration, leaving 10% as a pause
              tone(HPpin,pgm_read_word_near(melody+thisNote ), noteDuration * 0.9);
              // Wait for the specief duration before playing the next note.
              long smartdelay=millis();
              do{
                refreshbutton();
                if( atop or abot or amiddle or across){return true;} // permet de sortir de la musique si besoin :)
                //
              }while((millis()-smartdelay) < noteDuration);
              // stop the waveform generation before the next note.
              noTone(HPpin);
            }
            digitalWrite(HPpin, LOW); // on remet bien la pin a zero a la fin 
}
