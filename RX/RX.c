//----��������----//
#define  F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define UP    4								// �������������� �������� ����� ��������
#define DOWN  7								// �����/����, 0 - ����������!
#define ICR_TOP 60      					// ������� ��� (15 �������� � ���� �������)

#define OCR_MIN 14							// ������ ��� ������������
#define OCR_CEN 23							// �������������� �/�2
#define OCR_MAX 32

volatile unsigned char start=1;				// ���� ������� ��������� - ��� �������!
volatile unsigned char shift=0;             // ������������ ��������
volatile unsigned char count=0;				// ������� �������
volatile unsigned char count_drv=64; 	    // ������� �������� ������
volatile unsigned char count_srv=9;	        // ������� �������� ����� 
volatile unsigned char binary=0x00;  		// ���������� ������  
volatile unsigned char signal=0;			// ����������� �����������: 1-������; 2-�����;
volatile unsigned char counter=0;			// ������� �������� ������� T/C0
volatile unsigned char _backL=0;			// ���� ����-��������: 1 - ������ ������; 0 - ���.
volatile unsigned char count_buf=0;

ISR(USART_RXC_vect)
{
 unsigned char status=0x00, data=0x00;

 // ��������� watchdog �� 120 ms ���� ������ �������
 if(start)
 { 
   wdt_enable(WDTO_120MS);					
   start=0;
 }
    					
 // ���������� ������
 status=UCSRA;
 if((status&(_BV(FE)|_BV(PE)|_BV(DOR))))
 {
  while(UCSRA&_BV(RXC)) data=UDR;
  return;
 }

 data=UDR;

 // ���� ������������
 if(count==0)
 { 
  // ������ ����������� - �������� ������� ������, ������ ���������� ������
  if(data==0xF0)
  {
    count++;
    wdt_reset();
  }
 }
 else
 {
  // ���� ����������� �� � ���� - ����� ��������
  if(data==0xF0) count=0;
  // ����� - ����������� ������
  else
  {
    switch(count)
    {
      case 1:
      {
	    if(data&_BV(6))shift=DOWN;
		else if(data&_BV(7))shift=UP;
		else shift=0;

		data&=~(_BV(7)|_BV(6));

		count_drv=data;
	    break;
	  }

      case 2:
	  {
        count_srv=data;
	    break;
	  }

	  case 3:
	  {
        data&=~(_BV(7)|_BV(6));
		binary=data;
	  }
    }
   
    count++;
	if(count>3) count=0;
  }
 }
}

ISR(TIMER0_OVF_vect)
{
 counter++;

 if(counter==10)
 {
   counter=0;

   // ����-������
   PORTB&=~_BV(PB4);
   _backL=0;

   // �����������
   if(signal!=0)
   if(signal==1)PORTD^=_BV(PD6);
   else PORTD^=_BV(PD7);
 }
}

int main(void)
{
 // ���������� watchdog �� ������ ������
 wdt_disable();

 // ������ OC1B/OC1A ��� ��� DRV; OC2 ��� SRV;PB0 - ������ ���;
 DDRB=0x1F;
 PORTB=0xE0;

 // PortC - ��������� ����� (����� reset)
 DDRC=0x3F;
 PORTC=0x00;

 // PD6, PD7 - ����� �����������; ��������� - �����
 DDRD=0xC0;
 PORTD=0x3F;

 TCCR1A=0x00;
 TCCR1B=0x00;

 // ��������� ��� Driver: N=256; ����=FastPWM; OC1A,OC1B  - ��������.; Warning COM DESABLED!!!
 TCCR1B|=_BV(WGM13)|_BV(WGM12)|_BV(CS12);
 //TCCR1A|=_BV(WGM11)|_BV(COM1A1)|_BV(COM1B1);
 TCCR1A|=_BV(WGM11);

 // ������ ������������ ��� ������� �/�1
 ICR1=ICR_TOP;
 OCR1A=0x00;
 OCR1B=0x00;

 // ����������� �������� �/�2 - �����
 OCR2=OCR_CEN; 
 TCCR2=0x00;
 // ��������� FastPWM Servo; ������; N=1024; F=Fcpu/256*N=61 Hz;
 TCCR2|=_BV(WGM21)|_BV(WGM20)|_BV(COM21)|_BV(CS22)|_BV(CS21)|_BV(CS20);

 // �������� �������� 9600 bod
 UBRRH=0;
 UBRRL=103;

 // ������������� UCSRA
 UCSRA=0x00;

 // D1 - ����� USART; ���������� �� ������;
 UCSRB=0x00;
 UCSRB|=_BV(RXEN)|_BV(RXCIE);

 // UCSRC: 8 ��� ������; �������� ��������;
 UCSRC=_BV(URSEL)|_BV(UPM1)|_BV(UCSZ1)|_BV(UCSZ0);

 // ������������� ��������� Servo
 count_srv=9;

 // ���������� ��������� ����������� (�� ������)
 ACSR|=_BV(ACD);

 // ������������� T/C0
 TCNT0=0;
 TIMSK|=_BV(TOIE0);
 TCCR0|=_BV(CS02)|_BV(CS00);

 // ����� ���������� ����������
 sei();

 while(1)
 { 
  // ����������� ������������
  OCR2=OCR_MIN+count_srv;

  // ������ � ����������
  switch(shift)
  {
    case 0:
	{
	  OCR1B=OCR1A=0x00;	
	  TCCR1A&=~(_BV(COM1A1)|_BV(COM1B1));

	  PORTB&=~(_BV(PB0)|_BV(PB4));
	  break;     
	}

	case UP:
	{
	  
	  OCR1B=0x00;
      OCR1A=count_drv; 
      TCCR1A|=_BV(COM1A1)|_BV(COM1B1);

	  PORTB&=~_BV(PB0);
      break;
	}

	case DOWN:
	{  
	  OCR1A=0x00; 
      OCR1B=count_drv;
	  TCCR1A|=_BV(COM1A1)|_BV(COM1B1);

	  PORTB|=_BV(PB0);
	}
  }

  // � ����-��������
  if(count_buf>count_drv)
  {
	_backL=1;
	PORTB|=_BV(PB4);
  }
  count_buf=count_drv;

  // ����������� ���������
  PORTC=binary;

  // �������� �����������
  if(count_srv==0 || count_srv==18)
  {
    if(signal==0)
	{
      if(count_srv==0)signal=1;
	  else signal=2;
	}
  }
  else
  if(signal!=0)
  {
    signal=0;
    PORTD&=~(_BV(PD7)|_BV(PD6));
  }
 

 }


 return 0;
}
