#include <msp430.h>
#include <driverlib.h>
#include "string.h"

// *****************************************
// ** FUNCTIONS AND CONSTANTS DECLARATION **
// *****************************************

void Init_LCD();
void showChar(char c, int position);
void displayScrollText(char *msg);
void clearLCD();
void increaseValues();
void checkNum();
void play();
void Init_GameVariables();
void showLocalOrGlobalScore(int n);
void showScore();
void gameOverScore();
void Init_GPIO();
void Init_Timer();

#define WELCOME 0
#define STARTUP_MODE 1
#define PLAY_MODE 2
#define SCORE_MODE 3
#define END_PLAY 4
#define FINISH_GAME 5

volatile unsigned int n1, n2, n3, aux1, aux2, aux3; // Variables para mostrar valores máquina
volatile unsigned char mode = WELCOME; // variable estado (modo)
volatile unsigned char S1button_onDebounce = 0; // variables para hacer debounce de los botones
volatile unsigned char S2button_onDebounce = 0;
volatile unsigned int holdCount = 0; // Para cambiar entre modos (cuenta el tiempo que se mantiene pulsado un boton)

volatile int currScore, maxScore = 3;// Puntuación local y global
//volatile unsigned int winSeq = 0, loosSeq = 0;

volatile unsigned int runningPlay = 0; // Quizas pueda cambiarse por PLAY_MODE, pero puede ser útil para funciones SCORE_MODE
volatile unsigned int start_stop;  // Para activar y parar la maquina en PLAY_MODE. Se cambia al soltar el boton, en interrupcion timer

// *****************************************
// **    MAIN FUNCTION IMPLEMENTATION     **
// *****************************************

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5;     // Disable the GPIO power-on default high impedance mode
                              // to active previously configured port settings

    __enable_interrupt();     //enable interrupts

    // Initializations
    Init_GPIO();
    Init_Timer();
    Init_LCD();

    __enable_interrupt();     //enable interrupts

    displayScrollText("WELCOME TO SLOT MACHINE");

    mode = STARTUP_MODE;
    while(mode != FINISH_GAME) {
        switch(mode) {
            case STARTUP_MODE:        // Startup mode
                clearLCD();
                Init_GameVariables();
                displayScrollText("S1 TO PLAY");
                if(mode != STARTUP_MODE) break;
                displayScrollText("S2 TO FINISH");
                if(mode != STARTUP_MODE) break;
                displayScrollText("HOLD BOTH FOR SCORE");
                break;

            case PLAY_MODE:           // Stopwatch Timer mode
                clearLCD();              // Clear all LCD segments
                displayScrollText("STARTING GAME");
                play();
                break;

            case SCORE_MODE:         // Temperature Sensor mode
                clearLCD();              // Clear all LCD segments
                showScore();
                mode = PLAY_MODE;
                break;

            case END_PLAY:             // Temperature Sensor mode
                clearLCD();              // Clear all LCD segments
                gameOverScore();
                mode = STARTUP_MODE;
                break;
        }
    }

    displayScrollText("THANKS FOR PLAYING");

    return 0;
}


// *****************************************
// **       INTERRUPTIONS   METHODS       **
// *****************************************


#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    if (P1IFG & BIT1){ // BOTON1 PULSADO
        P1OUT |= BIT0;    // Turn LED1 On
        if(!S1button_onDebounce){
            // Set debounce flag on first high to low transition
            S1button_onDebounce = 1;
            holdCount = 0;
        }
        P1IFG &= ~BIT1; // Deshabilitamos interrupcion
    }
    else if (P1IFG & BIT2) { // BOTON2 PULSADO
        P9OUT |= BIT7;    // Turn LED2 On
        if(!S1button_onDebounce){
            // Set debounce flag on first high to low transition
            S2button_onDebounce = 1;
            holdCount = 0;
        }
        P1IFG &= ~BIT2; // Deshabilitamos interrupcion
    }
}

/*
 * Timer A0 Interrupt Service Routine
 * Used as button debounce timer
 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR (void)
{
    // Both button S1 & S2 held down
    if (!(P1IN & BIT1) && !(P1IN & BIT2)) {
        holdCount++;
        if (holdCount == 12) {
            // Change mode
            if(mode == PLAY_MODE) mode = SCORE_MODE;
            else if(mode == STARTUP_MODE) mode = SCORE_MODE;
        }
    }
    // Just S1 held down
    else if(!(P1IN & BIT1)) {
        holdCount++;
        if (holdCount == 12) {
            // Change mode
            if (mode == STARTUP_MODE) mode = PLAY_MODE;
        }
    }
    // Just S2 held down
    else if(!(P1IN & BIT2)) {
        holdCount++;
        if (holdCount == 12) {
            // Change mode
            if(mode == STARTUP_MODE) mode = FINISH_GAME;
            else if(mode == PLAY_MODE || mode == SCORE_MODE) mode = END_PLAY;
        }
    }
    // Button S1 released
    if (P1IN & BIT1) {
        if (S1button_onDebounce && mode == PLAY_MODE){
            if(start_stop % 2 == 0 && start_stop != 0) checkNum();
            ++start_stop;
        }
        S1button_onDebounce = 0;
        P1OUT &= ~BIT0;
    }
    // Button S2 released
    if (P1IN & BIT2) {
        S2button_onDebounce = 0;
        P9OUT &= ~BIT7;
    }
    increaseValues();
}

// *****************************************
// **    LCD FUNCTIONS AND IMPLEMENTATION **
// *****************************************

#define pos1 9   /* Digit A1 begins at S18 */
#define pos2 5   /* Digit A2 begins at S10 */
#define pos3 3   /* Digit A3 begins at S6  */
#define pos4 18  /* Digit A4 begins at S36 */
#define pos5 14  /* Digit A5 begins at S28 */
#define pos6 7   /* Digit A6 begins at S14 */

// LCD memory map for numeric digits
const char digit[10][2] =
{
    {0xFC, 0x28},  /* "0" LCD segments a+b+c+d+e+f+k+q */
    {0x60, 0x20},  /* "1" */
    {0xDB, 0x00},  /* "2" */
    {0xF3, 0x00},  /* "3" */
    {0x67, 0x00},  /* "4" */
    {0xB7, 0x00},  /* "5" */
    {0xBF, 0x00},  /* "6" */
    {0xE4, 0x00},  /* "7" */
    {0xFF, 0x00},  /* "8" */
    {0xF7, 0x00}   /* "9" */
};

// LCD memory map for uppercase letters
const char alphabetBig[26][2] =
{
    {0xEF, 0x00},  /* "A" LCD segments a+b+c+e+f+g+m */
    {0xF1, 0x50},  /* "B" */
    {0x9C, 0x00},  /* "C" */
    {0xF0, 0x50},  /* "D" */
    {0x9F, 0x00},  /* "E" */
    {0x8F, 0x00},  /* "F" */
    {0xBD, 0x00},  /* "G" */
    {0x6F, 0x00},  /* "H" */
    {0x90, 0x50},  /* "I" */
    {0x78, 0x00},  /* "J" */
    {0x0E, 0x22},  /* "K" */
    {0x1C, 0x00},  /* "L" */
    {0x6C, 0xA0},  /* "M" */
    {0x6C, 0x82},  /* "N" */
    {0xFC, 0x00},  /* "O" */
    {0xCF, 0x00},  /* "P" */
    {0xFC, 0x02},  /* "Q" */
    {0xCF, 0x02},  /* "R" */
    {0xB7, 0x00},  /* "S" */
    {0x80, 0x50},  /* "T" */
    {0x7C, 0x00},  /* "U" */
    {0x0C, 0x28},  /* "V" */
    {0x6C, 0x0A},  /* "W" */
    {0x00, 0xAA},  /* "X" */
    {0x00, 0xB0},  /* "Y" */
    {0x90, 0x28}   /* "Z" */
};

void Init_LCD()
{
    LCD_C_initParam initParams = {0};
    initParams.clockSource = LCD_C_CLOCKSOURCE_ACLK;
    initParams.clockDivider = LCD_C_CLOCKDIVIDER_1;
    initParams.clockPrescalar = LCD_C_CLOCKPRESCALAR_16;
    initParams.muxRate = LCD_C_4_MUX;
    initParams.waveforms = LCD_C_LOW_POWER_WAVEFORMS;
    initParams.segments = LCD_C_SEGMENTS_ENABLED;

    LCD_C_init(LCD_C_BASE, &initParams);
    // LCD Operation - VLCD generated internally, V2-V4 generated internally, v5 to ground

    /*  'FR6989 LaunchPad LCD1 uses Segments S4, S6-S21, S27-S31 and S35-S39 */
    LCD_C_setPinAsLCDFunctionEx(LCD_C_BASE, LCD_C_SEGMENT_LINE_4,
                                LCD_C_SEGMENT_LINE_4);
    LCD_C_setPinAsLCDFunctionEx(LCD_C_BASE, LCD_C_SEGMENT_LINE_6,
                                LCD_C_SEGMENT_LINE_21);
    LCD_C_setPinAsLCDFunctionEx(LCD_C_BASE, LCD_C_SEGMENT_LINE_27,
                                LCD_C_SEGMENT_LINE_31);
    LCD_C_setPinAsLCDFunctionEx(LCD_C_BASE, LCD_C_SEGMENT_LINE_35,
                                LCD_C_SEGMENT_LINE_39);

    LCD_C_setVLCDSource(LCD_C_BASE, LCD_C_VLCD_GENERATED_INTERNALLY,
                        LCD_C_V2V3V4_GENERATED_INTERNALLY_NOT_SWITCHED_TO_PINS,
                        LCD_C_V5_VSS);

    // Set VLCD voltage to 3.20v
    LCD_C_setVLCDVoltage(LCD_C_BASE,
                         LCD_C_CHARGEPUMP_VOLTAGE_3_02V_OR_2_52VREF);

    // Enable charge pump and select internal reference for it
    LCD_C_enableChargePump(LCD_C_BASE);
    LCD_C_selectChargePumpReference(LCD_C_BASE,
                                    LCD_C_INTERNAL_REFERENCE_VOLTAGE);

    LCD_C_configChargePump(LCD_C_BASE, LCD_C_SYNCHRONIZATION_ENABLED, 0);

    // Clear LCD memory
    LCD_C_clearMemory(LCD_C_BASE);

    //Turn LCD on
    LCD_C_on(LCD_C_BASE);
}


void showChar(char c, int position)
{
    if (c == ' ') {
        // Display space
        LCDMEM[position] = 0;
        LCDMEM[position+1] = 0;
    }

    else if (c >= '0' && c <= '9') {
        // Display digit
        LCDMEM[position] = digit[c-48][0];
        LCDMEM[position+1] = digit[c-48][1];
    }
    else if (c >= 'A' && c <= 'Z') {
        // Display alphabet
        LCDMEM[position] = alphabetBig[c-65][0];
        LCDMEM[position+1] = alphabetBig[c-65][1];
    }
    else {
        // Turn all segments on if character is not a space, digit, or uppercase letter
        LCDMEM[position] = 0xFF;
        LCDMEM[position+1] = 0xFF;
    }
}

/*
 * Scrolls input string across LCD screen from left to right
 */
void displayScrollText(char *msg)
{
    int length = strlen(msg);
    //int oldmode = mode;
    int i;
    int s = 5;
    char buffer[6] = "      ";
    for (i=0; i<length+7; i++)
    {
        //if (mode != oldmode)
            //break;
        int t;
        for (t=0; t<6; t++)
            buffer[t] = ' ';
        int j;
        for (j=0; j<length; j++)
        {
            if (((s+j) >= 0) && ((s+j) < 6))
                buffer[s+j] = msg[j];
        }
        s--;

        showChar(buffer[0], pos1);
        showChar(buffer[1], pos2);
        showChar(buffer[2], pos3);
        showChar(buffer[3], pos4);
        showChar(buffer[4], pos5);
        showChar(buffer[5], pos6);

        __delay_cycles(200000);
    }
}

void clearLCD()
{
    LCDMEM[pos1] = LCDBMEM[pos1] = 0;
    LCDMEM[pos1+1] = LCDBMEM[pos1+1] = 0;
    LCDMEM[pos2] = LCDBMEM[pos2] = 0;
    LCDMEM[pos2+1] = LCDBMEM[pos2+1] = 0;
    LCDMEM[pos3] = LCDBMEM[pos3] = 0;
    LCDMEM[pos3+1] = LCDBMEM[pos3+1] = 0;
    LCDMEM[pos4] = LCDBMEM[pos4] = 0;
    LCDMEM[pos4+1] = LCDBMEM[pos4+1] = 0;
    LCDMEM[pos5] = LCDBMEM[pos5] = 0;
    LCDMEM[pos5+1] = LCDBMEM[pos5+1] = 0;
    LCDMEM[pos6] = LCDBMEM[pos6] = 0;
    LCDMEM[pos6+1] = LCDBMEM[pos6+1] = 0;

    LCDM14 = LCDBM14 = 0x00;
    LCDM18 = LCDBM18 = 0x00;
    LCDM3 = LCDBM3 = 0x00;
}


// *****************************************
// **      FUNCTIONS  IMPLEMENTATION      **
// *****************************************

void Init_GPIO() {
    // Habilitacion de botones S1 y S2 (pull-up)
    // Para el boton de inicio | pausa
    P1DIR &= ~(BIT1|BIT2);
    P1REN |=  (BIT1|BIT2);
    P1OUT |=  (BIT1|BIT2);

    // Habiilitamos interrupciones
    P1IFG &= ~(BIT1|BIT2);
    __bis_SR_register(GIE);
    P1IE  |=  (BIT1|BIT2);
    P1IES |=  (BIT1|BIT2);

    // Activamos P1.0 y P9.7 como salida (LEDs rojo y verde)
    P1DIR |= BIT0;
    P9DIR |= BIT7;

    // Valores iniciales LEds apagados
    P1OUT &= ~(BIT0);
    P9OUT &= ~BIT7;
}

void Init_Timer(){
    // Configuración TIMER_A:
    // TimerA1, ACLK/1, modo up, reinicia TACLR
    TA0CTL = TASSEL_1 | TACLR | MC_1;

    // ACLK tiene una frecuencia de 32768 Hz
    // Carga cuenta en TA1CCR0 0.1seg TA1CCR=(0,1*32768)-1
    TA0CCR0 = 5000;
    TA0CCTL0 = CCIE; // Habilita interrupción (bit CCIE)
}


void Init_GameVariables(){
    currScore = 3;
    n1 = 0; aux1 = 0;
    n2 = 0; aux2 = 0;
    n3 = 0; aux3 = 0;
    start_stop = 0;
}

void increaseValues(){
    if(runningPlay && (start_stop % 2 == 0)){
        // Incrementamos las auxiliares
        ++aux1; ++aux2; ++aux3;
        // Incrementamos los valores a mostrar en ciclos de diferente duracion y resetamos auxiliares
        if(aux1 == 2){ ++n1; aux1 = 0;}
        if(aux2 == 3){ ++n2; aux2 = 0;}
        if(aux3 == 1){ ++n3; aux3 = 0;}
        // Reseteamos en caso de superar el valor de 9
        if(n1 == 10) n1 = 0;
        if(n2 == 10) n2 = 0;
        if(n3 == 10) n3 = 0;
    }
}

// *****************************************
// **      PLAY MODE IMPLEMENTATION       **
// *****************************************

void play(){
    runningPlay = 1;
    while(mode == PLAY_MODE){
        showChar('I', pos1);
        showChar(n1 + '0', pos2);
        showChar('I', pos3);
        showChar(n2 + '0', pos4);
        showChar('I', pos5);
        showChar(n3 + '0', pos6);
    }
    runningPlay = 0;
}

// *****************************************
// **   SCORE MODE IMPLEMENTATION         **
// *****************************************

void checkNum(){
    if (n1 == n2 && n1 == n3){
        if (n1 == 7){ currScore = 100; displayScrollText("JACKPOT"); }
        else { currScore += n1; displayScrollText("BIG PRIZE"); }
    }
    else if (n1 == n2 || n1 == n3 || n2 == n3) currScore +=1;
    else --currScore;

    if(currScore >= 100) {currScore = 100; mode = END_PLAY;}
    else if(currScore <= -100) {currScore = -100; mode = END_PLAY;}
}

void showLocalOrGlobalScore(int n){
    int unidades = n % 10; n /= 10;
    int decenas = n % 10; n /= 10;
    int centenas = n % 10 ;
    int i = 0;
    while(i< 5000){
       showChar(centenas + '0', pos4);
       showChar(decenas + '0', pos5);
       showChar(unidades + '0', pos6);
       i++;
    }
}

void showScore(){
    if(currScore >= 0){
        displayScrollText("CURRENT SCORE");
        showLocalOrGlobalScore(currScore);
    }
    else{
        displayScrollText("HAVE LOST CREDITS");
        showLocalOrGlobalScore(-currScore);
    }
    displayScrollText("MAX SCORE");
    showLocalOrGlobalScore(maxScore);
}

// *****************************************
// **    END MODE IMPLEMENTATION          **
// *****************************************

void gameOverScore(){
    displayScrollText("GAME OVER");
    if(currScore >= 0){
        displayScrollText("YOUR EARNS");
        showLocalOrGlobalScore(currScore);
    }
    else {
        displayScrollText("MONEY LOST");
        showLocalOrGlobalScore(-currScore);
    }
    if (currScore > maxScore){
        displayScrollText("NEW RECORD");
        maxScore = currScore;
    }
    displayScrollText("BACK TO MAIN MENU");
}


