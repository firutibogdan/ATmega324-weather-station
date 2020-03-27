#ifndef ADC_H_
#define ADC_H_

#include <avr/io.h>
#include <stdio.h>

void ADC_init(void);

/*
 * Functia porneste o noua conversie pentru canalul precizat.
 * In modul fara intreruperi, apelul functiei este blocant. Aceasta se
 * intoarce cand conversia este finalizata.
 *
 * In modul cu intreruperi apelul NU este blocant.
 *
 * @return Valoarea numerica raw citita de ADC de pe canlul specificat.
 */
uint16_t ADC_get(uint8_t channel);

/*
 * Functia primeste o valoare numerica raw citita de convertul Analog-Digital
 * si calculeaza tensiunea (in volti) pe baza tensiunei de referinta.
 */
double ADC_voltage(int raw);


#endif // ADC_H_
