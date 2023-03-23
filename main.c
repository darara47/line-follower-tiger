/*
 * main.c
 *
 *  Created on: 3 06 2021
 *      Author: Patryk
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#define LED1 PC2
#define LED2 PC1
#define S1 PD6
#define S2 PD7

#define M1a PB2
#define M1b PB3
#define M2a PB1
#define M2b PB0

volatile int petla = 0;
volatile long licznik = 0;

int main( void ) {

	int prog = 100;
	int czujnik[8] = {0};
	int wagi[8] = { -35, -25, -6, -1, 1, 6, 25, 35 };
	int blad = 0;
	int poprzed_blad = 0;
	int ilosc = 0;
	int przestrzelony = 0;
	int predkosc = 120;
	int uchyb=0;
	int kp = 5;
	int kd = 3;
	int PWMa = 0;
	int PWMb = 0;
	int iteracje=0;

	// Inicjalizacja LED
	DDRC  |= (1<<LED1) | (1<<LED2);
	//PORTC  |= (1<<LED1) | (1<<LED2);

	// Inicjalizacja przyciskow
	DDRD  &= ~((1<<S1) | (1<<S2));
	PORTD  &= ~((1<<S1) | (1<<S2));

	// Inicjalizacja Mostka H i PWM
	DDRB |= (1<<M1a) | (1<<M1b) | (1<<M2a) | (1<<M2b);
	PORTB |= (1<<M1a) | (1<<M2a);
	PORTB &= ~((1<<M1b) | (1<<M2b));
	DDRD |= (1<<PD4) | (1<<PD5);

	// Reset przerwan
	sei();

	// Inicjalizacja Timer1 (PWM)
	TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
	TCCR1B |= (1<<CS10) | (1 << WGM12);

	// Inicjalizacja Timer0 (1ms/cykl)
	TCCR0 |= (1<<CS00);
	TIMSK |= (1<<TOIE0);

	// Inicjalizacja ADC
	ADMUX |= (1<<REFS0) | (1<<ADLAR);
	ADCSRA |= (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0) | (1<<ADEN);

	OCR1A = OCR1B = 0;

	while (1) {

		if (petla) {

			// Przyciski
			if (!(PIND & (1<<S1))) {
				predkosc += 10 ;
				PORTC |= (1 << LED2);
				_delay_ms(60);
				PORTC &= ~(1 << LED2);
				_delay_ms(60);
				PORTC |= (1 << LED2);
				_delay_ms(60);
				PORTC &= ~(1 << LED2);
				_delay_ms(60);
				PORTC |= (1 << LED2);
				_delay_ms(150);
				PORTC &= ~(1 << LED2);
				_delay_ms(600);
				iteracje=0;
			}
			if (!(PIND & (1 << S2))){
				kp += 1;
				PORTC |= (1 << LED2);
				_delay_ms(60);
				PORTC &= ~(1 << LED2);
				_delay_ms(60);
				PORTC |= (1 << LED2);
				_delay_ms(60);
				PORTC &= ~(1 << LED2);
				_delay_ms(60);
				PORTC |= (1 << LED2);
				_delay_ms(150);
				PORTC &= ~(1 << LED2);
				_delay_ms(600);
				iteracje=0;
			}

			// LEDY
			if ((iteracje % 500) < 360)
				PORTC |= (1 << LED2);
			else
				PORTC &= ~(1 << LED2);

			// Odczyt czujnikow
			for (int i = 0; i < 8; i++) {
				ADMUX &= 0b11100000;
				ADMUX |= i;
				ADCSRA |= _BV(ADSC);
				while (ADCSRA & _BV(ADSC)) {};

				if (ADCH < prog)
					czujnik[i] = 1;
				else
					czujnik[i] = 0;
			}

			// Obliczanie bledu
			poprzed_blad = blad;
			blad = 0;
			ilosc = 0;
			for (int i = 0; i < 8; i++) {
				blad += czujnik[i] * wagi[i];
				ilosc += czujnik[i];
			}
			blad /= ilosc;

			// Sygnalizacja polozenia
			if (ilosc) {
				PORTC |= (1 << LED1);
				if (-15 < blad && blad < 15)
					przestrzelony = 0;
			}
			else {
				PORTC &= ~(1 << LED1);
				if (przestrzelony == 0) {
					if (poprzed_blad < 0)	przestrzelony = 1;
					else if (poprzed_blad > 0) przestrzelony = 3;
				}
			}

			// Regulator
			uchyb = kp * blad + kd * (blad - poprzed_blad);

			// Sterowanie
			if (przestrzelony == 0) {
				if (uchyb > 0) {
					PWMa = predkosc + uchyb / 2;
					PWMb = predkosc - uchyb;
				} else if (uchyb < 0) {
					PWMa = predkosc + uchyb;
					PWMb = predkosc - uchyb / 2;
				} else {
					PWMa = predkosc + uchyb;
					PWMb = predkosc - uchyb;
				}
			} else if (przestrzelony == 1) {
				PWMa = -40;
				PWMb = 100;
			} else if (przestrzelony == 3) {
				PWMa = 100;
				PWMb = -40;
			}

			// Konfiguracja Mostka H
			if (PWMa<0) {
				PORTB |= (1<<M1b);
				PORTB &= ~(1<<M1a);
				OCR1A = -1 * PWMa;
			} else {
				PORTB |= (1<<M1a);
				PORTB &= ~(1<<M1b);
				OCR1A = PWMa;
			}
			if (PWMb<0){
				PORTB |= (1<<M2b);
				PORTB &= ~(1<<M2a);
				OCR1B = -1 * PWMb;
			} else {
				PORTB |= (1<<M2a);
				PORTB &= ~(1<<M2b);
				OCR1B = PWMb;
			}

			if (iteracje >= 1000)
				iteracje = 0;
			iteracje++;
			petla = 0;
		}
	}

}

ISR(TIMER0_OVF_vect) {
	licznik++;
	if (licznik >= 63) {
		licznik = 0;
		petla = 1;
	}
}


