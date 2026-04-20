//Test Program for Data-Acquistion Board to validate half-duplex UART comms

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#define _XTAL_FREQ 32000000UL

// device config bits
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

//PPS lock/unlock functinos
static void pps_unlock(void){
    PPSLOCK = 0x55; PPSLOCK = 0xAA; PPSLOCKbits.PPSLOCKED = 0;
}
static void pps_lock(void){
    PPSLOCK = 0x55; PPSLOCK = 0xAA; PPSLOCKbits.PPSLOCKED = 1;
}

// ================= PINS =================
#define SDA_PORT   PORTCbits.RC1
#define SDA_LAT    LATCbits.LATC1
#define SDA_TRIS   TRISCbits.TRISC1

#define SCL_PORT   PORTCbits.RC0
#define SCL_LAT    LATCbits.LATC0
#define SCL_TRIS   TRISCbits.TRISC0

//software I2C helpers
static inline void i2c_delay(void){ __delay_us(5); }
static inline void SDA_release(void){ SDA_TRIS = 1; }
static inline void SDA_low(void){ SDA_LAT = 0; SDA_TRIS = 0; }
static inline void SCL_release(void){ SCL_TRIS = 1; }
static inline void SCL_low(void){ SCL_LAT = 0; SCL_TRIS = 0; }

static void i2c_init_sw(void){ SDA_release(); SCL_release(); i2c_delay(); }

static void i2c_start(void){
    SDA_release();
    SCL_release();
    i2c_delay();
    SDA_low();
    i2c_delay();
    SCL_low();
    i2c_delay();
}
static void i2c_stop(void){
    SDA_low();
    i2c_delay();
    SCL_release();
    i2c_delay();
    SDA_release();
    i2c_delay();
}
//bit-bang write byte I2C
static bool i2c_write_byte(uint8_t b)
{
    for(uint8_t i=0;i<8;i++){
        if(b & 0x80) SDA_release(); else SDA_low();
        i2c_delay(); SCL_release(); i2c_delay(); SCL_low(); i2c_delay();
        b <<= 1;
    }
    SDA_release(); i2c_delay();
    SCL_release(); i2c_delay();
    bool ack = (SDA_PORT == 0);
    SCL_low(); i2c_delay();
    return ack;
}
//bit-bang read byte function I2C
static uint8_t i2c_read_byte(bool ack)
{
    uint8_t b=0; SDA_release();
    for(uint8_t i=0;i<8;i++){
        b <<= 1;
        SCL_release();
        i2c_delay();
        if(SDA_PORT) b |= 1;
        SCL_low();
        i2c_delay();
    }
    if(ack) SDA_low();
    else SDA_release();
    i2c_delay();
    SCL_release();
    i2c_delay();
    SCL_low();
    i2c_delay();
    SDA_release();
    return b;
}

//ADS1115 set-up registers
#define ADS_ADDR        0x48
#define ADS_REG_CONV    0x00
#define ADS_REG_CONFIG  0x01

static uint16_t ads_build_config(uint8_t ch)
{
    uint16_t mux = 0x4 + (ch & 0x03);
    return (1u<<15) | (mux<<12) | (1u<<9) | (1u<<8) | (0x7u<<5) | (0x3u);
}
//ADC write register routine
static bool ads_write_reg(uint8_t reg, uint16_t val)
{
    i2c_start();
    if(!i2c_write_byte((ADS_ADDR<<1)|0)) { i2c_stop(); return false; }
    if(!i2c_write_byte(reg)) { i2c_stop(); return false; }
    if(!i2c_write_byte((uint8_t)(val>>8))) { i2c_stop(); return false; }
    if(!i2c_write_byte((uint8_t)val)) { i2c_stop(); return false; }
    i2c_stop();
    return true;
}
//ADC I2C read transaction
static bool ads_read_reg(uint8_t reg, uint16_t *out)
{
    i2c_start();
    if(!i2c_write_byte((ADS_ADDR<<1)|0)) { i2c_stop(); return false; }
    if(!i2c_write_byte(reg)) { i2c_stop(); return false; }

    i2c_start();
    if(!i2c_write_byte((ADS_ADDR<<1)|1)) { i2c_stop(); return false; }

    uint8_t msb = i2c_read_byte(true);
    uint8_t lsb = i2c_read_byte(false);
    i2c_stop();
    *out = ((uint16_t)msb<<8) | lsb;
    return true;
}
//ADC read function I2C
static uint16_t ads_read_channel_u16(uint8_t ch)
{
    (void)ads_write_reg(ADS_REG_CONFIG, ads_build_config(ch));
    __delay_ms(2);
    uint16_t raw=0;
    (void)ads_read_reg(ADS_REG_CONV, &raw);
    return raw;
}

//UART setup (32MHz intosc, pin mapping, baud rate setup for 9600
static void uart_init_9600(void)
{
    OSCFRQbits.HFFRQ = 0b0110; // 32MHz
    pps_unlock();
    RA5PPS = 0x14; // TX1->RA5
    RXPPS  = 0x04; // RA4->RX1
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
//UART print functions

static void uart_putc(char c){ while(!PIR1bits.TXIF); TXREG = c; }
static void uart_print_u16(uint16_t v){ char buf[6]; uint8_t i=0; if(v==0){ uart_putc('0'); return; } while(v>0 && i<5){ buf[i++]='0'+(v%10); v/=10; } while(i) uart_putc(buf[--i]); }
static void uart_print_u8(uint8_t v){ if (v >= 100) { uart_putc((char)('0' + (v / 100))); v %= 100; } if (v >= 10)  { uart_putc((char)('0' + (v / 10)));  v %= 10;  } uart_putc((char)('0' + v)); }
static void uart_print_3digits(uint16_t v){ uart_putc((char)('0' + (uint8_t)(v / 100u))); uart_putc((char)('0' + (uint8_t)((v / 10u) % 10u))); uart_putc((char)('0' + (uint8_t)(v % 10u))); }
static void print_temp_from_adc(uint16_t code)
{
    uint32_t temp_mC = ((uint32_t)code * 106500u) >> 15; // scale temp 0 to 106.5oC for 0 to 4.096V
    uint8_t whole = (uint8_t)(temp_mC / 1000);//convert and format for oC 3dp
    uint16_t frac = (uint16_t)(temp_mC % 1000);//

//print temperature as x.xxxoC
    uart_print_u8(whole);
    uart_putc('.');
    uart_print_3digits(frac);
    uart_putc('o');
    uart_putc('C');
}

//UART recieve set-up
#define CMD_BUF_SIZE 8
char cmd_buf[CMD_BUF_SIZE];
uint8_t cmd_idx = 0;

static void uart_process(void)
{
    while(PIR1bits.RCIF)
    {
        char c = RCREG;
        if(c == '\r' || c == '\n')
        {
            cmd_buf[cmd_idx] = 0; // null-terminate
            if(cmd_idx > 0)
            {
               
                if(cmd_buf[0]=='t' && cmd_buf[1]=='e' && cmd_buf[2]=='m' && cmd_buf[3]=='p')
                {
                    uint16_t adc = ads_read_channel_u16(0);
                    print_temp_from_adc(adc);
                    uart_putc('\r'); uart_putc('\n');
                }
            }
            cmd_idx = 0; // reset
        }
        else if(cmd_idx < CMD_BUF_SIZE-1)
        {
            cmd_buf[cmd_idx++] = c;
        }
    }
}

//GPIO SETUP
static void gpio_init(void){ ANSELA = 0; ANSELC = 0; LATC=0; SDA_TRIS=SCL_TRIS=1; }

// MAIN Loop
void main(void)
{
    gpio_init();
    i2c_init_sw();
    uart_init_9600();

    for(;;)
    {
        uart_process();
    }
}

