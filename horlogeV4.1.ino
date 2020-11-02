// programme de gestion de lhorloge V3
//20/10/2020 Yohan Wanderoild
/*
 *les boutons sont operationels,
 *on appuis sur fleche haut pour voir lalarme1 puis en mainetant fleche haut on appuie sur rond pour lactiver et croix pour la desactiver
 *de meme avec lalarme 2 et fleche bas
 *en maintenant O appuyé on rentre deans le menu de reglage heure alarme 1 pour alarme 2
 *la melody pour l'arame est a changer :)
 *Si les leds clognotent rapidement on est dans le menu configuration
 *il est possible de gerer les alamrmes avec le RTC .. et utiliser la pin interuption
 *En cas de coupure de courant les alarmes sont eteinte a changer ?
 *gérer la luminosité ? // baisser les resistances
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
int lumoff=12; // luminosité de la led eteinte
int lastlumon=255;
int lastlumoff=1;

//Pour le RTC
#include "RTClib.h"
RTC_DS3231 rtc;
int RTCint=A3;

// pour suivre le temps sans iunterroger le RTC en permanance
#include <TimeLib.h>
long tempslastmaj;  // variable permettant de stocker le moment de la mise a jour
int deltamaj=86400; // temps entre deux mise a jour de lhorloge en secondes

// pour le son
int HPpin = 5;
// pour le tilt
int Pintilt = 13;
float tiltaverage;
int nb_av=8;// ponderation de la moyenne glissante pour le tilt, plus le chiffre est grand plus les leds vont mettre du temps a s'eteindre...
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
int trigB=40; // valeur a augmenter si les boutons sont trop sensibles
bool atop,abot,amiddle,across; // variable permettant de savoir si les boutons onté été appuyés elles sont mis a jour par refreshbutton
unsigned long tempsappuilong = 1500; ///(1seconde 5)

// gestion des etats
enum State_enum{NORMAL,CHANGEHOUR1,CHANGEHOUR2,CHANGEHOUR3,CHANGEHOUR4,CHANGEHOUR5,CHANGEHOUR6,DISPLAYALARM1,DISPLAYALARM2,ALARM1,ALARM2}; // etats mpossible dans la machine a état
uint8_t state = NORMAL; // la variable state permet de savoir dans quel état on est

unsigned long amiddlepressed;// variable permettant de noter le temps ou le bouton O a commencé a être maintenu
bool amiddlecounting=false; // permet de savoir si le bouton rond a étét maintenu appuyé

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
void music();
void setalarm();
void adjutsRTC();
void settime();


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
    atop =  Btop.capacitiveSensor(30)>trigB;
    abot =  Bbottom.capacitiveSensor(30)>trigB;
    amiddle =  Bmiddle.capacitiveSensor(30)>trigB;
    across =  Bcross.capacitiveSensor(30)>trigB;
    #ifdef debugging
//    Serial.print(Btop.capacitiveSensor(30));
//    Serial.print(Bbottom.capacitiveSensor(30));
//    Serial.print(Bmiddle.capacitiveSensor(30));
//    Serial.println(Bcross.capacitiveSensor(30));
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
      if(appuilongmiddle()) {state=CHANGEHOUR1;Newclock.Hour=hour(); break;}
      if (ALARM1_ON && alarm1.Hour==hour() && alarm1.Minute==minute()){state=ALARM1; break;} /// afini 
      if (ALARM1_ON && alarm2.Hour==hour() && alarm2.Minute==minute()){state=ALARM2; break;} // a finir
      break;

      case DISPLAYALARM1:
        leddisplay(alarm1.Hour,alarm1.Minute,ALARM1_ON or D0, false,false); // on affiche sur le display
        if(amiddle){ALARM1_ON=true; break;} // on met en route lalarme si on presse fleche haut et rond en meme temps
        if(across){ALARM1_ON=false; break;}
        if(!atop){state=NORMAL; break;} // si on appuie plus sur la fleche on retourne en etat normal
        break;
        
       case DISPLAYALARM2:
       leddisplay(alarm2.Hour,alarm2.Minute, false , ALARM2_ON or D0,false); // on affiche sur le display
        if(amiddle){ALARM2_ON=true; break;}
        if(across){ALARM2_ON=false; break;}
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
          if(appuilongmiddle()) {state=CHANGEHOUR2; Newclock.Minute=minute(); break;}
          if(across){state=NORMAL;clockchanged=false; break;}
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
          if(appuilongmiddle()) {state=CHANGEHOUR3; adjutsRTC();clockchanged=false;break;}
          if(across){state=NORMAL;clockchanged=false; break;}
          break;

      case CHANGEHOUR3:
      //afficher alarm1 en faisant clignoter les heures et la led alram 1
      leddisplay(blinck*alarm1.Hour,alarm1.Minute,blinck,false,blinck);
          if(atop){alarm1.Hour+=1;}
          if(abot){alarm1.Hour-=1;}
          if(appuilongmiddle()) {state=CHANGEHOUR4; EEPROM.write(1, alarm1.Hour);  break;}
          if(across){state=NORMAL; break;}
          break; 
          
       case CHANGEHOUR4:
      //afficher alarm1 en faisant clignoter les minutes
      leddisplay(alarm1.Hour,blinck*alarm1.Minute,blinck,false,blinck);
          if(atop){alarm1.Minute+=1;}
          if(abot){alarm1.Minute-=1;}
          if(appuilongmiddle()) {state=CHANGEHOUR5; EEPROM.write(2, alarm1.Minute); break;}
          if(across){state=NORMAL; break;}
          break;

       case CHANGEHOUR5:
      //afficher alarm2 en faisant clignoter les heures
      leddisplay(blinck*alarm2.Hour,alarm2.Minute,false,blinck,blinck);
          if(atop){alarm2.Hour+=1;}
          if(abot){alarm2.Hour-=1;}
          if(appuilongmiddle()) {state=CHANGEHOUR6; EEPROM.write(3, alarm2.Hour);break;}
          if(across){state=NORMAL; break;}
          break;
       case CHANGEHOUR6:
      //afficher alarm2 en faisant clignoter les minutes
      leddisplay(alarm2.Hour,blinck*alarm2.Minute,false,blinck,blinck);
          if(atop){alarm2.Minute+=1;}
          if(abot){alarm2.Minute-=1;}
          if(appuilongmiddle()) {state=NORMAL; EEPROM.write(4, alarm2.Minute); break;}
          if(across){state=NORMAL; break;}
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
void music(){
//musique de lalarme... a affiner 
      tone(HPpin , 440,100);      delay(120);      noTone(HPpin );      tone(HPpin , 349,100);      delay(120);      noTone(HPpin );      digitalWrite(HPpin, LOW);
}


void  setalarm(){
  if(EEPROM.read(0)!=1){ // c'est la premiere fois que l'horloge est configurée, ont met arbitrairement les alamres a 12H12
    EEPROM.write(0, 1);
    EEPROM.write(1, 12);
    EEPROM.write(2, 12);
    EEPROM.write(3, 12);
    EEPROM.write(4, 12);
  }
  alarm1.Hour=EEPROM.read(1);
  alarm1.Minute=EEPROM.read(2);
  alarm2.Hour=EEPROM.read(3);
  alarm2.Minute=EEPROM.read(4);
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
