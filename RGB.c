#define F_CPU 1000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <util/delay.h>

#define add1 0b00101000
#define add2 0b00101001

#define MODE1 0
#define MODE2 0
#define SUBADR1 2
#define SUBADR2 3
#define SUBADR3 4
#define ALLCALLADR 5
#define LED0_ON_L 6
#define LED0_ON_H 7
#define LED0_OFF_L 8
#define LED0_OFF_H 9

void timer_init(void);
void timer_start(void);
void timer_stop(void);
void update_LED(uint8_t LED, uint8_t colour, double brightness);
void disp_LED(long period);
void LED_row_init(void);
void LED_clear(void);
uint16_t rand_number(void);
void LEDdrive_on(void);
void LEDdrive_off(void);
void LEDdrive_init(void);
int LED_write_reg(uint8_t address, uint8_t reg, uint8_t data);
void TWI_init(void);
void TWI_stop(void);
void TWI_start(void);
void TWI_write(uint8_t u8data);
uint8_t TWI_readACK(void);
uint8_t TWI_readNACK(void);
uint8_t TWI_getStatus(void);

uint16_t RGB_arr[3][9][9];
double bright_scale = 1;
unsigned long time = 0;

int main(void)
{
	OSCCAL = 0xF2;
	
	LEDdrive_on();
	LEDdrive_init();
	TWI_init();
	LED_row_init();
	timer_init();

	double rand_num;
	int i,j;
	while(1)
	{
		for(j=0;j<3;j++)	
		{
			for(i=0;i<81;i++)
			{
				rand_num = (double)((uint16_t)rand()) / 65536;
				update_LED(i,j,rand_num);
			}
		}
		disp_LED(5000);
		LED_clear();
	}	
}

//interrupt service routine for timer
ISR(TIMER3_COMPA_vect)
{
	time++;
}
//initialising timer 3 registers
void timer_init(void)
{
	TCCR3A |= (1 << COM3A1); //clears OC1A/OC1B on compare match
	TCCR3B |= (1 << WGM32); //set to CTC mode
	
	TIMSK3 |= (1 << OCIE3A); //enable CTC interrupt
	sei(); //enable global interrupts
	
	OCR3A = 999; //sets the CTC compare value to 999 ticks or 1ms given 1MHz clock
}

void timer_start(void)
{
	TCCR3B |= (1 << CS31); //1 scaling CPU clock source for timer
}

void timer_stop(void)
{
	TCCR3B &amp;= 0xF8;
}
/*changes the brightness of LED colour by converting normalised brightness
* to the off time required in terms of PWM duty cycle 
*/
void update_LED(uint8_t LED, uint8_t colour, double brightness)
{
	int col = LED / 9;
	int row = LED % 9;
	
	uint16_t off_time = (uint16_t)(4095*brightness*bright_scale);
	
	RGB_arr[colour][row][col] = off_time;
}
//reads the LED array data and displays it until the timer runs out
void disp_LED(long period)
{
	int arr_idx[3][9] = {{0,3,6,10,13,0,3,6,10},{1,4,8,11,14,1,4,8,11},{2,5,9,12,15,2,5,9,12}};
	uint8_t port_arr[2][9] = {{0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},{0xFF,0xFE,0xFD,0xFB,0xF7,0xEF,0xDF,0xBF,0x7F}};
	uint8_t add_arr[9] = {0x28,0x28,0x28,0x28,0x28,0x29,0x29,0x29,0x29};
	int i, k, j, reg_on_L;
	
	long start = time;
	long elapsed = 0;
	uint8_t address;
	
	timer_start();
	while(elapsed < period)
	{
		PORTA ^= 0x80;
		for(i=0;i<9;i++)
		{
			PORTA = port_arr[0][i];
			PORTD = port_arr[1][i];
			
			for(k=0;k<9;k++)
			{	
				address = add_arr[k];
			
				for(j=0;j<3;j++) { reg_on_L = 6 + 4*arr_idx[j][k]; LED_write_reg(address,reg_on_L + 2,(uint8_t)(RGB_arr[j][k][i])); LED_write_reg(address,reg_on_L + 3,(uint8_t)(RGB_arr[j][k][i] >> 8));				
				}
			}
		}
		elapsed = time - start;
	}
	timer_stop();
	time = 0;
	PORTD = 0xFF;
}
//initialises the LED driving ports on the MCU and the LED array
void LED_row_init(void)
{
	DDRD = 0xFF;
	DDRA = 0xFF;
	PORTD = 0xFF;
	PORTA |= 0x01;
	
	int i,k,j;
	for(i=0;i<9;i++)
	{
		for(k=0;k<9;k++)
		{
			for(j=0;j<3;j++)
				RGB_arr[j][i][k] = 0;
		}
	}
}
//turns off all LEDs by setting duty cycle to 0
void LED_clear(void)
{
	int i,k,j;
	for(i=0;i<9;i++)
	{
		for(k=0;k<9;k++)
		{
			for(j=0;j<3;j++)
				RGB_arr[j][i][k] = 0;
		}
	}
}
// creates a random 12 bit number
uint16_t rand_number(void)
{
	return (uint16_t)rand();
}
// turns both LED drive chips on
void LEDdrive_on(void)
{
	MCUCR |= (1<<JTD);
	
	DDRC = 0xFF; //sets PC2 and PC3 as output
	PORTC = 0x00; //sets PC2 and PC3 HIGH
}
// turns both LED drivers off
void LEDdrive_off(void)
{
	PORTC = 0xFF;
}
//initialises the required registers for the PCA9685
void LEDdrive_init(void)
{
	while (LED_write_reg(add1,MODE1,0x01) == -1); //default MODE1 except Sleep normal
	while (LED_write_reg(add1,MODE2,0x04) == -1); //sets INVRT 0, OCH 0, OTDRV 1, LEDs off when OE = 1 
	while (LED_write_reg(add2,MODE1,0x01) == -1); //default MODE1 except Sleep normal
	while (LED_write_reg(add2,MODE2,0x04) == -1); //sets INVRT 0, OCH 0, OTDRV 1, LEDs off when OE = 1 
	
	LED_write_reg(add1,250,0x00);
	LED_write_reg(add1,251,0x00);
}
//writes data to designated register
int LED_write_reg(uint8_t address, uint8_t reg, uint8_t data)
{
	TWI_start();
	if (TWI_getStatus() != 0x08)
			return -1;
	TWI_write(0x80 | (address<<1)); //writes the device address with MSB being 1 and LSB being 0: write
	if (TWI_getStatus() != 0x18)
			return -1;
	TWI_write(reg);
	if (TWI_getStatus() != 0x28)
			return -1;
	TWI_write(data);
	if (TWI_getStatus() != 0x28)
			return -1;
	TWI_stop();
	return 1;	
}
//initialise the two wire interface
void TWI_init(void)
{
	//set SCL clock to 100kHz
	TWSR = 0x00; //set prescaling to 1
	TWBR = 0x02; //set bit rate register
	
	TWCR = (1<<TWEN); //enables TWI 
}
//sends TWI start packet
void TWI_start(void)
{
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN); //clears interrupt flag, sets start bit, enables TWI 
	while ((TWCR &amp; (1<<TWINT)) == 0);	//wait until interrupt flag is set (0)
}
//sends TWI stop packet
void TWI_stop(void)
{
	TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN); //clears interrupt flag, sets stop bit, enables TWI
}
//sends 8bit data via TWI
void TWI_write(uint8_t u8data)
{
    TWDR = u8data;	//sets the data
    TWCR = (1<<TWINT)|(1<<TWEN);	//clears interrupt flag, enables TWI
    while ((TWCR &amp; (1<<TWINT)) == 0);	//waits for interrupt flag
}
// read with acknowledge byte
uint8_t TWI_readACK(void)
{
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA); //clears interrupt, enables TWI, sets acknowledge enable bit
    while ((TWCR &amp; (1<<TWINT)) == 0); //waits for interrupt flag
    return TWDR;	// returns the read data
}
// read without acknowledge byte
uint8_t TWI_readNACK(void)
{
    TWCR = (1<<TWINT)|(1<<TWEN);	//clears interrupt, enables TWI
    while ((TWCR &amp; (1<<TWINT)) == 0);	//waits for interrupt 
    return TWDR;	//returns the read data
}
//reads the return packet for TWI
uint8_t TWI_getStatus(void)
{
    uint8_t status;
    status = TWSR &amp; 0xF8;  //mask status 
    return status;
}
