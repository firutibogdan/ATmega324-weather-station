#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* Tensiunea de referinta utilizata de ADC. */
#define ADC_AREF_VOLTAGE 5


/* Numele canalelor de ADC. */
typedef struct
{
  const char *name;
  uint8_t    channel;
} adc_channel_t;

// Senzor de puls conectat la ADC0
const adc_channel_t pulse_channel= {"Pulse", 0};

/*
 * Functia initializeaza convertorul Analog-Digital.
 */
void ADC_init(void)
{
    // enable ADC with:
    // * reference AVCC with external capacitor at AREF pin
    // * without left adjust of conversion result
    // * no auto-trigger
    // * no interrupt
    // * prescaler at 32
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (5 << ADPS0);
}

/*
 * Functia porneste o noua conversie pentru canalul precizat.
 * In modul fara intreruperi, apelul functiei este blocant. Aceasta se
 * intoarce cand conversia este finalizata.
 *
 * In modul cu intreruperi apelul NU este blocant.
 *
 * @return Valoarea numerica raw citita de ADC de pe canlul specificat.
 */
uint16_t ADC_get(uint8_t channel)
{
    // start ADC conversion on "channel"
    // wait for completion
    // return the result
    ADMUX = (ADMUX & ~(0x1f << MUX0)) | channel;

    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC));

    return ADC;
}

/*
 * Functia primeste o valoare numerica raw citita de convertul Analog-Digital
 * si calculeaza tensiunea (in volti) pe baza tensiunii de referinta.
 */
double ADC_voltage(int raw)
{
    (void)raw;
    // TODO 2
    return raw * ADC_AREF_VOLTAGE / 1023.0;
}
