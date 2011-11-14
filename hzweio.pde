/* Steuerungsprogramm HZWEIO
 * IKT M3 - HTW Berlin
 * Robert Jung (c) 2011 r_jung@gmx.de
 * 
 * Steuerung für HWEIO - Getränkeautomaten
 *
 */

#include "ctrllib.h"
#include "hardware.h"
#include <Wire.h>
#include <EEPROM.h>

/* Parameters stored in EEPROM */
Param Vol     ("Vol",      0, 0); // Gesamtvolumen
Param VolW    ("VolW",     2, 0); // Prozent Warmwasser am Gesamtvolumen
Param TWarm   ("TWarm",    4, 0); // Zieltemperatur fürs Heizen
Param TEnd    ("TEnd",     6, 0); // Zielendtemperatur
Param TDrop   ("TDrop",    8, 0); // Temperaturverlust zwischen Kessel und Flasche
Param PreHeat ("PreHeat", 10, 0); // Vorwärmzeit
Param mls     ("mls",     12, 0); // ml pro s
Param UVTime  ("UVTime",  14, 0); // Zeit zum UV - Entkeimen
Param ZWarmMax("ZWMax",   16, 0); // Maximale Zeit zum Erwärmen
Param Pulver  ("Pulver",  18, 0); // Pulvermenge
Param mlst    ("mlst",    20, 0); // ml/s Pumpe Tank -> Kessel

/* Hardware definitions */

/* LEDs */
Hardware LUV     ("LUV",   40, DIGITAL_OUT, NULL, NULL);
Hardware LHeiz   ("LHeiz", 42, DIGITAL_OUT, NULL, NULL);
Hardware LProcess("LProc", 44, DIGITAL_OUT, NULL, NULL);

/* Taster */
Hardware Notaus("Notaus", 32, DIGITAL_IN, NULL, NULL);
Hardware TasterRuehr("TasRue", 38, DIGITAL_IN, NULL, NULL);
Hardware TasterKessel("TasKes", 39, DIGITAL_IN, NULL, NULL);
Hardware TasterKalt("TasKal", 37, DIGITAL_IN, NULL, NULL);



/* Encoder */
Hardware Enc1 ("E1",   (24 << 8) | 26, ENCODER_IN, NULL, NULL);
Hardware BtnE1("BE1",   30, DIGITAL_IN, NULL, NULL);
Hardware EVol ("EVol", (31 << 8) | 29, ENCODER_IN, NULL, NULL);
Hardware BVol ("BVol",  33, DIGITAL_IN, NULL, NULL);
Hardware ETmp ("ETmp", (23 << 8) | 25, ENCODER_IN, NULL, NULL);
Hardware BTmp ("BTmp",  27, DIGITAL_IN, NULL, NULL);
Hardware EPul ("EPul", (36 << 8) | 35, ENCODER_IN, NULL, NULL);
Hardware BPul ("BPul",  34, DIGITAL_IN, NULL, NULL);


/* Temperatursensoren */ 
int pt100(int val)  { return map(val, 15,  75, 19, 100); }
int thermo(int val) { return map(val,  0, 210,  0, 100); }
Hardware TTank   ("TTank",   (0x48<<8 | 0x00), PCF8591_IN, pt100, NULL);
Hardware TKessel ("TKessel", (0x48<<8 | 0x01), PCF8591_IN, pt100, NULL);
Hardware TFlasche("TFlasch", (0x48<<8 | 0x02), PCF8591_IN, pt100, NULL);
Hardware TH1     ("TH1",     (0x49<<8 | 0x00), PCF8591_IN, thermo, NULL);
Hardware TH2     ("TH2",     (0x49<<8 | 0x01), PCF8591_IN, thermo, NULL);

/* Relais */
int buffer_pcf1;
Hardware H1     ("H1",      ((0x20<<8) | 0x00), PCF8574_OUT, NULL, &buffer_pcf1); //010.0XXX
Hardware H2     ("H2",      ((0x20<<8) | 0x01), PCF8574_OUT, NULL, &buffer_pcf1);
Hardware VKalt  ("VKalt",   ((0x20<<8) | 0x02), PCF8574_OUT, NULL, &buffer_pcf1);
Hardware VWarm  ("VWarm",   ((0x20<<8) | 0x03), PCF8574_OUT, NULL, &buffer_pcf1);
Hardware VKessel("VKessel", ((0x20<<8) | 0x04), PCF8574_OUT, NULL, &buffer_pcf1);
Hardware UV     ("UV",      ((0x20<<8) | 0x05), PCF8574_OUT, NULL, &buffer_pcf1);

/* Sonstiges */
Hardware Ruehr   ("Ruehr", 41, DIGITAL_OUT, NULL, NULL);
Hardware Motor   ("Motor",  1, SERVO, NULL, NULL);
Hardware UVSensor("UVSens", (0x4A<<8 | 0x01), PCF8591_IN, NULL, NULL);
Hardware Deckel  ("UVTop",  (0x4A<<8 | 0x00), PCF8591_IN, NULL, NULL);

/* States */
State background("BG");
State mm("MM");
State process("P");
State spuelen("Spuelen");
State testvol("TestVol");
State testdrop("Testdro");


/* State functions */

void idle() { ; }

/* Hauptmenü */
void mm_init_start() { msg("Getraenkeautomat", "Start"); p_safe(); }
void mm_start() {
  if(process != 0 && Notaus == ON) {
    msg("Notaus gedrueckt", "Selbst schuld!");
    process = "Final";
  } else
    mm.menu_enter(Enc1, BtnE1, &process);
}

void mm_init_testvol() { msg(NULL, "Volumen - Test"); }
void mm_testvol() { mm.menu_enter(Enc1, BtnE1, &testvol); }

void mm_init_testdrop() { msg(NULL, "Delta-T - Test"); }
void mm_testdrop() { mm.menu_enter(Enc1, BtnE1, &testdrop); }

void mm_init_spuelen() { msg(NULL, "Spuelen"); }
void mm_spuelen() { mm.menu_enter(Enc1, BtnE1, &spuelen); }

void mm_init_manuell() { msg(NULL, "Manuell"); }
void mm_manuell() { 
  VKalt = TasterKalt;
  VKessel = TasterKessel;
  Ruehr = TasterRuehr;
  /* Volumen einstellen */
  static int16_t vol = 0;
  if(BVol == ON) {
    if(!vol)
      vol = Vol.getValue();
    msg("Wassermenge einstellen", "");
    vol += EVol.getValue();
    if(vol < 100) vol = 100;
    if(vol > 250) vol = 250;
    bar(vol);
  } else if(vol) {
    if(Vol != vol)
      Vol = vol;
    vol = 0;
    msg("Getraenkeautomat", "Manuell");
  }
  
  /* Mischtemperatur einstellen */
  static int16_t tmp = 0;
  if(BTmp == ON) {
    if(!tmp)
      tmp = TWarm.getValue();
    msg("Mischtemperatur?", "");
    tmp += ETmp.getValue();
    if(tmp < 40) tmp = 40;
    if(tmp > 70) tmp = 70;
    bar(tmp);
  } else if(tmp) {
    if(TWarm != tmp)
      TWarm = tmp;
    tmp = 0;
    msg("Getraenkeautomat", "Manuell");
  }
  
  static int16_t pul = 0;
  if(BPul == ON) {
    if(!pul)
      pul = Pulver.getValue();
    msg("Pulvermenge?", "");
    pul += EPul.getValue();
    if(pul < 50) pul = 50;
    if(pul > 100) pul = 100;
    bar(pul);
  } else if(pul) {
    if(Pulver != pul)
      Pulver = pul;
    pul = 0;
    msg("Getraenkeautomat", "Manuell");
  }
  
  mm.menu(Enc1);
}

/* Process */
double ratio_warm = 0.5;
double ratio_kalt = 0.5;

/* Zweipunkt-Regelung für Heizkartuschen */
void heizen() {
  if(H1 == ON && TH1 > 95)
    H1 = OFF;
  else if(TH1 < 90)
    H1 = ON;
  if(H2 == ON && TH2 > 95)
    H2= OFF;
  else if(TH2 < 90)
    H2 = ON;
}
/* Bestimmen der Warm- und Kaltwasseranteile mit Hilfe des Mischkreuzes */
void calculate_ratio() {
  // Messen & Berechnen
  // TWarm: Mischtemperatur, Zieltemperatur im Kessel ist TWarm + TDrop
  ratio_kalt = abs(TWarm.getValue() - TEnd.getValue());
  ratio_warm = abs( TEnd.getValue() - TTank.getValue());
  // Normieren
  ratio_warm = ratio_warm / (ratio_warm + ratio_kalt);
  ratio_kalt = 1.0 - ratio_warm;
  Serial.print("Ratio_kalt: "); Serial.println(ratio_kalt);
  Serial.print("Ratio_warm: "); Serial.println(ratio_warm);
}

void p_init_uv() {
  LProcess = ON;
  UV       = ON;
  LUV      = ON;
  msg("Wasser entkeimen", "");
}
void p_uv() {
  bar((byte) ((process.timer / (UVTime.getValue() * 1000.0)) * 255.0));
  /* Sicherheitsschaltung: UVSensor wird ignoriert da kein UV im Testaufbau verwendet wird */
  if(((UVSensor == OFF && 0) || Deckel > 128)) {
    UV  = OFF;
    LUV = OFF;
    msg("Fehler: Klappe!", "Gibt Krebs...");
    process = "Final";
  }
  if(process.timer > UVTime.getValue() * (long) 1000)
    process = "WWein";
}

void p_init_wwein() {
  UV    = OFF;
  LUV   = OFF;
  VWarm = ON;
  msg("Warmwasser...", "");
  calculate_ratio();
}
void p_wwein() {
  long vol = ((Vol.getValue() * (double) 1000.0 * ratio_warm) / (double) mlst.getValue());
  if(process.timer > vol) {
    process = "Heizen";
  } else if(process.timer > (vol - (PreHeat.getValue()) * 1000)) {
    LHeiz = ON;
    heizen();
  }
  bar((byte) ( ((float) process.timer / vol) * 255.0));
}

void p_init_heiz() {
  VWarm = OFF;
  msg("Erwaermen...", "");
  bar(TKessel.getValue());
  LHeiz = ON;
  H1 = (TH1 > 90) ? OFF : ON;
  H2 = (TH2 > 90) ? OFF : ON;
}
void p_heiz() {
  heizen();

  bar((byte) ( ((float) TKessel.getValue() / (TWarm.getValue() + TDrop.getValue())) * 255.0));
  if(TKessel >= TWarm.getValue() + TDrop.getValue())
    process = "Abfuel";

  if(process.timer > (ZWarmMax.getValue() * (long) 1000) ) {
    msg("Fehler:", "Erwarmen zu langsam");
    process = "Final";
  }
}

void p_init_abfuellen() {
  LHeiz   = OFF;
  H1      = OFF;
  H2      = OFF;
  VKessel = ON;
  VKalt   = ON;
  Motor   = ON;
  msg("Abfuellen", "");
}
void p_abfuellen() {
  double r = (ratio_warm > ratio_kalt) ? ratio_warm : ratio_kalt;
  long vol = (long) (1000.0 * (double) Vol.getValue()) / (double) mls.getValue();
  if(process.timer > (vol * ratio_warm))
    VKessel = OFF;
  if(process.timer > (vol * ratio_kalt))
    VKalt = OFF;
  if(process.timer > (vol * r)) {
    msg("Fertig!", "Knopf druecken");
    process = "Final";
  }
  /* Motor simulieren mit blinkender LED */
  LUV = ((process.timer / 100) % 2);
  bar((byte) ( (process.timer / (vol * r)) * 255.0));
}

/* Alle Aktoren in sichere Zustände setzen */
void p_safe() {
  UV       = OFF;
  LUV      = OFF;
  LHeiz    = OFF;
  LProcess = OFF;
  H1       = OFF;
  H2       = OFF;
  VKalt    = OFF;
  VWarm    = OFF;
  VKessel  = OFF;
  Ruehr    = OFF;
}
void p_end() {
  Ruehr = ON;
  msg("Endtemperatur", NULL);
  if(BtnE1 == ON)
    process = "Final";
  bar(TFlasche.getValue());
}
void p_final() {
  LProcess = OFF;
  process.menu_exit(BtnE1, &mm);
}


/* Spühlprogramm */
void spuelen_init_start() { msg("Getraenkeautomat", "Spuelt..."); VWarm = ON; VKalt = ON; VKessel = OFF; LProcess = ON; }
void spuelen_start() {
  if(spuelen.timer > 3000)
    VKessel = ON;
  if(spuelen.timer < 5000)
    return;
  VWarm = OFF;
  VKalt = OFF;
  if(spuelen.timer < 6000)
    return;
  VKessel = OFF;
  spuelen = 0;
  mm = 0;
}


/* Wassermenge abfüllen zum händischen Messen der Durchflussgeschwindigkeit */
void testvol_init_start() {
  VKalt = ON;
  LProcess = ON; 
  msg("Abfuellen", "");
}
void testvol_start() {
  if(testvol.timer < 20000)
    return;
  VKalt = OFF;
  testvol = 0;
  mm = 0;
}


/* Temperaturverlust für spezifische Wassermenge und Temperatur inkeltem System messen */
void testdrop_init_start() {
  VWarm = ON;
  msg("Abfuellen", "");
}
void testdrop_start() {
  if(testdrop.timer < 30000)
    return;
  VWarm = OFF;
  H1    = ON;
  H2    = ON;
  LHeiz = ON;
  msg("Erwaermen", "");
  testdrop = "Warm";
}
void testdrop_warm() {
  heizen();

  bar((byte) ( ((float) TKessel.getValue() / (TWarm.getValue() + TDrop.getValue())) * 255.0));
  if(TKessel >= TWarm.getValue() + TDrop.getValue()) {
    testdrop = "End";
    VKessel = ON;
  }
}
void testdrop_end() {
  if(testdrop.timer < 15000)
    return;
  p_safe();
  delay(5);
  TDrop = TWarm.getValue() - TFlasche.getValue();
  msg("OK", "TDrop gespeichert");
  delay(1000);
  testdrop = 0;
  mm = 0;
}


/* Setup - Phase der Steuerung */
void setup() {
  /* Initialisierungen */
  Wire.begin();         // I2C - Bus
  Serial.begin(115200); // UART
  init_display();       // Display
  
  /* Zustandsfunktionen registrieren */
  
  /* Hintergrundfunktionen */
  background.register_state("serial", serial, NULL);

  /* Hauptmenü */
  mm.register_state("Start",   mm_start,    mm_init_start);
  mm.register_state("Spuelen", mm_spuelen,  mm_init_spuelen);
  mm.register_state("TestVol", mm_testvol,  mm_init_testvol);
  mm.register_state("TestDro", mm_testdrop, mm_init_testdrop);
  mm.register_state("Hand",    mm_manuell,  mm_init_manuell);
    
  /* Spühlprogramm */
  spuelen.register_state("Idle", idle, NULL);
  spuelen.register_state("Start", spuelen_start, spuelen_init_start);
  
  /* Durchflussmengen - Bestimmung */
  testvol.register_state("Idle", idle, NULL);
  testvol.register_state("Start", testvol_start, testvol_init_start);
  
  /* Temperaturabfall - Bestimmung */
  testdrop.register_state("Idle", idle, NULL);
  testdrop.register_state("Start", testdrop_start, testdrop_init_start);
  testdrop.register_state("Warm", testdrop_warm, NULL);
  testdrop.register_state("End", testdrop_end, NULL);
  
  /* Mischprogramm */
  process.register_state("Idle", idle, p_safe);
  process.register_state("Start", p_uv, p_init_uv);
  process.register_state("WWein", p_wwein, p_init_wwein);
  process.register_state("Heizen", p_heiz, p_init_heiz);
  process.register_state("Abfuel", p_abfuellen, p_init_abfuellen);
  process.register_state("Final", p_final, p_safe);
  process.register_state("Ende", p_end, p_safe);
  
  /* Start - Botschaft */
  msg("Getraenkeautomat", "Willkommen");
  delay(1000);
}

/* Hauptschleife */
void loop() {
  background.execute();
  mm.execute(); 
}
