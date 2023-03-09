#include <DisplayV2.h>
#include <Arduino.h>
#include <string.h>

#define BATTERY_CHAR			0x9F
#define EMPTY_BATTERY_CHAR		0x78

#define SELFTEST_DELAY			100
#define BRIGHT_KOEFF_K			13.5
#define BRIGHT_KOEFF_B			170
#define VIN_PIN					A7
#define DIV_R1					10
#define DIV_R2					1
#define VREF					1.1
#define FILTER_K				0.5

#define ENC_RESOLUTION			2	// 20um per pulse

float expRunningAverage(float newVal) {
	static float filVal = 0;
	filVal += (newVal - filVal) * FILTER_K;
	return filVal;
}

//**************ENCODER*DEFINITION***********//

// A - D2, interrupt 0
// B - D3, interrupt 1

volatile long		curr_pos	= 0; // curr_pos
volatile long		last_pos	= 0; // last_pos
volatile bool		encFlag		= 0; // update flag
volatile uint8_t	reset		= 0;
volatile uint8_t	last		= 0;

void enc_setup() {
	attachInterrupt(0, encIsr, CHANGE);
	attachInterrupt(1, encIsr, CHANGE);
}

void encIsr() {	
	uint8_t state = (PIND & 0b1100) >> 2;	// D2 + D3
	if (reset && state == 0b11) {
		long prevCount = 		curr_pos;
		if (last == 0b10)			curr_pos++;
		else if (last == 0b01)		curr_pos--;
		if (prevCount != curr_pos)	encFlag = 1;		
		reset = 0;
	}
	if (!state) reset = 1;
	last = state;
}

//**************ENCODER*DEFINITION***********//


//**************DISPLAY*DEFINITION***********//

/*
 0123456789ABCDEF
|================|
|D00000.00mm    B|	
|C000000  L000000|	
|================|  //*/

#define SCREEN_INIT_DELAY		500
#define HELLO_SCREEN_SHOWTIME	5000

Screen helloScreen(
	(dataBlock[]){
		{"frst",	"%16s",			 0,	0},
		{"scnd",	"%16s",			 0,	1},
	{NULL}});

Screen mainScreen(
	(dataBlock[]){
		{"delta",	"D%08.2fmm    ",	0,	0},
		{"curr",	"C%06li  ",			0,	1},
		{"last",	"L%06li",			9,	1},
		{"batt",	"%c",				15,	0},
	{NULL}});

void screens_init() {
	helloScreen.init();
	mainScreen.init();

	helloScreen.updateVal("frst",	"   ENC Tester");
	helloScreen.updateVal("scnd",	" Version 1.3.2");
	mainScreen.updateVal("delta", 	0, 0);
	mainScreen.updateVal("curr",	0);
	mainScreen.updateVal("last",	0);
	mainScreen.updateVal("batt",	BATTERY_CHAR);
}

void update_delta() {
	long delta_10um = (curr_pos - last_pos) * ENC_RESOLUTION;

	mainScreen.updateVal("delta", (float)(delta_10um / 100));
}

void update_current() {
	mainScreen.updateVal("curr", curr_pos);
	update_delta();
}

void update_last() {
	mainScreen.updateVal("last", last_pos);
	update_delta();
}


//**************DISPLAY*DEFINITION***********//

//**************OTHER*FUNCTIONS**************//

void remember_pos() {
	last_pos = curr_pos;
	update_last();
}

void reset_position() {
	curr_pos = 0;
	update_current();
	remember_pos();
}

void change_backlight() {
	setBacklight(!getBacklight());
}


void printing() {
	if (encFlag) {
		encFlag = false;
		update_current();
	}
}

void selftest_setup() {
	analogReference(INTERNAL);
}

void selftest() {
	static unsigned long last_selftest = 0;
	if (millis() - last_selftest > SELFTEST_DELAY) {
		last_selftest = millis();

		float voltage = expRunningAverage(float(analogRead(VIN_PIN) * VREF * (DIV_R1 + DIV_R2) / (DIV_R2 * 1023.0)));
		int battery_state = int(voltage * 1.07 - 5.643);

		setBrightness(uint8_t(BRIGHT_KOEFF_B - int(BRIGHT_KOEFF_K * voltage)));

		if (battery_state < 0 || battery_state > 4) {
			mainScreen.updateVal("batt",	EMPTY_BATTERY_CHAR);
		} else {
			mainScreen.updateVal("batt",	BATTERY_CHAR - battery_state);
		} 
	}
}

//**************OTHER*FUNCTIONS**************//


//**************BUTTONS*DEFINITION***********//

#define START_BUTTON	4
#define	NEXT_BUTTON		A1
#define	BACK_BUTTON		A0

#define BUTTON_DELAY	70

void butt_setup() {
	pinMode(START_BUTTON,	INPUT_PULLUP);
	pinMode(NEXT_BUTTON,	INPUT_PULLUP);
	pinMode(BACK_BUTTON,	INPUT_PULLUP);
}	

void butt_thread() {
	static unsigned long last_butt_millis = 0;
	if (millis() - last_butt_millis > BUTTON_DELAY) {
		last_butt_millis = millis();

		bool start_state		= !digitalRead(A0);
		bool next_state			= !digitalRead(A1);
		bool back_state			= !digitalRead(4);
		static bool start_trig	= false;
		static bool next_trig	= false;
		static bool back_trig	= false;

		if ( start_state && !start_trig) { start_trig = true;
			reset_position();
		}
		if (!start_state &&  start_trig) { start_trig = false;
		}

		if ( next_state && !next_trig) { next_trig = true;
			remember_pos();
		}
		if (!next_state &&  next_trig) { next_trig = false;
		}

		if ( back_state && !back_trig) { back_trig = true;
			change_backlight();
		}
		if (!back_state &&  back_trig) { back_trig = false;
		}
	}
}

//**************BUTTONS*DEFINITION***********//


void setup() {
	selftest_setup();
	enc_setup();
	butt_setup();

	DISPLAY_SETUP(true);
	screens_init();
	delay(SCREEN_INIT_DELAY);
	setScreen(&helloScreen);
	delay(HELLO_SCREEN_SHOWTIME);
	setBacklight(false);
	setScreen(&mainScreen);
}
	
void loop() {
	selftest();
	printing();
	butt_thread();

	DISPLAY_TASK();
}
