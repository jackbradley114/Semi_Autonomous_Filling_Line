#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#pragma config FEXTOSC = OFF
#pragma config RSTOSC  = HFINT32
#pragma config CLKOUTEN= OFF
#pragma config WDTE    = OFF
#pragma config PWRTE   = ON
#pragma config MCLRE   = ON
#pragma config CP      = OFF
#pragma config BOREN   = ON
#pragma config LPBOREN = OFF
#pragma config STVREN  = ON
#pragma config PPS1WAY = OFF
#pragma config WRT     = OFF
#pragma config LVP     = OFF

#define _XTAL_FREQ 32000000UL

//adjust definitions for different target mass and min/max random times
#define RECORD_TARGET_MASS_G  250U
#define WRITE_TIME_MIN_MS     5000U
#define WRITE_TIME_MAX_MS     35000U

// I2C timing definitions for 100kbps speed and timeout time
#define I2C_BAUD_HZ        100000UL
#define I2C_TIMEOUT_COUNT  30000U

//PIN definitions for I/O, pull up enable, and tristate settings
#define WRITE_TRIG_PORT  PORTCbits.RC5
#define WRITE_TRIG_TRIS  TRISCbits.TRISC5
#define WRITE_TRIG_WPU   WPUCbits.WPUC5

#define RECORD_TRIG_PORT PORTCbits.RC4
#define RECORD_TRIG_TRIS TRISCbits.TRISC4
#define RECORD_TRIG_WPU  WPUCbits.WPUC4

#define ACK_LAT LATCbits.LATC3
#define ACK_TRIS TRISCbits.TRISC3

#define PULSE_LAT  LATCbits.LATC2
#define PULSE_TRIS TRISCbits.TRISC2

//definitions for address for I2C functions

#define ADS_ADDR        0x48
#define ADS_REG_CONV    0x00
#define ADS_REG_CONFIG  0x01
#define US_I2C_ADDR     0x57

//timer1 values for 1ms tick generation
#define TMR1_RELOAD_H   0xFC
#define TMR1_RELOAD_L   0x18

//peripheral pin select lock/unlock functions
static void pps_unlock(void){
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 0;
}
static void pps_lock(void){
    PPSLOCK = 0x55;
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 1;
}

//TIMER1 module setup, prescaling and clock source setup
static void timer1_init(void)
{
    T1CON = 0x00;
    T1GCON = 0x00;

    T1CONbits.TMR1CS = 0b00;
    T1CONbits.T1CKPS = 0b11;
    T1CONbits.T1SYNC = 1;

    TMR1H = TMR1_RELOAD_H;
    TMR1L = TMR1_RELOAD_L;

    PIR1bits.TMR1IF = 0;
    T1CONbits.TMR1ON = 1;
}
//1ms timer function using TMR1
static void timer1_wait_1ms(void)
{
    TMR1H = TMR1_RELOAD_H;
    TMR1L = TMR1_RELOAD_L;
    PIR1bits.TMR1IF = 0;

    while(!PIR1bits.TMR1IF)
    {
        ;
    }
}

static void delay_ms(uint16_t ms)
{
    while(ms--)
    {
        timer1_wait_1ms();
    }
}

// I2C functions

//I2C fail
static bool i2c_fail(void)
{
    SSP1CON1bits.SSPEN = 0;
    SSP1CON2 = 0x00;
    SSP1CON1bits.WCOL = 0;
    PIR1bits.SSP1IF = 0;
    SSP1CON1bits.SSPEN = 1;
    return false;
}

//I2C timeout function (stops lockup when testing without US sensor)
static bool i2c_wait_idle(void)
{
    uint16_t t = I2C_TIMEOUT_COUNT;

    while((SSP1CON2 & 0x1F) || SSP1STATbits.R_nW)
    {
        if(--t == 0)
        {
            return i2c_fail();
        }
    }

    return true;
}


//Hardware I2C setup routine
static void i2c_init_hw(void)
{
    TRISCbits.TRISC0 = 1;
    TRISCbits.TRISC1 = 1;

    ANSELCbits.ANSC0 = 0;
    ANSELCbits.ANSC1 = 0;

    pps_unlock();
    SSP1CLKPPS = 0x10;   // RC0
    SSP1DATPPS = 0x11;   // RC1
    pps_lock();

    SSP1CON1 = 0x00;
    SSP1CON2 = 0x00;
    SSP1CON3 = 0x00;
    SSP1STAT = 0x80;

    SSP1ADD = (uint8_t)((_XTAL_FREQ / (4UL * I2C_BAUD_HZ)) - 1UL);

    PIR1bits.SSP1IF = 0;
    SSP1CON1bits.SSPM = 0b1000;
    SSP1CON1bits.SSPEN = 1;
}

//I2C start bit function
static bool i2c_start(void)
{
    uint16_t t = I2C_TIMEOUT_COUNT;

    if(!i2c_wait_idle()) return false;

    SSP1CON2bits.SEN = 1;
    while(SSP1CON2bits.SEN)
    {
        if(--t == 0) return i2c_fail();
    }

    return true;
}

static bool i2c_restart(void)
{
    uint16_t t = I2C_TIMEOUT_COUNT;

    if(!i2c_wait_idle()) return false;

    SSP1CON2bits.RSEN = 1;
    while(SSP1CON2bits.RSEN)
    {
        if(--t == 0) return i2c_fail();
    }

    return true;
}

static void i2c_stop(void)
{
    uint16_t t = I2C_TIMEOUT_COUNT;

    if(!i2c_wait_idle()) return;

    SSP1CON2bits.PEN = 1;
    while(SSP1CON2bits.PEN)
    {
        if(--t == 0)
        {
            (void)i2c_fail();
            return;
        }
    }
}

static bool i2c_write_byte(uint8_t b)
{
    uint16_t t = I2C_TIMEOUT_COUNT;

    if(!i2c_wait_idle()) return false;

    SSP1CON1bits.WCOL = 0;
    PIR1bits.SSP1IF = 0;
    SSP1BUF = b;

    while(!PIR1bits.SSP1IF)
    {
        if(--t == 0) return i2c_fail();
    }

    if(SSP1CON1bits.WCOL) return i2c_fail();

    return (SSP1CON2bits.ACKSTAT == 0);
}

static bool i2c_read_byte(uint8_t *b, bool ack)
{
    uint16_t t;

    if(!i2c_wait_idle()) return false;

    SSP1CON2bits.RCEN = 1;
    t = I2C_TIMEOUT_COUNT;
    while(!SSP1STATbits.BF)
    {
        if(--t == 0) return i2c_fail();
    }

    *b = SSP1BUF;

    if(!i2c_wait_idle()) return false;

    SSP1CON2bits.ACKDT = ack ? 0 : 1;
    SSP1CON2bits.ACKEN = 1;

    t = I2C_TIMEOUT_COUNT;
    while(SSP1CON2bits.ACKEN)
    {
        if(--t == 0) return i2c_fail();
    }

    return true;
}

// ADS1115 config register setup, channel muxing
static uint16_t ads_build_config(uint8_t ch)
{
    uint16_t mux = 0x4 + (ch & 0x03);
    return (1u << 15) | (mux << 12) | (1u << 9) | (1u << 8) | (0x7u << 5) | (0x3u);
}
//I2C write routine for ADC with step out for failure
static bool ads_write_reg(uint8_t reg, uint16_t val)
{
    if(!i2c_start()) return false;
    if(!i2c_write_byte((ADS_ADDR << 1) | 0)) goto fail;
    if(!i2c_write_byte(reg)) goto fail;
    if(!i2c_write_byte((uint8_t)(val >> 8))) goto fail;
    if(!i2c_write_byte((uint8_t)val)) goto fail;
    i2c_stop();
    return true;

fail:
    i2c_stop();
    return false;
}

//I2C read routine for ADC with failproofing
static bool ads_read_reg(uint8_t reg, uint16_t *out)
{
    uint8_t msb, lsb;

    if(!i2c_start()) return false;
    if(!i2c_write_byte((ADS_ADDR << 1) | 0)) goto fail;
    if(!i2c_write_byte(reg)) goto fail;

    if(!i2c_restart()) goto fail;
    if(!i2c_write_byte((ADS_ADDR << 1) | 1)) goto fail;

    if(!i2c_read_byte(&msb, true)) goto fail;
    if(!i2c_read_byte(&lsb, false)) goto fail;

    i2c_stop();
    *out = ((uint16_t)msb << 8) | lsb;
    return true;

fail:
    i2c_stop();
    return false;
}

static uint16_t ads_read_channel(uint8_t ch)
{
    uint16_t raw = 0;

    if(!ads_write_reg(ADS_REG_CONFIG, ads_build_config(ch)))
    {
        return 0;
    }

    delay_ms(2);
    (void)ads_read_reg(ADS_REG_CONV, &raw);
    return raw;
}

// ULTRASONIC SENSOR I2C
static uint16_t us_read_mm(void)
{
    uint8_t b0, b1, b2;
    uint32_t raw;

    if(!i2c_start()) return 0;
    if(!i2c_write_byte((US_I2C_ADDR << 1) | 0)) goto fail;
    if(!i2c_write_byte(0x01)) goto fail;
    i2c_stop();

    delay_ms(120);

    if(!i2c_start()) return 0;
    if(!i2c_write_byte((US_I2C_ADDR << 1) | 1)) goto fail;

    if(!i2c_read_byte(&b0, true)) goto fail;
    if(!i2c_read_byte(&b1, true)) goto fail;
    if(!i2c_read_byte(&b2, false)) goto fail;

    i2c_stop();

    raw = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | b2;
    raw /= 1000UL;

    return (uint16_t)raw;

fail:
    i2c_stop();
    return 0;
}

//UART module initialisation (9600 baud rate, pin mapping and HFFRQ selection)
static void uart_init_9600(void)
{
    OSCFRQbits.HFFRQ = 0b0110;

    pps_unlock();
    RA5PPS = 0x14;
    RXPPS  = 0x04;
    pps_lock();

    TX1STAbits.BRGH = 1;
    BAUD1CONbits.BRG16 = 1;
    SP1BRGH = (832 >> 8) & 0xFF;
    SP1BRGL = 832 & 0xFF;

    RC1STAbits.SPEN = 1;
    TX1STAbits.TXEN = 1;
    RC1STAbits.CREN = 1;

    TRISAbits.TRISA5 = 0;
    TRISAbits.TRISA4 = 1;
}

static void uart_putc(char c)
{
    while(!PIR1bits.TXIF)
    {
        ;
    }
    TXREG = c;
}

static void uart_print_str(const char *s)
{
    while(*s)
    {
        uart_putc(*s++);
    }
}

static void uart_print_u16(uint16_t v)
{
    char buf[6];
    uint8_t i = 0;

    if(v == 0)
    {
        uart_putc('0');
        return;
    }

    while(v > 0 && i < 5)
    {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }

    while(i)
    {
        uart_putc(buf[--i]);
    }
}

static void uart_print_2digits(uint8_t v)
{
    uart_putc((char)('0' + (v / 10)));
    uart_putc((char)('0' + (v % 10)));
}

static void uart_print_3digits(uint16_t v)
{
    uart_putc((char)('0' + (v / 100)));
    uart_putc((char)('0' + ((v / 10) % 10)));
    uart_putc((char)('0' + (v % 10)));
}

//unit conversion and print helper functions

static uint16_t temp_k_x100_from_adc(uint16_t code)
{
    uint32_t c_x1000 = ((uint32_t)code * 106500UL) >> 15;
    return (uint16_t)(((c_x1000 + 5UL) / 10UL) + 27315UL);
}

static void print_mass_g_from_counts(uint16_t counts)
{
    uint32_t g_x100 = ((uint32_t)counts * 400000UL) >> 15;

    uart_print_u16((uint16_t)(g_x100 / 100UL));
    uart_putc('.');
    uart_print_2digits((uint8_t)(g_x100 % 100UL));
    uart_putc('g');
}

static void print_temp_K_x100(uint16_t k_x100)
{
    uart_print_u16((uint16_t)(k_x100 / 100U));
    uart_putc('.');
    uart_print_2digits((uint8_t)(k_x100 % 100U));
    uart_putc('K');
}

static void print_height_m(uint16_t mm)
{
    uart_print_u16((uint16_t)(mm / 1000U));
    uart_putc('.');
    uart_print_3digits((uint16_t)(mm % 1000U));
    uart_putc('0');
    uart_putc('m');
}

static void print_time_ms(uint16_t t)
{
    uart_print_u16(t);
    uart_putc('.');
    uart_putc('0');
    uart_putc('0');
    uart_putc('m');
    uart_putc('s');
}

//Random time generation
static uint16_t lfsr = 0xACE1u;

static uint16_t random_time(uint16_t min, uint16_t max)
{
    lfsr ^= (uint16_t)(lfsr << 7);
    lfsr ^= (uint16_t)(lfsr >> 9);
    lfsr ^= (uint16_t)(lfsr << 8);

    return (uint16_t)(min + (lfsr % (uint16_t)(max - min + 1)));
}

//GPIO set-up (analogue disable, tristate, initial output state, and internal pull up enabling)

static void gpio_init(void)
{
    ANSELA = 0;
    ANSELC = 0;
    LATC = 0;

    WRITE_TRIG_TRIS  = 1;
    WRITE_TRIG_WPU   = 1;

    RECORD_TRIG_TRIS = 1;
    RECORD_TRIG_WPU  = 1;

    ACK_TRIS = 0;
    ACK_LAT = 0;
    
    PULSE_TRIS = 0;
    PULSE_LAT  = 0;
}

//main
void main(void)
{
    uint8_t mode;
    uint16_t temp_code;
    uint16_t temp_k_x100;
    uint16_t temp_k_int;
    uint16_t weight_before;
    uint16_t weight_after;
    uint16_t height_mm;
    uint16_t time_ms;
    uint32_t sum_ms;
    int32_t net_counts;

    gpio_init();
    i2c_init_hw();
    uart_init_9600();
    timer1_init();

    for(;;)
    {
        mode = 0;
        temp_code = 0;
        temp_k_x100 = 0;
        weight_before = 0;
        weight_after = 0;
        height_mm = 0;

        if(RECORD_TRIG_PORT == 0)
        {
            mode = 2;
        }
        else if(WRITE_TRIG_PORT == 0)
        {
            mode = 1;
        }

        if(mode != 0)
        {
            temp_code     = ads_read_channel(0);
            temp_k_x100   = temp_k_x100_from_adc(temp_code);
            weight_before = ads_read_channel(1);
            height_mm     = us_read_mm();

            if(mode == 2)
            {
                temp_k_int = (uint16_t)((temp_k_x100 + 50U) / 100U);

                sum_ms = (uint32_t)RECORD_TARGET_MASS_G
                       + (uint32_t)height_mm
                       + (uint32_t)temp_k_int;

                time_ms = (sum_ms > 65535UL) ? 65535U : (uint16_t)sum_ms;
            }
            else
            {
                time_ms = random_time(WRITE_TIME_MIN_MS, WRITE_TIME_MAX_MS);
            }

            PULSE_LAT = 1;
            delay_ms(time_ms);
            PULSE_LAT = 0;

            delay_ms(1000);

            weight_after = ads_read_channel(1);

            net_counts = (int32_t)weight_after - (int32_t)weight_before;
            if(net_counts < 0)
            {
                net_counts = 0;
            }

            if(mode == 2)
            {
                uart_print_str("record, ");
            }
            else
            {
                uart_print_str("write, ");
            }

            print_mass_g_from_counts((uint16_t)net_counts);
            uart_print_str(", ");
            print_temp_K_x100(temp_k_x100);
            uart_print_str(", ");
            print_height_m(height_mm);
            uart_print_str(", ");
            print_time_ms(time_ms);
            uart_putc('\r');
            uart_putc('\n');
            ACK_LAT=1;
            delay_ms(200);
            ACK_LAT=0;
        }
    }
}
