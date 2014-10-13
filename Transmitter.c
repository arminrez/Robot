#include <stdio.h>
#include <at89lp51rd2.h>

// ~C51~ 
 
#define CLK 22118400L
#define BAUD 115200L
#define BRG_VAL (0x100-(CLK/(32L*BAUD)))

//We want timer 0 to interrupt every 100 microseconds ((1/10000Hz)=100 us)
#define FREQ 15000L
#define TIMER0_RELOAD_VALUE (65536L-(CLK/(12L*FREQ*2L)))

#define rs          P2_5
#define rw          P2_6
#define enable      P2_7
#define lcd_port    P0

volatile unsigned int txon;

void lcd_command(unsigned char command);
void lcd_display(unsigned char display);
void write_string1(unsigned char *var1);
void write_string2(unsigned char *var2);
void delay_ms();
void lcd_initialization();

unsigned char _c51_external_startup(void)
{
	// Configure ports as a bidirectional with internal pull-ups.
	P0M0=0;	P0M1=0;
	P1M0=0;	P1M1=0;
	P2M0=0;	P2M1=0;//0B_1111_1111;
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
	
	P2_0=0;
	P2_1=1;
	
	rs=0;
	rw=0;
	lcd_initialization();
    
    return 0;
}

void lcd_initialization()
{
	lcd_command(0x30);// 2 line 5x8
	lcd_command(0x30);// 2 line 5x8
	lcd_command(0x30);// 2 line 5x8
	lcd_command(0x38);// cursor on
	lcd_command(0x0C);// clear display
	lcd_command(0x1);
	lcd_command(0x06);// entry mode
}

void lcd_command(unsigned char command)
{
	lcd_port=command;
	rs=0;
	rw=0;
	enable=1;
	enable=0;
	delay_ms();
}


void lcd_display(unsigned char display)
{
	lcd_port = display;
	rs=1;
	rw=0;
	enable=1;
	enable=0;
	delay_ms();
}

void write_string1(unsigned char *var1)
{
	lcd_command(0x80);
 	while(*var1){
		lcd_display(*var1++); //send characters one by one
	}
}

void write_string2(unsigned char *var2)
{
  	lcd_command(0xC0);	
 	while(*var2){
		lcd_display(*var2++); //send characters one by one
	}
}

void delay_ms()  
{
	unsigned int i,j;
         for(i=0;i<30;i++)        //A simple for loop for delay
            for(j=0;j<255;j++);
}

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

/*
void wait_one_and_half_bit_time(void){
	_asm
		mov r2,#3
	L6:	mov r1,#12
	L5:	mov r0,#184
	L4:	djnz r0,L4
		djnz r1,L5
		djnz r2,L6
		ret
	_endasm;

}
*/
/*
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
}*/

void tx_byte ( unsigned char val ) {
	unsigned char j;
  	//Send the start bit
  	txon=0;
  	wait_bit_time();
  	for (j=0; j<8; j++){
    	txon=val&(0x01<<j)?1:0;
    	wait_bit_time();
 	}
  	txon=1;
  	//Send the stop bits
  	wait_bit_time();
  	wait_bit_time();
}

void pwmcounter (void) interrupt 1
{
	if(txon==1){
		P2_0=!P2_0;
		P2_1=!P2_1;
	}else{
		P2_0 = 0;
		P2_1 = 0;
	}
}

void main (void){
	txon=1;
	//commands
	lcd_command(0x1);
	write_string1("     Tether");
	write_string2("     Robot");
	while(1){	
		if(P1_2==0){ //move closer
			lcd_command(0x1);
			write_string1("   Set");
			write_string2("   Distance 1");
			EA=0;
			tx_byte(0B_0000_0001);
			while (P1_2==0){}
			EA=1;
		}else if(P1_4==0){ //move farther
			lcd_command(0x1);
			write_string1("   Set");
			write_string2("   Distance 2");
			tx_byte(0B_1000_1000);
		}else if(P1_6==0){ //rotate 180
			lcd_command(0x1);
			write_string1("   Set");
			write_string2("   Distance 3");
			tx_byte(0B_1001_1001);
		}else if(P4_1==0){ //parallel park
		printf("pin4.3   ");
			lcd_command(0x1);
			write_string1("   Set");
			write_string2("   Distance 4");
			tx_byte(0B_0110_0110);
		}else if(P3_3==0){
			tx_byte(0B_0111_0110);
			lcd_command(0x1);
			write_string1("   Rotate");
			write_string2("   180");
		}else if(P3_5==0){
			tx_byte(0B_0110_1110);
			lcd_command(0x1);
			write_string1("   Parallel");
			write_string2("   Park");
		}else if(P3_7==0){
			tx_byte(0B_0110_0111);
			lcd_command(0x1);
			write_string1("   Reverse");
			write_string2("   Parallel Park");
		}else if(P3_6==0){			//mic button
			tx_byte(0B_0000_0111);
		}else if (P1_0==0){			// FORWARD
			lcd_command(0x1);
			write_string1("   Write Command");
			tx_byte(0B_0101_0000);
			write_string2("   Forward");
		}else if (P1_1==0){			// BACK
			lcd_command(0x1);
			write_string1("   Write Command");
			tx_byte(0B_0101_0001);
			write_string2("   Back");
		}else if (P1_3==0){			//EXECUTE
			lcd_command(0x1);
			write_string1("   Write Command");
			tx_byte(0B_0101_0011);
			write_string2("   Execution");
		}else if (P1_5==0){			// RIGHT
			lcd_command(0x1);
			write_string1("   Write Command");
			tx_byte(0B_0101_0111);
			write_string2("   Right");
		}else if (P1_7==0){			// LEFT
			lcd_command(0x1);
			write_string1("   Write Command");
			tx_byte(0B_0101_1111);
			write_string2("   Left");
		}
		else{
	
		}
	}
}
