/*
 *     SP2IPT 2014
 *
 * PL: Miernik mocy wykorzystujacy detektor serii TruPwr
 * PL: pomysl wynikl z dyskusji na forum http://mikrofale.iq24.pl
 * PL: Opublikowano na licencji GPL
 * PL: Wykorzystanie tego kodu w produktach komercyjnych wymaga odrebnej licencji
 * EN: Power meter using TruPwr series detector
 * EN: idea came from a discussion on http://mikrofale.iq24.pl board
 * EN: Published under GPL license
 * EN: Use of this code in commercial products requires separate licence
 */

//
// *********** DEFINES **********
//

/*
 * PL: Definicja pinu wyjsciowego do ustroju analogowego
 * EN: Analog output pin definition
 */
#define METER_OUT 46

/*
 * PL: Definicja pinow wejsciowych
 * EN: Input pins definition
 */
#define ENCODER_A 50
#define ENCODER_B 52
#define ENCODER_BUTTON 48

/*
 * PL: Definicja pinow I/O do komunikacji
 * EN: I/O port definition
 * Arduino Mega I2C default : SDA = 20, SCL = 21
 */
#define OLED_SDA 18
#define OLED_SCL 19
#define HEAD_SDA 20
#define HEAD_SCL 21

/*
 * PL: Definicje uzywanych ADC
 * EN: ADCs used
 */
#define ADC0 0
#define ADCMAXVALUE  2.56

/*
 * PL: Napisy w jezykach narodowych
 * EN: Text values in national languages
 */
#define POWERVALUE	"  Wartosc mocy"

//
// *********** INCLUDES AND INITIAL VALUES **********
//

/*
 * PL: Zakres zmiennych potrzebny do pilnowania licznika enkodera
 * EN: Variables range needed for encoder counter monitoring
 */
#include <limits.h>

/*
 * OLED
 * http://www.bro.net.pl/Obrazki/OLED_I2C_SPI.zip
 */
#include <Adafruit_GFX.h>
#include <IIC_without_ACK.h>
#include "oledfont.c"
IIC_without_ACK Display(OLED_SDA, OLED_SCL);


/*
 * Encoder
 * https://github.com/0xPIT/Encoder/tree/arduino
 */
#include <ClickEncoder.h>
#include <TimerOne.h>
ClickEncoder *Encoder;
int16_t EncoderLast, EncoderCurrent;


/*
 * PL: Zmienna okreslajaca pozycje w menu - menu hierarchiczne rozlozone 
 * PL: na strukture plaska - 8 pozycji na toplevel, kazda z pozycji posiada
 * PL: dostepne 8 pozycji podmenu.
 * PL: Pozycja 0 menu toplevel sluzy do umieszczenia informacji o urzadzeniu
 *
 * EN: Variable describing position in menu - hierarchical menu put into
 * EN: flat structure - 8 positions for top level, each position can hold 
 * EN: up to 8 submenu positions
 * EN: Position number 0 in toplevel menu used for info about device
 * 
 * 0      - info
 * 1-9    - toplevel
 * 10-17  - sublevel 1
 * 18-25  - sublevel 2
 * 26-33  - sublevel 3
 * 34-41  - sublevel 4
 * 42-49  - sublevel 5
 * 50-57  - sublevel 6
 * 58-65  - sublevel 7
 * 66-73  - sublevel 8
 * PL: pozostale pozycje - rezerwa np. na jeszcze nizszy poziom
 * EN: other positions as reserve, maybe for a lower level
 *
 */
byte MenuSub=0;
String MenuBarTemp="SP2IPT     2014";

// PL: Zmienna sluzaca wybierania pozycji w danym poziomie menu
// EN: Variable used as a selector of current position in menu
byte MenuPos=0;


// PL: Obsluga przerwania dla enkodera
// EN: Interrupt service for encoder
void timerIsr() {
  Encoder -> service();
}


// EEPROM (I2C)
// http://arduino.cc/playground/Main/LibraryForI2CEEPROM
#include <Wire.h>
#include <I2C_eeprom.h>
I2C_eeprom eeprom(0x01);


/*
 * PL: Zmienna opisujaca typ wyswietlanej wartosci
 * EN: Variable describing displayed measure type
 * DisplayPower
 */
byte PowerDisplayType = 0;


/*
 * PL: Zmienne dla przetwornika ADC0 - start z zerowymi wskazaniami
 * EN: ADC0 variables - program start with zero values
 */
float ADC0RefValue = 0;
float ADC0CorrFactor = 100;




//
// *********** BOARD SETUP **********
//
void setup()
{
  // PL: Ustawienie trybow pracy pinow I/O
  // EN: I/O pin setup
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  pinMode(ENCODER_BUTTON, INPUT);
  pinMode(METER_OUT, OUTPUT);

 /*
  * PL: Wewnetrzne napiecie odniesienia standardowo wynosi 5V
  * PL: AD8370 daje maksymalnie 2,5V mozna zatem wybrac 2,56V
  * EN: Standard internal reference voltage is 5 V, AD8370 
  * EN: gives 2.5 V max so we can choose 2.56 V
  */
  analogReference(INTERNAL2V56);

  // PL: Ustawienie enkodera: A, B, przycisk, typ enkodera, typ przycisku 
  // EN: Encoder setup: A, B, button, encoder type, button type
  Encoder = new ClickEncoder(ENCODER_A, ENCODER_B, ENCODER_BUTTON, 4, LOW);
  // PL: Wlaczenie przerwania dla obslugi enkodera
  // EN: Encoder interrupe enable
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  EncoderLast = 0;
 
  // Inicjalizacja EEPROMu
  //eeprom.begin();
  
  // PL: Inicjalizacja wyswietlacza
  // EN: Display initialize
  Display.Initial();
  Display.Fill_Screen(0x00);
  Display.Char_F8x16(0, 0, POWERVALUE);
  DisplayMenuBar();

  // PL: Otwarcie portu szeregowego
  // EN: Serial port open
  Serial.begin(57600);

  // PL: Opoznienie - na wszelki wypadek
  // EN: Delay just in case
  delay(10);
}



//
// *********** ENCODER **********
//
// PL: Fnkcja obsluguje sterowanie enkoderem
// EN: encoder control function
void EncoderRun(void) {
  // PL: Lista pozycji w glownym poziomie menu - ostatni element musi byc "" - sluzy do obliceznia ilosci el.
  // EN: Menu positions in top level menu, the last element has to be "" - it serves a purpose in counting
  String MenuTopLevel[] = {MenuBarTemp, "Head", "Corr", "Zero", "Display", ""};
  
  // PL: Lista pozycji podmenu 1 - glowica pomiarowa
  // EN: Submenu 1 list - power head
  String MenuSubLevel1[] = {"Initialize", "Calibrate", ""};
  
/*TODO: this should be taken from an EEPROM in the head		*/
  // PL: Lista pozycji podmenu 2 - korekcja czestotliwosci dla glowicy
  // EN: Submenu 2 list - frequency correction for the head
  String MenuSubLevel2[] = { "100M", "200M", "500M", "1.0G", "1.5G", "2.0G", "3.0G", "4.0G", ""};
 
  // PL: Lista wspolczynnikow korekcji dla drugiej tablicy podmenu
  // EN: Correction values for Submenu 2
  float MenuSubLevel2Values[] = {100, 50, 99, 99, 99, 98, 98, 1 };
 
/* TODO: this menu should hold workflow type: simple measurement, relative, automated types */
  // PL: Lista pozycji podmenu 3 - zerowanie pomiarow
  // EN: Submenu 3 list - zeroing of measurements
  String MenuSubLevel3[] = { "Off", "Sample", ""};
  
  // PL: Lista pozycji podmenu 4 - wyswietlane wartosci
  // EN: Submenu 4 list - displayed values
  String MenuSubLevel4[] = { "dBm", "dBV", "dBmV", "dBuV", "V", "dB", "" };
  
  // PL: Odczytanie wartosci enkodera
  // EN: Read current encoder value
  EncoderCurrent += Encoder -> getValue();
                                                                                                                                                                                                                                                                                                                                           
  /*
   * PL: Enkoder przekrecony w prawo
   * EN: Encoder turned right
   *
   */
  if ( (EncoderCurrent > EncoderLast) || ( EncoderCurrent == INT_MIN && EncoderLast == INT_MAX ) ) {
      if (MenuSub == 0) {
      // PL: Menu glowne
      // EN: Main menu
        // PL: Zawijanie menu
	// EN: Menu overlap
        ( MenuPos == CountListItems(MenuTopLevel)-1 ) ? MenuPos = 1 : MenuPos++;
        // PL: Wyswietlenie aktualnej pozycji
	// EN: Display current position
        MenuBarTemp = MenuTopLevel[MenuPos];
        DisplayMenuBar();
      } else
      // PL: Podmenu 1 - glowica pomiarowa
      // EN: Submenu 1 - power head
      if (MenuSub == 1) {
        ( MenuPos == CountListItems(MenuSubLevel1)-1 ) ? MenuPos = 0 : MenuPos++;
        MenuBarTemp = MenuSubLevel1[MenuPos];
        DisplayMenuBar();
      } else
      // PL: Podmenu 2 - korekcja czestotliwosci
      // EN: Submenu 2 - frequency correction
      if (MenuSub == 2) {
        ( MenuPos == CountListItems(MenuSubLevel2)-1 ) ? MenuPos = 0 : MenuPos++;
        MenuBarTemp = MenuSubLevel2[MenuPos];
        DisplayMenuBar();
      } else 
      // PL: Podmenu 3 - zerowanie
      // EN: Submenu 3 - zeroing
      if (MenuSub == 3) {
        ( MenuPos == CountListItems(MenuSubLevel3)-1 ) ? MenuPos = 0 : MenuPos++;
        MenuBarTemp = MenuSubLevel3[MenuPos];
        DisplayMenuBar();
      } else
      // PL: Podmenu 4 - wyswietlane wartosci
      // EN: Submenu 4 - displayed values
      if (MenuSub == 4) {
        ( MenuPos == CountListItems(MenuSubLevel4) - 1 ) ? MenuPos = 0 : MenuPos++;
        MenuBarTemp = MenuSubLevel4[MenuPos];
        DisplayMenuBar();
      }   
    } else  

  /*
   * PL: Enkoder przekrecony w lewo
   * EN: Encoder turned left
   *
   */
   if ( (EncoderCurrent < EncoderLast) || (EncoderCurrent == INT_MAX && EncoderLast == INT_MIN ) ) {
     if (MenuSub == 0) {
      // PL: Menu glowne
      // EN: Main menu
        // PL: Zawijanie menu
	// EN: Menu overlap
       ( MenuPos <= 1 ) ? MenuPos = CountListItems(MenuTopLevel)-1 : MenuPos--;
       MenuBarTemp = MenuTopLevel[MenuPos];
       DisplayMenuBar();
     } else
     if (MenuSub == 1) {
      // PL: Podmenu 1 - glowica pomiarowa
      // EN: Submenu 1 - power head
       ( MenuPos == 0 ) ? MenuPos = CountListItems(MenuSubLevel1)-1 : MenuPos--;
       MenuBarTemp = MenuSubLevel1[MenuPos];
       DisplayMenuBar();
     } else
     if (MenuSub == 2) {
      // PL: Podmenu 2 - korekcja czestotliwosci
      // EN: Submenu 2 - frequency correction
       ( MenuPos == 0 ) ? MenuPos = CountListItems(MenuSubLevel2)-1 : MenuPos--;
       MenuBarTemp = MenuSubLevel2[MenuPos];
       DisplayMenuBar();
     } else 
     if (MenuSub == 3) {
      // PL: Podmenu 3 - zerowanie
      // EN: Submenu 3 - zeroing
       ( MenuPos == 0 ) ? MenuPos = CountListItems(MenuSubLevel3)-1 : MenuPos--;
       MenuBarTemp = MenuSubLevel3[MenuPos];
       DisplayMenuBar();
     } else 
     if (MenuSub == 4) {
      // PL: Podmenu 4 - wyswietlane wartosci
      // EN: Submenu 4 - displayed values
       ( MenuPos == 0 ) ? MenuPos = CountListItems(MenuSubLevel4)-1 : MenuPos--;
       MenuBarTemp = MenuSubLevel4[MenuPos];
       DisplayMenuBar();
     }
  }
  
  // PL: Przepisanie wartosci do sprawdzenia w nastepnym wywolaniu petli glownej
  // EN: Value saved for next cycle check
  EncoderLast = EncoderCurrent;
  
  // PL: Obsluga przycisku
  // EN: Button actions
  ClickEncoder::Button EncoderButton = Encoder -> getButton();
  if (EncoderButton != ClickEncoder::Open){
    switch (EncoderButton) {
      case ClickEncoder::Pressed:
        break;
      case ClickEncoder::Held:
        break;
      case ClickEncoder::Released:
        break;
      case ClickEncoder::Clicked:
        // PL: W podmenu 2 wybieramy czestotliwosc do korekcji
	// EN: In submenu 2 we pick up frequency for correction
        if ( MenuSub == 2 ) { ADC0CorrFactor = MenuSubLevel2Values[MenuPos]; }
        
        // PL: W podmenu 3 pozycja Off zeruje wartosc odniesienia, pozycja Sample ustawia biezaca
	// PL: wartosc jako odniesienie
	// EN: In submenu 3 position Off zeroes reference power level, position Sample probes input
	// EN: for new reference value
        // wartosc ADC jako wartosc odniesienia
        if ( MenuSub == 3 ) { ( MenuPos == 0 ) ? ADC0RefValue = 0 : ADC0RefValue = ADCRead(ADC0, 5); }
        
        // PL: Podmenu 4 - wybieramy typ prezentacji wartosci
	// EN: Submenu 4 - we choose value presentation method
        if ( MenuSub == 4 ) { PowerDisplayType = MenuPos; }
        
        // PL: Przelaczamy pozycje menu w zaleznosci od miejsca, w ktorym klikniemy enkoderem
	// EN: Position switch depending on where we were while clicking
        switch (MenuPos) {
          case 0:
	  // PL: Menu glowne na pozycji 0 - przechodzimy do normalnej pracy
	  // EN: Top menu, position 0 - commencing normal work
            if ( MenuSub == 0 ) {
              MenuPos = 1;
              MenuBarTemp = MenuTopLevel[MenuPos];
	  // PL: Jestesmy w podmenu - wychodzimy
	  // EN: We're in submenu - go back
            } else {
              MenuPos = MenuSub;
              MenuSub = 0;
              MenuBarTemp = MenuTopLevel[MenuPos];
            }
            DisplayMenuBar();
            break;
          case 1:
	  // PL: Wybieramy wartosc i wychodzimy wyzej
	  // EN: We choose the value and go level up
            if ( MenuSub == 0 ) {  
              MenuSub = MenuPos;
              MenuPos = 0;
              MenuBarTemp = MenuSubLevel1[MenuPos];
	  // PL: Jestesmy w podmenu - wychodzimy
	  // EN: We're in submenu - go back
            } else {
              MenuPos = MenuSub;
              MenuSub = 0;
              MenuBarTemp = MenuTopLevel[MenuPos];
            } 
            DisplayMenuBar();
            break;
          case 2:
	  // PL: Wybieramy wartosc i wychodzimy wyzej
	  // EN: We choose the value and go level up
            if ( MenuSub == 0 ) {
              MenuSub = MenuPos;
              MenuPos = 0;
              MenuBarTemp = MenuSubLevel2[MenuPos];
	  // PL: Jestesmy w podmenu - wychodzimy
	  // EN: We're in submenu - go back
            } else {
              MenuPos = MenuSub;
              MenuSub = 0;
              MenuBarTemp = MenuTopLevel[MenuPos];
            }
            DisplayMenuBar();
            break;
          case 3:
	  // PL: Wybieramy wartosc i wychodzimy wyzej
	  // EN: We choose the value and go level up
            if ( MenuSub == 0 ) {
              MenuSub = MenuPos;
              MenuPos = 0;
              MenuBarTemp = MenuSubLevel3[MenuPos];
	  // PL: Jestesmy w podmenu - wychodzimy
	  // EN: We're in submenu - go back
            } else {
              MenuPos = MenuSub;
              MenuSub = 0;
              MenuBarTemp = MenuTopLevel[MenuPos];
            }
            DisplayMenuBar();
            break;
          case 4:
	  // PL: Wybieramy wartosc i wychodzimy wyzej
	  // EN: We choose the value and go level up
            if ( MenuSub == 0 ) {
              MenuSub = MenuPos;
              MenuPos = 0;
              MenuBarTemp = MenuSubLevel4[MenuPos];
	  // PL: Jestesmy w podmenu - wychodzimy
	  // EN: We're in submenu - go back
            } else {
              MenuPos = MenuSub;
              MenuSub = 0;
              MenuBarTemp = MenuTopLevel[MenuPos];
            }
            DisplayMenuBar();
            break;
          default:
	  // PL: Jestesmy w podmenu - wychodzimy
	  // EN: We're in submenu - go back
            MenuPos = MenuSub;
            MenuSub = 0;
            MenuBarTemp = MenuTopLevel[MenuPos];
            DisplayMenuBar();
            break;
        }
        break;
      case ClickEncoder::DoubleClicked:
        break;
      default:
        break;
    }
  }
}  



// PL: Obliczenie ilosci wpisow w tablicy String
// EN: Count items number in Strig table
int CountListItems(String List[]){
  int ListItemsNum = -1;
  while (List[++ListItemsNum] != NULL) {};
  return ListItemsNum;
}



// PL: Wypisanie zawartosci linii menu w dolnym wierszu wyswietlacza
// EN: Write bottom row of the display
void DisplayMenuBar() {
  char MenuBarTextChar[16];
  MenuBarTemp.toCharArray(MenuBarTextChar, 16);
  Display.Char_F8x16(0, 6, "                ");
  Display.Char_F8x16(0, 6, MenuBarTextChar);
}



// PL: Odczytanie zadanej ilosci probek z wybranego przetwornika ADC i zwrocenie sredniej
// EN: Read selected number of samples from selected ADC and return average value
float ADCRead(int ADCNumber, int ADCNumSamples) {
  int ADC0value = 0;
  byte ADC0SampleNum = 0;
  while ( ++ADC0SampleNum <= ADCNumSamples ) {
    ADC0value += analogRead(ADCNumber);
  }
  return ( ( ADC0value * 1.0 ) / ADCNumSamples);
}



// PL: Wprowadzenie poprawek do odczytanej wartosci
// EN: Make corrections to the read value
/* TODO: temperature, measured power, correction table from eeprom */
float ADCCorrect(int ADCValue, float CorrFactor, float RefValue) {
  return ( ADCValue * RefValue * CorrFactor ) / ( 1023 * 100.0 );
}



// PL: Wyswietlenie wartosci mocy
// EN: Display power value
// 0 - dBm, 1 - dBV(50R), 2 - dBmV(50R), 3 - dVuV(50R), 4 - V, 5 - dB
// http://www.radiocomms.com.au/articles/53658-Understanding-the-decibel
// dBV = dBm - 13 dB / dBmV = dBm + 47 dB / dBuV = dBm + 107 dB
void DisplayPower(float PowerData, int PowerCorrFactor, byte PowerUnit, int PowerOffset) {
  String PowerValueString;
  float PowerValue;
  switch (PowerUnit) {
    case 0:
    // dBm
      PowerValue = (ADCCorrect(PowerData, PowerCorrFactor, ADCMAXVALUE) - ADCCorrect(PowerOffset, PowerCorrFactor, ADCMAXVALUE)) * 40.0;
      PowerValueString = " " + String(PowerValue) + " dBm ";
      analogWrite(METER_OUT, (PowerValue / 40) * 255 / ADCMAXVALUE);
      break;
    case 1:
    // dBV
      PowerValue = (ADCCorrect(PowerData, PowerCorrFactor, ADCMAXVALUE) - ADCCorrect(PowerOffset, PowerCorrFactor, ADCMAXVALUE)) * 40.0 - 13.0;
      PowerValueString = " " + String(PowerValue) + " dBV ";
      analogWrite(METER_OUT, ((PowerValue + 13) / 40) * 255 / ADCMAXVALUE);
      break;
    case 2:
    // dBmV
      PowerValue = (ADCCorrect(PowerData, PowerCorrFactor, ADCMAXVALUE) - ADCCorrect(PowerOffset, PowerCorrFactor, ADCMAXVALUE)) * 40.0 + 47.0;
      PowerValueString = " " + String(PowerValue) + " dBmV ";
      analogWrite(METER_OUT, ((PowerValue - 47) / 40) * 255 / ADCMAXVALUE);
      break;
    case 3:
    // dBuV
      PowerValue = (ADCCorrect(PowerData, PowerCorrFactor, ADCMAXVALUE) - ADCCorrect(PowerOffset, PowerCorrFactor, ADCMAXVALUE)) * 40.0 + 107.0;
      PowerValueString = " " + String(PowerValue) + " dBuV ";
      analogWrite(METER_OUT, ((PowerValue - 107) / 40) * 255 / ADCMAXVALUE);
      break;
    case 4:
    // V
      PowerValue = (ADCCorrect(PowerData, PowerCorrFactor, ADCMAXVALUE) - ADCCorrect(PowerOffset, PowerCorrFactor, ADCMAXVALUE));
      PowerValueString = " " + String(PowerValue) + " V ";
      analogWrite(METER_OUT, PowerValue * 255 / ADCMAXVALUE);
      break;
    case 5:
    // dB
      PowerValueString = " " + String(PowerValue) + " dB ";
      break;
  }
  
  // TODO: read correction from power head eeprom
  char PowerValueChar[16] = "";
  PowerValueString.toCharArray(PowerValueChar, 16);

  // PL: wartosc 15 ze wzgledu na dodana poczatkowa spacje do naspidania minusa
  // EN: value 15 because we've added a space at the beginning to overwrite minus sign
  Display.Char_F8x16( (15-PowerValueString.length())*8/2, 3, PowerValueChar);
}



//
// *********** MAIN LOOP **********
//
void loop()
{
  EncoderRun();
  DisplayPower(ADCRead(ADC0, 10), ADC0CorrFactor, PowerDisplayType, ADC0RefValue);
}

// EOF
