/*
 Name:		Sketch1.ino
 Created:	12/17/2016 11:22:40 AM
 Author:	Lars
*/

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#define MINPRESSURE 200
#define MAXPRESSURE 1000
#include "FreeDefaultFonts.h"

// ALL Touch panels and wiring is DIFFERENT
// copy-paste results from TouchScreen_Calibr_native.ino
const int XP = 8, XM = A2, YP = A3, YM = 9;  //ID=0x9341
// const int TS_LEFT = 191, TS_RT = 913, TS_TOP = 969, TS_BOT = 195; // PORTRAIT
const int TS_LEFT = 969, TS_RT = 195, TS_TOP = 904, TS_BOT = 101;  // LANDSCAPE

MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

Adafruit_GFX_Button start_btn;

volatile int pixel_x, pixel_y;  //Touch_getXY() updates global vars

#define RELAEPIN 48
#define OC1A_OUTPUT 44

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

volatile int count = 0;
volatile int timer_count = -1;
volatile int oldCount = 0;
volatile int oldTimer_count = -1;
volatile int timer_ovf = 0;
volatile int batteryTimerCount = 0;

const uint32_t F_CPU_HZ  = 16000000UL;
const uint16_t PRESCALER = 1024;
const int freqArray[] = { 50, 100, 150, 200, 250, 300, 350, 400, 425, 450, 475, 500, 525, 550, 575, 600 };
const int ocr5aArray[] = { 15624, 7812, 5208, 3906, 3125, 2604, 2234, 1953, 1837, 1736, 1641, 1563, 1487, 1420, 1361, 1302 };
//const int ocr1aArray[] = { 31249, 15624, 10416, 7812, 6249, 5207, 4464, 3905, 3674, 3471, 3289, 3124, 2974, 2840, 2716, 2603 };

unsigned char ticks = 0, tiere = 0, enere = 0, loop_ticks = 255;
char buf[25];
int trigger = 0;
volatile const char CounterV = 256 - 125;

volatile bool isRunning = false;
volatile bool cnt = false;
const int analogPin = A15;
volatile bool batteryStatus = false;

uint16_t calcOCR_phase(float freq) {
  // Phase-Correct: f = F_CPU / (2·PRESCALER·(TOP+1))
  float top = F_CPU_HZ / (2.0f * PRESCALER * freq) - 1.0f;
  return uint16_t(top + 0.5f);
}

void timer_5_init() {
	// set compare match register to desired timer count:
	OCR5A = ocr5aArray[0];

	Serial.println("Init Timer 5");

	//TCCR5A = 0x00;

	// turn on CTC mode:
	TCCR5A = 0x00;
	TCCR5B |= (1 << WGM52);

	// Set CS10 and CS12 bits for 1024 prescaler:
	TCCR5B |= (1 << CS50);
	TCCR5B |= (1 << CS52);

	// enable timer compare interrupt:
	TIMSK5 |= (1 << OCIE5A);
}

void timer_3_init() {
	Serial.println("Init Timer 3");
	  TCCR3A = 0;
	TCCR3B = 0;
	TCNT3 = 0;

	// CTC mode
	TCCR3B |= (1 << WGM32);

	// Prescaler = 1024
	TCCR3B |= (1 << CS31); // | (1 << CS30);

	// 16 MHz / 1024 = 15.625 ticks/ms → 33,3 ms = ca. 5208 ticks
	OCR3A = 39999;

	// Enable Compare Match interrupt
	TIMSK3 |= (1 << OCIE3A);
}

void timer_1_init() {
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;

	// 5 sek = 5 × 15.625 = 78125 ticks → OCR1A = 78125
	OCR1A = 78124;  // én mindre da tælleren starter fra 0

	TCCR1B |= (1 << WGM12);               // CTC mode
	TCCR1B |= (1 << CS12) | (1 << CS10);  // Prescaler = 1024
	TIMSK1 |= (1 << OCIE1A);              // Aktiver Compare Match interrupt
}

void init_control() {
	cli();
	isRunning = false;
	cnt = true;

	// Timer 1
	//timer_1_init();
	//timer_3_init();
	timer_5_init();

	timer_count = 0;
	//oldTimer_count = -1;
	timer_ovf = 0;
	loop_ticks = 255;

	count = 0;
	//oldCount = 0;

	TCNT5 = 0;
	TCNT3 = 0;
	sei();
}

volatile bool isRising = false;
volatile int pulseCount = 0;
volatile bool isFinish = false;

ISR(TIMER5_COMPA_vect) {

	Serial.println("Timer 5 COMPA");

	digitalWrite(RELAEPIN, !digitalRead(RELAEPIN));
	if (isRunning && timer_count < 12)  // Timer_Count = index
	{
		if (digitalRead(RELAEPIN)) {
			count++;
			cnt = true;
		}
	}

	if (digitalRead(RELAEPIN))
		pulseCount++;

	if (pulseCount >= 6) {
		pulseCount = 0;
		timer_count++;

		if (timer_count < 16) {
			OCR5A = ocr5aArray[timer_count];
		} else {
			TIMSK5 &= ~(1 << OCIE5A);
			digitalWrite(RELAEPIN, LOW);
			isFinish = true;
			isRunning = false;
		}
	}

}

volatile int freq = 0;
volatile int oldFreq = 0;

ISR(TIMER3_COMPA_vect) {
	//Serial.println("Timer 3 COMPA");
	if (cnt) {
		int tmpCount = count;
		cnt = false;
		freq = freqArray[(timer_count > 11 ? 11 : timer_count)];

		// Tæller værdi

		showSevenSegXY(225, 145, 1, &FreeSevenSegNumFont, tmpCount, oldCount);

		//Serial.print("Ticks: ");Serial.println(tmpTicks);
		//Serial.print("oldTicks:");Serial.println(oldTicks);
		// Frekvens i Hz

		showSevenSegFreqXY(45, 145, 1, &FreeSevenSegNumFont, freq, oldFreq);
		//Serial.print("Frekvens: ");Serial.println(freqArray[timer_count]);

		oldFreq = freq;
		oldCount = tmpCount;
	}
}

volatile float V_batt = 0.0;
volatile float percent = 0.0;
volatile float oldPercent = -1.0;

void check_Battery() {
	V_batt = readBatteryVoltage();
	percent = estimateUnderLoad(V_batt);

	Serial.print("Procent: ");
	Serial.println(percent);

	if (oldPercent != percent) {
		if (percent >= 50) {
			tft.fillRect(345, 80, 110, 80, GREEN);
		} else if (percent >= 20) {
			tft.fillRect(345, 80, 110, 80, YELLOW);
		} else
			tft.fillRect(345, 80, 110, 80, RED);

		tft.setFont();
		tft.setTextColor(BLACK);
		tft.setTextSize(2);
		tft.setCursor(355, 100);
		tft.print((percent < 100 ? (percent < 10 ? "  " : " ") : "") + String(percent, 1) + "%");
		tft.setCursor(355, 120);
		tft.print("  " + String(V_batt, 2) + "V");
	}
	oldPercent = percent;
}

ISR(TIMER1_COMPA_vect) {
	batteryTimerCount++;

//	if (batteryTimerCount >= 30) {  // 6 × 5 sek = 30 sekunder
		batteryTimerCount = 0;
		batteryStatus = true;

		//check_Battery();
//	}
}

bool Touch_getXY(void) {
	TSPoint p = ts.getPoint();
	pinMode(YP, OUTPUT);  //restore shared pins
	pinMode(XM, OUTPUT);
	digitalWrite(YP, HIGH);  //because TFT control pins
	digitalWrite(XM, HIGH);
	bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
	if (pressed) {
		pixel_x = map(p.y, TS_LEFT, TS_RT, 0, 480);  //tft.width()); //.kbv makes sense to me
		pixel_y = map(p.x, TS_TOP, TS_BOT, 0, 320);  //tft.height());
	}
	return pressed;
}

bool down;

void showmsgXY(int x, int y, int sz, const GFXfont *f, const char *msg) {
	int16_t x1, y1;
	uint16_t wid, ht;
	//tft.drawFastHLine(0, y, tft.width(), WHITE);
	tft.setFont(f);
	tft.setCursor(x, y);
	tft.setTextColor(WHITE);
	tft.setTextSize(sz);
	tft.print(msg);
}

void showSevenSegXY(int x, int y, int sz, const GFXfont *f, const unsigned int count, const int oldCount) {
	char buf[10], buffer[10];
	int16_t x1, y1;
	uint16_t wid, ht;
	//tft.drawFastHLine(0, y, tft.width(), WHITE);
	tft.setFont(f);
	//tft.setCursor(x, y);
	//tft.setTextColor(WHITE);
	//tft.setTextSize(sz);

	sprintf(buf, "%03d", oldCount);
	//Serial.println(buf);
	sprintf(buffer, "%03d", count);
	//Serial.println(buffer);

	for (int i = 0; buf[i] != '\0'; i++) {
		if (buf[i] != buffer[i])
			tft.drawChar(x + (i * 32), y, buf[i], RED, RED, sz);
		tft.drawChar(x + (i * 32), y, (i == 0 || i == 1 ? ((i == 0 || i == 1 && buffer[0] == '0') && buffer[i] == '0' ? ' ' : buffer[i]) : buffer[i]), WHITE, RED, sz);
	}
}

void showSevenSegFreqXY(int x, int y, int sz, const GFXfont *f, const unsigned int count, int oldCount) {
	char buf[10], buffer[10];
	int16_t x1, y1;
	uint16_t wid, ht;
	//tft.drawFastHLine(0, y, tft.width(), WHITE);
	tft.setFont(f);
	//tft.setCursor(x, y);
	//tft.setTextColor(WHITE);
	//tft.setTextSize(sz);
	sprintf(buf, "%03d", oldCount);
	//Serial.println(buf);
	sprintf(buffer, "%03d", count);
	//Serial.println(buffer);

	for (int i = 0; buf[i] != '\0'; i++) {
		if (buf[i] != buffer[i])
			tft.drawChar(x + i * 32, y, buf[i], RED, RED, sz);
		tft.drawChar(x + i * 32, y, buffer[i], WHITE, RED, sz);
	}
}

void updateDisplay() {
	if (cnt) {
		int tmpCount = count;
		cnt = false;
		freq = freqArray[(timer_count > 11 ? 11 : timer_count)];

		// Tæller værdi

		showSevenSegXY(225, 145, 1, &FreeSevenSegNumFont, tmpCount, oldCount);

		//Serial.print("Ticks: ");Serial.println(tmpTicks);
		//Serial.print("oldTicks:");Serial.println(oldTicks);
		// Frekvens i Hz

		showSevenSegFreqXY(45, 145, 1, &FreeSevenSegNumFont, freq, oldFreq);
		//Serial.print("Frekvens: ");Serial.println(freqArray[timer_count]);

		oldFreq = freq;
		oldCount = tmpCount;
	}
}

void setup(void) {
	Serial.begin(9600);

	pinMode(RELAEPIN, OUTPUT);
	digitalWrite(RELAEPIN, LOW);
	pinMode(OC1A_OUTPUT, OUTPUT);
	digitalWrite(OC1A_OUTPUT, LOW);

	pinMode(analogPin, INPUT);

	analogReference(DEFAULT);

	// initialize Timer1
	TCCR5A = 0;  // set entire TCCR1A register to 0
	TCCR5B = 0;  // same for TCCR1B

	Serial.println("Setup...");

	uint16_t ID = tft.readID();
	Serial.print("TFT ID = 0x");
	Serial.println(ID, HEX);
	Serial.println("Calibrate for your Touch Panel");
	if (ID == 0xD3D3)
		ID = 0x9486;  // write-only shield
	tft.begin(ID);
	tft.setRotation(1);  //PORTRAIT
	tft.fillScreen(BLACK);
	tft.setFont();
	start_btn.initButton(&tft, 255, 280, 120, 60, WHITE, CYAN, BLACK, "START", 3);
	start_btn.drawButton();
	tft.fillRect(40, 80, 165, 80, RED);

	showSevenSegFreqXY(45, 145, 1, &FreeSevenSegNumFont, 0, 0);
	showmsgXY(64, 148, 2, &FreeBigFont, ".");
	showmsgXY(145, 151, 4, &FreeSmallFont, "H");
	showmsgXY(171, 151, 4, &FreeSmallFont, "z");

	tft.fillRect(220, 80, 110, 80, RED);
	showmsgXY(50, 75, 2, &FreeSmallFont, "Frekvens");
	showmsgXY(220, 75, 2, &FreeSmallFont, "Counter");
	showSevenSegXY(225, 145, 1, &FreeSevenSegNumFont, 0, 0);

	showmsgXY(345, 75, 2, &FreeSmallFont, "Batteri");
	check_Battery();
	timer_1_init();
	sei();
}

/* one button is quite simple
*/
void loop(void) {
	if (batteryStatus) {
		batteryStatus = false;

		check_Battery();
	}

	updateDisplay();
	
	if (!isRunning) {
		if (isFinish) {
			tft.setFont();
			start_btn.drawButton();
			ticks = 0;
			isFinish = false;
			sei();
		}

		down = Touch_getXY();
		start_btn.press(down && start_btn.contains(pixel_x, pixel_y));

		if (start_btn.justPressed()) {
			tft.setFont();
			start_btn.drawButton(true);

			ticks = 0;

			Serial.println("Opstart af test...");
			Serial.println("Kontrollen startet...");
			init_control();
			isRunning = true;
		}
	}
}


/*
	Batteri + -------- R1 ------------ R2 ------- -
	                            |
															|
															|
													Arduino A17 
*/

const float R1 = 47000.0;
const float R2 = 47000.0;
/*
void loop() {

  Serial.print("🔋 Belastet spænding: ");
  Serial.print(V_batt);
  Serial.print(" V | Estimeret %: ");
  Serial.print(percent);
  Serial.println("%");

  delay(2000);
}
*/
float readBatteryVoltage() {
	int raw = analogRead(analogPin);
	float measured = raw * (5.0 / 1023.0);
	return measured * ((R1 + R2) / R2);
}

// Tilpasset til belastningsfald — fx ved 0.2 A
float estimateUnderLoad(float voltage) {
	if (voltage >= 4.05) return 100;
	else if (voltage >= 4.02) return 95;
	else if (voltage >= 3.96) return 90;
	else if (voltage >= 3.92) return 85;
	else if (voltage >= 3.88) return 80;
	else if (voltage >= 3.84) return 75;
	else if (voltage >= 3.80) return 70;
	else if (voltage >= 3.76) return 65;
	else if (voltage >= 3.72) return 60;
	else if (voltage >= 3.68) return 55;
	else if (voltage >= 3.65) return 50;
	else if (voltage >= 3.60) return 45;
	else if (voltage >= 3.50) return 40;
	else if (voltage >= 3.44) return 35;
	else if (voltage >= 3.36) return 30;
	else if (voltage >= 3.28) return 25;
	else if (voltage >= 3.20) return 20;
	else if (voltage >= 3.13) return 15;
	else if (voltage >= 3.06) return 10;
	else if (voltage >= 3.02) return 5;
	else return 0;
}
