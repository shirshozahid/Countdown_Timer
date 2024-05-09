#include <avr/io.h>
#include <avr/interrupt.h>
#include <xc.h>

enum STATE {Initial, Set, Count, Pause, Done, Minute, Seconds};
volatile uint32_t count_ms = 0;

uint16_t ADC_value;
ISR(ADC_vect) {
    ADC_value = ADC;  
}

ISR(TIMER1_COMPA_vect) {
    count_ms += 1;
}

void delay_ms(long num) {
    while (num--) {
        for (volatile long x = 0; x < 468; x++) {
            ;
        }
    }
}

// set up data direction registers for the output pins
void setup() {
    DDRC  &= 0b11110001;
    PORTC |= 0b00001110; 
    DDRD |= 0b10010000;
    PORTD &= 0b00000000;
    DDRB |= 0b00111101;
    PORTB |= 0b00111100; //turn off LEDs
    sei();
}

void setuptimer() {
    TCCR1A = 0b00000000;
    TCCR1B = 0b00001011;
    OCR1A = 249;
    TCCR1C = 0;
    TCNT1 = 0;
    OCR1B = 0;
    ICR1 = 0;
    TIFR1 = 0;
    TIMSK1 = 0b00000010;    
}  

void setupAtoD() {    
    ADMUX = 0b01100000; //AVcc with external cap at AREF, 1 for left adjust, 0000 for ADC0
    ADCSRA = 0b11111110; //enabled, start, auto, flag 1, interrupt 0, 64 prescaler
    ADCSRB = 0b00000000; //  free running
    DIDR0 |= 0b00000001;//Pin0 disabled
    sei();
} 

void sendData(uint8_t segments, uint8_t digits) {
    uint8_t mask = 0b10000000;
    PORTD &= 0b01101111;
    for(int i =0; i <= 7; i++) {
        PORTD &= 0b01111111;
        if((mask & segments) != 0) {
            PORTB |= 0b00000001;
        } else {
            PORTB &= 0b11111110;
        }
        mask = mask>>1;
        PORTD |= 0b10000000;
    }
    mask = 0b10000000;
    for (int i = 0; i <= 7; i++) {
        PORTD &= 0b01111111;
        if((mask & digits) != 0) {
            PORTB |= 0b00000001;
        } else {
            PORTB &= 0b1111110;
        }
        mask = mask>>1;
        PORTD |= 0b10000000; 
    }
    PORTD |= 0b00010000;
}

/* Segment byte maps for numbers 0 to 9 */
const uint8_t SEGMENT_MAP[] = {
    0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0X80,0X90,
/* Continuing on for A (10) to F (15) */
    0x88,0x83,0xC6,0xA1,0x86,0x8E,
/* Then blank (16), dash (17), 0. (18), 1.(19), 2.(20), O (21), n (22), . (23) */
    0xFF, 0xBF, 0x40, 0x79, 0x24, 0xA3, 0xAB, 0x7F};

uint8_t segmentMap(uint8_t value) {
    return SEGMENT_MAP[value];
}

const uint8_t SEGMENT_MAP_DECIMAL[] = {
    0xC0 & 0b01111111, 0xF9 & 0b01111111, 0xA4 & 0b01111111, 
    0xB0 & 0b01111111, 0x99 & 0b01111111, 0x92 & 0b01111111, 
    0x82 & 0b01111111, 0xF8 & 0b01111111, 0X80 & 0b01111111, 0X90 & 0b01111111};

uint8_t segmentMapDecimal(uint8_t value) {
    return SEGMENT_MAP_DECIMAL[value];
}

char digits[4];
void showDigitsDecimal(uint16_t num) {
    uint16_t newNum = num>>6;
    for(uint16_t i = 0; i < 1024; i+=10) {
        if(i<newNum) {
            for (int j = 0; j < 4; j++) {
                if(j ==  1 && j < 4) {
                sendData((segmentMap(digits[j]) & 0b01111111), (1 << j)); 
                } else {
                    sendData(segmentMap(digits[j]), (1 << j));
                }
            }
            sendData(segmentMap(16), 0); // blank the display
        } else {
            sendData(segmentMap(16), 0);
        }
    }
}

void showDigits(uint16_t num) {
    uint16_t newNum = num>>6;
    for(uint16_t i = 0; i < 1024; i+=10) {
        if(i<newNum) {
            for (int i = 0; i < 4; i++) {
                sendData(segmentMap(digits[i]), (1 << i)); 
            }
            sendData(segmentMap(16), 0); // blank the display
        } else {
            sendData(segmentMap(16), 0);
        }
    }
}


void setNum(uint16_t num) {
    uint16_t newNum = num >>6;
}

int main(void) {
    setup(); // set up the physical hardware 
    setuptimer();
    setupAtoD();
    enum STATE state = Initial;
    int is_Counting = 0;
    uint32_t second_1 = 0;
    uint32_t second_10 = 0;
    uint32_t pause_count = 0;
    uint32_t minute = 0;
    uint32_t button_time = 0;
    uint32_t button_valueA1 = 0b00000010;
    uint32_t button_stateA1 = 0b00000010;
    uint32_t button_valueA2 = 0b00000100;
    uint32_t button_stateA2 = 0b00000100;
    uint32_t button_valueA3 = 0b00001000;
    uint32_t button_stateA3 = 0b00001000;
    //uint32_t now = 0;
    uint32_t last_ms = 0;
    uint8_t button = 0;
    uint8_t button_2 = 0;
    uint8_t button_3 = 0;
    uint8_t saved_onesminute;
    uint8_t saved_tensminute;
    uint8_t saved_onesecond;
    uint8_t saved_tensecond;
    int decimal;
    int minutes;
    while (1) {
        if(count_ms > button_time) {
            button_valueA1 = (PINC & 0b00000010);
            if((button_valueA1 ^ button_stateA1) != 0) {
                button_stateA1 = button_valueA1;
                button_time = count_ms + 30;
                button = (button_valueA1==0)?1:0;
            }
            button_valueA3 = (PINC & 0b00001000);
            if((button_valueA3 ^ button_stateA3) != 0) {
                button_stateA3 = button_valueA3;
                button_time = count_ms + 30;
                button_2 = (button_valueA3==0)?1:0; 
            }
            button_valueA2 = (PINC & 0b00000100);
            if((button_valueA2 ^ button_stateA2) != 0) {
                button_stateA2 = button_valueA2;
                button_time = count_ms + 30;
                button_3 = (button_valueA2==0)?1:0;
            }
        }
        //now = count_ms;
        switch (state) {
            case Initial:
                second_1 = 0;
                second_10 = 0;
                minute = 0;
                PORTB |= 0b00111100;
                digits[0] = 0;
                digits[1] = 18;
                digits[2] = 0;
                digits[3] = 0;
                setNum(ADC_value);
                showDigits(ADC_value);
                digits[1] = 0;
                if((count_ms>button_time) && (button == 1)) {
                    digits[0] = 0;
                    digits[1] = 0;
                    digits[2] = 16;
                    digits[3] = 16;
                    state = Minute;
                    button = 0;
                }
                break;
            case Minute:
                PORTB &= 0b11010111;
                showDigits(ADC_value);                
                if((count_ms>button_time) && (button_3 == 1)) {
                    digits[1] += 1;
                    if(digits[1] > 9) {
                        digits[1] = 0;
                        digits[0] += 1;
                        if(digits[0] > 9) {
                            digits[1] = 0;
                            digits[0] = 0;
                        }
                    }
                    button_3 = 0;
                } 
                if ((count_ms > button_time) && (button_2 == 1)) {
                    if (digits[1] > 0) {
                        digits[1] -= 1;
                    } else if (digits[0] > 0) {
                        digits[1] = 9; // Set digits[1] to 9 for proper borrow
                        digits[0] -= 1;
                    }
                    button_2 = 0;
                }
                if((count_ms>button_time) && (button == 1)) {
                    saved_onesminute = digits[1];
                    saved_tensminute = digits[0];
                    digits[0] = 16;
                    digits[1] = 16;
                    digits[2] = 0;
                    digits[3] = 0;
                    state = Seconds;
                    button = 0;                    
                }
                break;
            case Seconds:
                PORTB |= 0b00011100;
                PORTB &= 0b11101111;
                showDigits(ADC_value);                
                if((count_ms>button_time) && (button_3 == 1)) {
                    digits[3] += 1;
                    if (digits[3] > 9) {
                        digits[2] += 1;
                        digits[3] = 0;
                        if(digits[2] > 5) {
                            digits[2] = 0;
                        }
                    }
                    button_3 = 0;
                }
                if ((count_ms > button_time) && (button_2 == 1)) {
                    if (digits[3] > 0) {
                        digits[3] -= 1;
                    } else if (digits[2] > 0) {
                        digits[3] = 9; // Set digits[1] to 9 for proper borrow
                        digits[2] -= 1;
                    }
                    button_2 = 0;
                } 
                if((count_ms>button_time) && (button == 1)) {
                    saved_onesecond = digits[3];
                    saved_tensecond = digits[2];                    
                    digits[0] = 16;
                    digits[1] = 16;
                    digits[2] = 0;
                    digits[3] = 0;
                    state = Set;
                    button = 0;                    
                }
                break;
            case Set:
                PORTB &= 0b11000111;
                digits[0] = saved_tensminute;               
                digits[2] = saved_tensecond;
                digits[3] = saved_onesecond;
                showDigits(ADC_value);
                digits[1] = saved_onesminute;
                showDigitsDecimal(ADC_value);
                if((count_ms>button_time) && (button==1)) {
                    state = Initial;
                    button = 0;
                }
                if((count_ms>button_time) && (button_2==1)) {                    
                    count_ms = 0;
                    button_time = count_ms + 30;
                    second_1 += count_ms;
                    second_10 += count_ms;
                    minute += count_ms;
                    state = Count;
                    button_2 = 0;
                }
                break;
            case Count:
                showDigits(ADC_value);
                showDigitsDecimal(ADC_value);
                PORTB |= 0b00011100;
                if(count_ms - second_1 >= 500) {
                PORTB &= 0b11111011;                     
                } else {
                    PORTB |= 0b00000100;
                }
                if (count_ms - second_1 >= 1000) {
                    showDigits(ADC_value);
                    showDigitsDecimal(ADC_value);
                    int calculated_seconds = ((saved_onesminute+(10*saved_tensminute))*60)+ (saved_onesecond + (10*saved_tensecond)); // Convert seconds to total seconds

                    int total_seconds = calculated_seconds; // Keep a copy of the initial value for calculations
                    int remaining_seconds = total_seconds - (count_ms / 1000); // Get remaining seconds

                    // Ensure remaining seconds are non-negative
                    remaining_seconds = (remaining_seconds < 0) ? 0 : remaining_seconds;

                    // Calculate seconds from remaining seconds
                    int seconds = remaining_seconds % 60;
                    minutes = remaining_seconds / 60;
                    digits[2] = seconds / 10; // Tens digit for seconds
                    digits[3] = seconds % 10; // Ones digit for seconds
                    digits[0] = minutes / 10; // Tens digit for minutes
                    //digits[1] = minutes % 10; // Ones digit for minutes 
                    // Update total_seconds for the next iteration (optional)
                    total_seconds = remaining_seconds;
                    showDigits(ADC_value);
                    digits[1] = minutes % 10;
                    showDigitsDecimal(ADC_value);
                    second_1 = count_ms;  
                }                
                if((count_ms>button_time) && (button==1)) {
                    button_time = count_ms;
                    state = Initial;
                    button = 0;
                } else if ((count_ms>button_time) && (button_2==1)) {
                    pause_count = count_ms;
                    is_Counting = 1;                    
                    state = Pause;
                    button_2 = 0;
                }
                if((digits[0]==0) && (digits[1]==0) && (digits[2]==0) && (digits[3]==0)) {
                    digits[0] = 0;
                    digits[1] = 18;
                    digits[2] = 0;
                    digits[3] = 0;
                    setNum(ADC_value);
                    showDigits(ADC_value);
                    state = Done;
                }
                break;
            case Pause:
                PORTB &= 0b11000111; 
                PORTB |= 0b00000100;
                if (is_Counting == 1) {                   
                    showDigits(ADC_value);
                    showDigitsDecimal(ADC_value);
                }
                if((count_ms>button_time) && (button==1)) {
                    button_time = count_ms;
                    state = Initial;
                    button = 0;
                }
                if((count_ms>button_time) && (button_2==1)) {
                    is_Counting = 0;
                    count_ms = pause_count;
                    button_time = pause_count+10;
                    second_1 += count_ms;
                    //now = pause_count;
                    state = Count;
                    button_2 = 0;
                    setNum(ADC_value);
                    showDigits(ADC_value);
                    showDigitsDecimal(ADC_value);
                }
                break;
            case Done:
                PORTB |= 0b00100000;
                digits[0] = 13;
                digits[1] = 21;
                digits[2] = 22;
                digits[3] = 14;
                setNum(ADC_value);
                showDigits(ADC_value);
                if((count_ms>button_time) && (button==1)) {
                    state = Initial;
                    button = 0;
                }
                break;
        }

    }
}
