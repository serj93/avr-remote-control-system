//----ПЕРЕДАТЧИК----//
#define  F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>

#define DELAY 99							// задержка отправки пакета (default=99 (10 ms))

#define OCR_MIN 14							// пороги для сервопривода
#define OCR_CEN 23							// вырабатывается Т/С2
#define OCR_MAX 32

volatile unsigned char count=0;				// счетчик посылки
volatile unsigned char count_drv=16; 	    // текущее значение мотора (не преобразовано)
volatile unsigned char count_srv=9;	    	// текущее значение сервы (не преобразовано)

volatile unsigned char word[]={0xF0,0x00,0x09,0x00};

ISR(INT0_vect)								// обработка прерывание ЭНКОДЕРА драйвера (work!)
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


ISR(INT1_vect)							// обработка прерывания ЭНКОДЕРА сервы (work!)
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


ISR(TIMER0_OVF_vect) 					// отправка данных через интервал
{
 while(!(UCSRA&_BV(UDRE)));
 UDR=word[count];

 TCNT0=DELAY;
 count++;
 if(count>3)count=0;
}


int main(void)
{
 // входа кнопок с подтягивающим сопротивлением (дискретка)
 DDRB=0x00;
 PORTB=0x3F;

 // входа энкодера PD0,PD2,PD3,PD4 (без подтяжки); кнопки сброса PD6,PD7 (с подтяжкой);
 DDRD=0x00;
 PORTD=0xE0;

 // выхода светодиодов индикации min/max; остальные - входа (с подтяжкой, кроме PC6 Reset)
 DDRC=0x0F;
 PORTC=0xB0;
 
 // настраиваем выход INT0 / INT1 на прерывание
 MCUCR|=_BV(ISC01)|_BV(ISC11);
 // разрешаем это прерывание в GIMSC
 GIMSK|=_BV(INT0)|_BV(INT1); 

 // скорость передачи 9600 bod
 UBRRH=0;
 UBRRL=103;

 // инициализация UCSRA
 UCSRA=0x00;

 // D1 - выход USART
 UCSRB=0x00;
 UCSRB|=_BV(TXEN);

 // UCSRC: 8 бит данных; контроль четности;
 UCSRC=_BV(URSEL)|_BV(UPM1)|_BV(UCSZ1)|_BV(UCSZ0);

 // настройка Т/C0 (задержка передачи=10мс); N=1024; прерывание по переполнению; 
 TIMSK|=_BV(TOIE0);
 TCCR0|=_BV(CS02)|_BV(CS00);
 TCNT0=DELAY; 

 // инициализация константы сервы
 count_srv=9;

  // выключение аналового компаратора (на всякие)
 ACSR|=_BV(ACD);

 // общее разрешение прерываний
 sei();

 while(1)
 { 
   // считывание дискретных каналов
   word[3]=~PINB;


   // сброс в 0 положения srv
   if(!(PIND&(1<<PD6)))
   {
     while(!(PIND&(1<<PD6)));
     word[2]=9;
	 count_srv=9;
   }

   // cброс в 0 положения drv
   if(!(PIND&(1<<PD7)))
   {
     while(!(PIND&(1<<PD7)));
     word[1]=0x00;
	 count_drv=16;
   }
   
   // установка светодиодов max/min ДРАЙВЕР
   switch(count_drv)
   {
     case 0  : PORTC|=_BV(PC1); break;
	 case 32 : PORTC|=_BV(PC0); break;   
     default : PORTC&=~(_BV(PC1)|_BV(PC0));
   }
   
   // установка светодиодов max/min СЕРВА
   switch(count_srv)
   {
     case 0  : PORTC|=_BV(PC3); break;
	 case 18 : PORTC|=_BV(PC2); break;   
     default : PORTC&=~(_BV(PC3)|_BV(PC2));
   }

 }

 return 0;
}
