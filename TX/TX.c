//----����������----//
#define  F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>

#define DELAY 99							// �������� �������� ������ (default=99 (10 ms))

#define OCR_MIN 14							// ������ ��� ������������
#define OCR_CEN 23							// �������������� �/�2
#define OCR_MAX 32

volatile unsigned char count=0;				// ������� �������
volatile unsigned char count_drv=16; 	    // ������� �������� ������ (�� �������������)
volatile unsigned char count_srv=9;	    	// ������� �������� ����� (�� �������������)

volatile unsigned char word[]={0xF0,0x00,0x09,0x00};

ISR(INT0_vect)								// ��������� ���������� �������� �������� (work!)
{
 if((PIND&_BV(PD0)))
 {
  if(count_drv>0)count_drv--;
 }
 else
 {
  if(count_drv<32)count_drv++;
 }

  //---shift=0;
  word[1]=0x00;

  if(count_drv>17)
  { 
   //---shift=UP;
//   if(count_drv<24)word[1]=7; else   
   word[1]=(count_drv-17)*4;

   word[1]|=_BV(7);
  }   
  else
  if(count_drv<15)
  {
   //---shift=DOWN;
//  if(count_drv>8) word[1]=7; else 
   word[1]=(15-count_drv)*4;

   word[1]|=_BV(6);
  } 

}


ISR(INT1_vect)							// ��������� ���������� �������� ����� (work!)
{
 if((PIND&_BV(PD4)))
 {
  if(count_srv>0)count_srv--;
 }
 else
 {
  if(count_srv<18)count_srv++;
 }

 word[2]=count_srv;
}


ISR(TIMER0_OVF_vect) 					// �������� ������ ����� ��������
{
 while(!(UCSRA&_BV(UDRE)));
 UDR=word[count];

 TCNT0=DELAY;
 count++;
 if(count>3)count=0;
}


int main(void)
{
 // ����� ������ � ������������� �������������� (���������)
 DDRB=0x00;
 PORTB=0x3F;

 // ����� �������� PD0,PD2,PD3,PD4 (��� ��������); ������ ������ PD6,PD7 (� ���������);
 DDRD=0x00;
 PORTD=0xE0;

 // ������ ����������� ��������� min/max; ��������� - ����� (� ���������, ����� PC6 Reset)
 DDRC=0x0F;
 PORTC=0xB0;
 
 // ����������� ����� INT0 / INT1 �� ����������
 MCUCR|=_BV(ISC01)|_BV(ISC11);
 // ��������� ��� ���������� � GIMSC
 GIMSK|=_BV(INT0)|_BV(INT1); 

 // �������� �������� 9600 bod
 UBRRH=0;
 UBRRL=103;

 // ������������� UCSRA
 UCSRA=0x00;

 // D1 - ����� USART
 UCSRB=0x00;
 UCSRB|=_BV(TXEN);

 // UCSRC: 8 ��� ������; �������� ��������;
 UCSRC=_BV(URSEL)|_BV(UPM1)|_BV(UCSZ1)|_BV(UCSZ0);

 // ��������� �/C0 (�������� ��������=10��); N=1024; ���������� �� ������������; 
 TIMSK|=_BV(TOIE0);
 TCCR0|=_BV(CS02)|_BV(CS00);
 TCNT0=DELAY; 

 // ������������� ��������� �����
 count_srv=9;

  // ���������� ��������� ����������� (�� ������)
 ACSR|=_BV(ACD);

 // ����� ���������� ����������
 sei();

 while(1)
 { 
   // ���������� ���������� �������
   word[3]=~PINB;


   // ����� � 0 ��������� srv
   if(!(PIND&(1<<PD6)))
   {
     while(!(PIND&(1<<PD6)));
     word[2]=9;
	 count_srv=9;
   }

   // c���� � 0 ��������� drv
   if(!(PIND&(1<<PD7)))
   {
     while(!(PIND&(1<<PD7)));
     word[1]=0x00;
	 count_drv=16;
   }
   
   // ��������� ����������� max/min �������
   switch(count_drv)
   {
     case 0  : PORTC|=_BV(PC1); break;
	 case 32 : PORTC|=_BV(PC0); break;   
     default : PORTC&=~(_BV(PC1)|_BV(PC0));
   }
   
   // ��������� ����������� max/min �����
   switch(count_srv)
   {
     case 0  : PORTC|=_BV(PC3); break;
	 case 18 : PORTC|=_BV(PC2); break;   
     default : PORTC&=~(_BV(PC3)|_BV(PC2));
   }

 }

 return 0;
}
