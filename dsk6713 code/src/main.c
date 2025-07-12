#include <math.h>
#include "dsk6713.h"
#include "dsk6713_aic23.h"
#include "dsk6713_led.h"

// === Configuration de base du codec audio AIC23 ===
DSK6713_AIC23_CodecHandle hCodec;
DSK6713_AIC23_Config config = DSK6713_AIC23_DEFAULTCONFIG;


// === Déclarations ===
#define PI 3.141592653589793
#define SAMPLE_RATE 8000
#define N 205  // Nombre d'échantillons pour 25 ms à 8 kHz

short buffer[N];
short index = 0;
int i=0;

float dtmf_freqs[] = {697, 770, 852, 941, 1209, 1336, 1477, 1633};
char dtmf_keys[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// === Prototypes ===
void set_gpio(int pin, int value);
void display_dtmf_binary(char key);
float goertzel(short *data, int numSamples, float target_freq, int sample_rate);
short get_sample();

// === Fonctions de gestion des gpio===
void set_gpio(int pin, int value) {
    if (value)
        DSK6713_LED_on(pin);
    else
        DSK6713_LED_off(pin);
}


// === Activation des LED en fonction du signal détecté
void display_dtmf_binary(char key) {
    int value = -1;

    switch (key) {
        case '0': value = 0x0; break;
        case '1': value = 0x1; break;
        case '2': value = 0x2; break;
        case '3': value = 0x3; break;
        case '4': value = 0x4; break;
        case '5': value = 0x5; break;
        case '6': value = 0x6; break;
        case '7': value = 0x7; break;
        case '8': value = 0x8; break;
        case '9': value = 0x9; break;
        case 'A': value = 0xA; break;
        case 'B': value = 0xB; break;
        case 'C': value = 0xC; break;
        case 'D': value = 0xD; break;
        case '*': value = 0xF; break;
        case '#': value = -1; break;
        default:  value = -1; break;
    }

    if (value >= 0) {
        set_gpio(0, value & 0x01);
        set_gpio(1, (value >> 1) & 0x01);
        set_gpio(2, (value >> 2) & 0x01);
        set_gpio(3, (value >> 3) & 0x01);
    } else {
        set_gpio(0, 0);
        set_gpio(1, 0);
        set_gpio(2, 0);
        set_gpio(3, 0);
    }
}

// === Algorithme de Goertzel pour le traitement du signal ===
float goertzel(short *data, int numSamples, float target_freq, int sample_rate) {
    int i, k;
    float floatnumSamples = (float)numSamples;
    float omega, sine, cosine, coeff, q0 = 0, q1 = 0, q2 = 0;
    float real, imag, result;

    k = (int)(0.5 + ((floatnumSamples * target_freq) / sample_rate));
    omega = (2.0 * PI * k) / floatnumSamples;
    sine = sin(omega);
    cosine = cos(omega);
    coeff = 2.0 * cosine;

    for (i = 0; i < numSamples; i++) {
        q0 = coeff * q1 - q2 + data[i];
        q2 = q1;
        q1 = q0;
    }

    real = (q1 - q2 * cosine);
    imag = (q2 * sine);
    result = sqrtf(real * real + imag * imag);
    return result;
}

// == Détection des fréquences ==
void detect_dtmf() {
    float magnitudes[8];
    for (i = 0; i < 8; i++) {
        magnitudes[i] = goertzel(buffer, N, dtmf_freqs[i], SAMPLE_RATE);
    }

    int row = -1, col = -1;
    float max_low = 0, max_high = 0;

    for (i = 0; i < 4; i++) {
        if (magnitudes[i] > max_low) {
            max_low = magnitudes[i];
            row = i;
        }
    }

    for (i = 4; i < 8; i++) {
        if (magnitudes[i] > max_high) {
            max_high = magnitudes[i];
            col = i - 4;
        }
    }

    if (max_low > 1000 && max_high > 1000) { // seuil à ajuster
        char key = dtmf_keys[row][col];
        display_dtmf_binary(key);
    }
}

// === Fonction pour lire audio par le codec AIC23
short get_sample() {
    short sample;
    while (!MCBSP_rrdy(DSK6713_AIC23_DATAHANDLE)); // Wait for data ready
    sample = MCBSP_read(DSK6713_AIC23_DATAHANDLE);  // Read sample (16-bit)
    return sample;
}

// === Fonction d'initialisation système ===
void init_system() {

    // Initialisation de la carte DSK6713
    DSK6713_init();

    //Initialisation des GPIO
    DSK6713_LED_init();


    // Ouverture du codec avec la configuration par défaut
    hCodec = DSK6713_AIC23_openCodec(0, &config);

    // Réglage de la fréquence d'échantillonnage à 8 kHz
    DSK6713_AIC23_setFreq(hCodec, DSK6713_AIC23_FREQ_8KHZ);
}

// === Main ===
void main() {
    init_system();

    while (1) {
        for (index = 0; index < N; index++) {
            buffer[index] = get_sample();
        }
        detect_dtmf();
    }
}
