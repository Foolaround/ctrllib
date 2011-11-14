/* Beispielprogramm für eine einfache lineare Ablaufsteuerung.
 * Aktoren und Sensoren müssen deklariert werden, Zeitwerde sind fiktiv */

void loop() {
	/* Alle Aktoren in sichere Stellungen bringen */
	UV = OFF;
	PumpeTankNachKessel = OFF;
	Heizung = OFF;
	PumpeKesselNachFlasche = OFF;
	PumpeKaltwasser = OFF;
	PulverMotor = OFF;

	/* Auf Start warten */
	msg("Bitte Knopf druecken", "");
	while(TasterStart != ON)
		;
	msg("Start", "");

	/* 10 Sekunden lang desinfizieren, Sicherheitschecks */
	int i = 10000;
	UV = ON;
	while(i--) {
		delay(1);
		if(TankDeckel == ON) {
			msg("Fehler: Deckel offen", "Das gibt Krebs!");
			return;
		} else if(UVSensor == 0) {
			msg("Fehler: UV defekt", "Das wird nicht sauber");
			return;
		}
	}

	/* Wasser zum erwärmen abmessen und auf 50 Grad temperieren */
	PumpeTankNachKessel = ON;
	delay(10000);
	PumpeTankNachKessel = OFF;
	Heizkartuschen = ON;
	msg("Erwärmen", "auf 50 Grad");
	while(TemperaturKessel < (50))
		;
	Heizkartuschen = OFF;

	/* Abmischen: 
	 * Zuerste Warmwasser, dazu das Pulver
	 * Anschließend Kaltwasser zum temperieren */
	msg("Mischen", "");
	Heizkartuschen = OFF;
	PumpeKesselNachFlasche = ON;
	delay(1000);
	PulverMotor = ON;
	delay(2000);
	PulverMotor = OFF;
	delay(1000);
	PumpeKesselNachFlasche = OFF;
	PumpeKaltwasser = ON;
	delay(3000);
	PumpeKaltwasser = OFF;
	msg("Fertig!", "Wohl bekommts");
	delay(1000);
}
