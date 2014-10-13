#include <stdio.h>
#include <at89lp51rd2.h>

// ~C51~ 
 
#define CLK 22118400L
#define BAUD 115200L
#define BRG_VAL (0x100-(CLK/(32L*BAUD)))
#define buzzer P3_2

//We want timer 0 to interrupt every 100 microseconds ((1/10000Hz)=100 us)
#define FREQ 15000L
#define TIMER0_RELOAD_VALUE (65536L-(CLK/(12L*FREQ)))

//These variables are used in the ISR
volatile unsigned char pwmcount;
volatile unsigned char pwm1;
volatile unsigned char pwm2;
volatile unsigned char pwm3;
volatile unsigned char pwm4;
volatile unsigned int mode;
volatile float d;
volatile int array[10];
volatile int i;
volatile int count;
//volatile unsigned int minn=1;

void ledyellow_left_on ();
void ledyellow_right_on ();
void ledred_off ();
void ledyellow_off ();
void ledred_on ();

unsigned char _c51_external_startup(void)
{
	// Configure ports as a bidirectional with internal pull-ups.
	P0M0=0;	P0M1=0;
	P1M0=0;	P1M1=0;
	P2M0=0;	P2M1=0;
	P3M0=0;	P3M1=0;
	AUXR=0B_0001_0001; // 1152 bytes of internal XDATA, P4.4 is a general purpose I/O
	P4M0=0;	P4M1=0;
    
    // Initialize the serial port and baud rate generator
    PCON|=0x80;
	SCON = 0x52;
    BDRCON=0;
    BRL=BRG_VAL;
    BDRCON=BRR|TBCK|RBCK|SPD;
	
	// Initialize timer 0 for ISR 'pwmcounter()' below
	TR0=0; // Stop timer 0
	TMOD=0x01; // 16-bit timer
	// Use the autoreload feature available in the AT89LP51RB2
	// WARNING: There was an error in at89lp51rd2.h that prevents the
	// autoreload feature to work.  Please download a newer at89lp51rd2.h
	// file and copy it to the crosside\call51\include folder.
	TH0=RH0=TIMER0_RELOAD_VALUE/0x100;
	TL0=RL0=TIMER0_RELOAD_VALUE%0x100;
	TR0=1; // Start timer 0 (bit 4 in TCON)
	ET0=1; // Enable timer 0 interrupt
	EA=1;  // Enable global interrupts
	
	pwmcount=0;
    
    return 0;
}

// Interrupt 1 is for timer 0.  This function is executed every time
// timer 0 overflows: 100 us.
void pwmcounter (void) interrupt 1
{
	if(++pwmcount>99) pwmcount=0;
	P2_4=(pwm1>pwmcount)?1:0;
	P2_5=(pwm2>pwmcount)?1:0; // if true then output 1 (high) else output 0 (low)
							// if ?0:1 then output is reversed (ie. high->low & low->high)
	P2_6=(pwm3>pwmcount)?1:0;
	P2_7=(pwm4>pwmcount)?1:0;
}
// need to wait 20 milliseconds -> set for transmitter as well
void wait_bit_time(void){
	_asm
		mov r2,#4
	L3:	mov r1,#62
	L2:	mov r0,#184
	L1:	djnz r0,L1
		djnz r1,L2
		djnz r2,L3
		ret
	_endasm;
}
// need to wait 30 seconds
void wait_one_and_half_bit_time(void){
	_asm
		mov r2,#2
	L6:	mov r1,#186
	L5:	mov r0,#184
	L4:	djnz r0,L4
		djnz r1,L5
		djnz r2,L6
		ret
	_endasm;
}

void SPIWrite( unsigned char value)
{
	SPSTA&=(~SPIF); // Clear the SPIF flag in SPSTA
	SPDAT=value;
	while((SPSTA & SPIF)!=SPIF); //Wait for transmission to end
}

unsigned int GetADC(unsigned char channel) {
	unsigned int adc;
	// initialize the SPI port to read the MCP3004 ADC attached to it.
	SPCON&=(~SPEN); // Disable SPI
	SPCON=MSTR|CPOL|CPHA|SPR1|SPR0|SSDIS;
	SPCON|=SPEN; // Enable SPI
	P1_4=0; // Activate the MCP3004 ADC.
	SPIWrite(channel|0x18); // Send start bit, single/diff* bit, D2, D1, and D0 bits.
	for(adc=0; adc<10; adc++); // Wait for S/H to setup
	SPIWrite(0x55); // Read bits 9 down to 4
	adc=((SPDAT&0x3f)*0x100);
	SPIWrite(0x55); // Read bits 3 down to 0
	P1_4=1; // Deactivate the MCP3004 ADC.
	adc+=(SPDAT&0xf0); // SPDR contains the low part of the result.
	adc>>=4;
	return adc;
}

float voltage (unsigned char channel)
{
	return ( (GetADC(channel)*5.0)/1023.0 ); // VCC=5.00V (measured)
}

unsigned char rx_byte ( float min )//0.5 min , reading is off , try to find value read
{
	unsigned char j, val;
	int v;
	//Skip the start bit
	val=0;
	wait_one_and_half_bit_time();
	for(j=0; j<8; j++)
	{
		v=GetADC(0);
		val|=(v>min)?(0x01<<j):0x00;
		wait_bit_time();
	}
//Wait for stop bits
	wait_one_and_half_bit_time();
	return val;
}

void wait1s (void)
{
	_asm	
		;For a 22.1184MHz crystal one machine cycle 
		;takes 12/22.1184MHz=0.5425347us
	    mov R2, #20
	L9:	mov R1, #248
	L8:	mov R0, #184
	L7:	djnz R0, L7 ; 2 machine cycles-> 2*0.5425347us*184=200us
	    djnz R1, L8 ; 200us*250=0.05s
	    djnz R2, L9 ; 0.05s*20=1s
	    ret
    _endasm;
}

void wait200us (void)
{
	_asm	
		;For a 22.1184MHz crystal one machine cycle 
		;takes 12/22.1184MHz=0.5425347us
	    mov R2, #4
	L93:	mov R1, #248
	L83:	mov R0, #184
	L73:	djnz R0, L73 ; 2 machine cycles-> 2*0.5425347us*184=200us
	    djnz R1, L83 ; 200us*250=0.05s
	    djnz R2, L93 ; 0.05s*20=1s
	    ret
    _endasm;
}

void wait180 (void)
{
	_asm	
		;For a 22.1184MHz crystal one machine cycle 
		;takes 12/22.1184MHz=0.5425347us
	    mov R2, #14
	L91:	mov R1, #253
	L81:	mov R0, #200
	L71:	djnz R0, L71 ; 2 machine cycles-> 2*0.5425347us*184=200us
	    djnz R1, L81 ; 200us*250=0.05s
	    djnz R2, L91 ; 0.05s*20=1s
	    ret
    _endasm;
}

void wait45 (void)
{
	_asm	
		;For a 22.1184MHz crystal one machine cycle 
		;takes 12/22.1184MHz=0.5425347us
	    mov R2, #33
	L92:	mov R1, #62
	L82:	mov R0, #180
	L72:	djnz R0, L72 ; 2 machine cycles-> 2*0.5425347us*184=200us
	    djnz R1, L82 ; 200us*250=0.05s
	    djnz R2, L92 ; 0.05s*20=1s
	    ret
    _endasm;
}

void wait33 (void)
{
	_asm	
		;For a 22.1184MHz crystal one machine cycle 
		;takes 12/22.1184MHz=0.5425347us
	    mov R2, #30
	L90:	mov R1, #62
	L80:	mov R0, #180
	L70:	djnz R0, L70 ; 2 machine cycles-> 2*0.5425347us*184=200us
	    djnz R1, L80 ; 200us*250=0.05s
	    djnz R2, L90 ; 0.05s*20=1s
	    ret
    _endasm;
}

void ledred_on (void)
{
	P0_0 = !P0_0;
	P0_1 = !P0_1;
	P0_2 = !P0_2;
	P1_0 = !P1_0;
	P1_1 = !P1_1;
	P1_2 = !P1_2;
}

void ledyellow_left_on (void)
{
	P0_7 = !P0_7;
	P4_0 = !P4_0;
	P4_4 = !P4_4;
}

void ledyellow_right_on (void)
{
	P3_4 = !P3_4;
	P3_5 = !P3_5;
	P3_6 = !P3_6;
}

void ledred_off (void)
{
	P0_0 = 1;
	P0_1 = 1;
	P0_2 = 1;
	P1_0 = 1;
	P1_1 = 1;
	P1_2 = 1;
}

void ledyellow_off (void)
{
	P0_7 = 1;
	P4_0 = 1;
	P4_4 = 1;
	P3_4 = 1;
	P3_5 = 1;
	P3_6 = 1;
}

void motor1forward(void){
	pwm1=90;
	pwm2=0;
}

void motor1back(void){
	pwm1=0;
	pwm2=90;	
}

void motor2forward(void){
	pwm3=90;
	pwm4=0;
}

void motor2back(void){
	pwm3=0;
	pwm4=90;
}

void stopmotor1(void){
	pwm1=0;
	pwm2=0;
	ledred_off();
	ledyellow_off();
}

void stopmotor2(void){
	pwm3=0;
	pwm4=0;
	ledred_off();
	ledyellow_off();
}

void motor1forward_turn(void){
	pwm1=40;
	pwm2=0;
}

void motor1backward_turn(void){
	pwm1=0;
	pwm2=40;
}

void motor2forward_turn(void){
	pwm3=40;
	pwm4=0;
}

void motor2backward_turn(void){
	pwm3=0;
	pwm4=40;
}
/*
void commands(char val, float d1, float d2, float dlow, float dhigh){
	
	if(val==0B_0000_0001){	//move closer
		motor1back();
		motor2back();
		wait1s();
		wait1s();
		stopmotor1();
		stopmotor2();
	}
	else if(val==0B_0000_1000){	//move farther
		motor1forward();
		motor2forward();
		wait1s();
		wait1s();
		stopmotor1();
		stopmotor2();
	}
	else if(val==0B_0000_1001){	//rotate 180
		motor1back();
		wait180();
		stopmotor1();
	}	
	else if(val==0B_0010_0000){	//parallel parking
		motor2back();
		wait45();
		
		stopmotor1();
		stopmotor2();
		wait200us();
		
		motor1back();
		motor2back();
		wait45();
		wait45();
		
		stopmotor1();
		stopmotor2();
		wait200us();
		
		motor1back();
		wait45();
	
		stopmotor1();
		stopmotor2();
	}*/
	/*
	if(val==0B_00000001){//forward
		dlow+=0.3;
		dhigh+=0.3;
		motor1forward();
		motor2forward();
		wait1s();
		wait1s();
		stopmotor1();
		stopmotor2();
		val=0B_00000000;
	
	}
	else if(val==0B_00000011){//backward
		motor1back();
		motor2back();
		wait1s();
		wait1s();
		stopmotor1();
		stopmotor2();
		val=0B_00000000;
		
	}
	else if (val==0B_00000111){//rotate
		motor1back();
		wait180();
		stopmotor1();
		wait1s();
		val=0B_00000000;
		
	}
	else if (val==0B_00001111){//parallel park
		motor2back();
		wait45();
	
		stopmotor1();
		stopmotor2();
		wait200us();
	
		motor1back();
		motor2back();
		wait45();
		wait45();
	
		stopmotor1();
		stopmotor2();
		wait200us();
	
		motor1back();
		wait45();	
	
		stopmotor1();
		stopmotor2();
		wait1s();
		val=0B_00000000;
		
	}*/
	
	/*
	if(d1>dhigh ){
		motor1back();
	}
	else if(d2>dhigh){
		motor2back();
	}
	else if(d1<dlow){
		motor1forward();
	}
	else if(d2<dlow){
		motor2forward();
	}*/
	/*else{
		stopmotor1();
		stopmotor2();
	}	
}*/


void move_closer()
	{

	/*	motor1forward();
		motor2forward();
		wait1s();
		stopmotor1();
		stopmotor2();*/
		if (d<2.6)
		d=d+0.4;
		return;
	}

void move_farther()
	{
/*
		motor1back();
		motor2back();
		wait1s();
		stopmotor1();
		stopmotor2();*/
		if( d>0.9 )
	    d=d-0.4;
		return;
	}
	
void rotate_180_forward (void)
	{
		ledyellow_right_on();
		motor1forward();
		motor2back();
		wait180();
		stopmotor1();
		stopmotor2();
		return;
	}
	
void rotate_180_backward (void)
	{
		ledyellow_left_on();
		motor1back();
		motor2forward();
		wait180();
		stopmotor1();
		stopmotor2();
		return;
	}

void parallel_parking (void)
	{
		ledyellow_right_on();
		motor1back();
		wait45();
		
		stopmotor1();
		stopmotor2();
		wait200us();
		
		ledred_on();
		motor1back();
		motor2back();
		wait45();
		wait45();
		
		stopmotor1();
		stopmotor2();
		wait200us();
		
		ledyellow_right_on();
		motor2back();
		wait45();
	
		stopmotor1();
		stopmotor2();
		return;
	}

void reverse_parallel_parking (void)
	{
		ledyellow_left_on();
		motor2back();
		wait45();
		
		stopmotor1();
		stopmotor2();
		wait200us();
		
		ledred_on();
		motor1back();
		motor2back();
		wait45();
		wait45();
		
		stopmotor1();
		stopmotor2();
		wait200us();
		
		ledyellow_left_on();
		motor1back();
		wait45();
	
		stopmotor1();
		stopmotor2();
		return;
	}

float abs(float a,float b)
	{
		if(a>b)
			return (a-b);
		else
			return (b-a);
	}

void markforward (void)
{
	array[i] = 1;
	i++;
	count++;
	wait45();
}

void markbackward (void)
{
	array[i] = 0;
	i++;
	count++;
	wait45();
}

void markleft (void)
{
	array[i] = 2;
	i++;
	count++;
	wait45();
}

void markright (void)
{
	array[i] = 3;
	i++;
	count++;
	wait45();
}

void executememory (void)
{
	wait45();
	
	for (i=0; i<count ; i++ )
	{
		if(array[i]==1)
		{
			motor1forward();
			motor2forward();
			wait45();
			stopmotor1();
			stopmotor2();
		}
		if(array[i]==0)
		{
			motor1back();
			motor2back();
			wait45();
			stopmotor1();
			stopmotor2();
		}
		if(array[i]==2)
		{
			motor1back();
			motor2forward();
			wait45();
			stopmotor1();
			stopmotor2();
		}
		if(array[i]==3)
		{
			motor1forward();
			motor2back();
			wait45();
			stopmotor1();
			stopmotor2();
		}
	}
	
	i=0;
	count=0;
	
}

void main (void)
{	
	float d1 = 0.0;
	float d2 = 0.0;
	unsigned char val;
	float dlow = 0.8;
	float dhigh = 2.0;
	float volt;
	float stop_tolerance = 0.3;
	float turn_tolerance = 0.45;
	i=0;
	count=0;
	ledred_off();
	ledyellow_off();
	
	
	d = 1.9;
	
	stopmotor1();
	stopmotor2();
	mode = 0;
	val = 0B_0000_0000;
	//d1 and d2
  while(1){

		volt = voltage(0);
		val=rx_byte(0.5);
		d1 = voltage(0);
		d2 = voltage(1);

		while(d1!=0 && d2!=0)
		{
			d1 = voltage(0);
			d2 = voltage(1);
			if(d1<3.00)
		{
				d1 = d1*1.66;
		}
			
			if(d1>3.00)
			{
				d1 = 3.01;
			}
			
			printf("d1 = %f\t d2 = %f\t val=%d\n",d1,d2,val);
					
			if( (abs(d,d1)<stop_tolerance&&abs(d,d2)<stop_tolerance) || d1<0.01 || d2<0.01 ){
				stopmotor1();
				stopmotor2();

			}else if (d1>d && d1>d2 && abs(d1, d2) > turn_tolerance) {
				//stopmotor1();
				ledyellow_left_on();
				motor2forward_turn();
				motor1back();
			}
			else if (d2>d && d2>d1 && abs(d1, d2) > turn_tolerance) {
				//stopmotor2();
				ledyellow_right_on();
				motor1forward_turn();
				motor2back();
			}
			else if (d1<d && d1>d2 && abs(d1, d2) > turn_tolerance) {
				//stopmotor1();
				ledyellow_left_on();
				motor1backward_turn();
				motor2forward();
			}
			else if (d2<d && d2>d1 && abs(d1, d2) > turn_tolerance) {
				//stopmotor2();
				ledyellow_right_on();
				motor2backward_turn();
				motor1forward();
			}
			else if (d1>d && d2>d && abs(d1, d2) < 0.45) {
				ledred_on();
				motor1back();
				motor2back();
				buzzer = !buzzer;
				TH0=RH0=TIMER0_RELOAD_VALUE/0x100;
				TL0=RL0=TIMER0_RELOAD_VALUE%0x100;
			}
			else if (d1<d && d2<d && abs(d1, d2) < 0.45) {
				motor1forward();
				motor2forward();
			}
			else {
				stopmotor1();
				stopmotor2();
			}
		
		volt=voltage(0);
		
		 
		}// end volt != 0
		
		val=rx_byte(0.5);
		printf("d1 = %f\t d2 = %f\t val=%d\n",d1,d2,val);
		//val=rx_byte(0.5);
		
	mode = 1;
	
	// end while mode 0
	//	d1 = voltage(0);
	   // d2 = voltage(1);
	
	//	printf("d1 = %f\t d2 = %f\t val=%d\n",d1,d2,val);
	while(mode == 1){
		if(val==192){
			d = 1;
			stop_tolerance = 0.10;
			turn_tolerance = 0.13;
			mode = 0;
		}
		else if (val==136){
			d = 1.5;
			stop_tolerance = 0.40;
			turn_tolerance = 0.35;
			mode = 0;
		}
		else if(val==57){
			d = 1.9;
			stop_tolerance = 0.30;
			turn_tolerance = 0.45;
			mode = 0;
		}	
		else if(val==230 || val==198){//206	
			d = 2.4;
			stop_tolerance = 0.30;
			turn_tolerance = 0.45;
			mode = 0;
		}
		else if(val==246){	
			rotate_180_forward();
			stopmotor1();
			stopmotor2();
			wait1s();
			val=rx_byte(0.5);
			mode = 0;
		}
		else if(val==222){	
			parallel_parking();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			wait1s();
			mode = 0;
		}
		else if(val==207||val==231||val==239||val==247){ //mode button is pressed	
			reverse_parallel_parking();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			wait1s();
			mode = 0;
		}
		else if(val==15||val==7){ //mode button is pressed	
			wait1s();
			wait1s();
			wait1s();
			
			motor1forward();
			motor2forward();
			wait1s();
			stopmotor1();
			stopmotor2();
			
			
			val=rx_byte(0.5);
			val=0;
			mode = 0;
		}
		///////////////////////
		else if(val==208){		//front pin
			markforward();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			mode = 0;
		}
		else if(val==177 || val==209){	//back pin
			markbackward();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			mode = 0;
			}
		else if(val==183 || val==175){	//right pin
			markright();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			mode = 0;
		}
		else if(val==191){	 //left pin
			markleft();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			mode = 0;
		}
		else if(val==179){	 //left pin
			executememory();
			stopmotor1();
			stopmotor2();
			val=rx_byte(0.5);
			val=0;
			mode = 0;
		}
		else
		{
			mode = 0;
		}	
	} // end while mode 1
	
	} // end while 1
} // end funcition
