//----ПРИЕМНИК----//
#define  F_CPU 16000000UL
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define UP    4								// идентификаторы поворота ручки энкодера
#define DOWN  7								// вверх/вниз, 0 - посередине!
#define ICR_TOP 60      					// вершина ШИМ (15 значение в одну сторону)

#define OCR_MIN 14							// пороги для сервопривода
#define OCR_CEN 23							// вырабатывается Т/С2
#define OCR_MAX 32

volatile unsigned char start=1;				// флаг первого включения - для вочдога!
volatile unsigned char shift=0;             // направлениен драйвера
volatile unsigned char count=0;				// счетчик посылки
volatile unsigned char count_drv=64; 	    // текущее значение мотора
volatile unsigned char count_srv=9;	        // текущее значение сервы 
volatile unsigned char binary=0x00;  		// дискретные каналы  
volatile unsigned char signal=0;			// направление поворотника: 1-вправо; 2-влево;
volatile unsigned char counter=0;			// счетчик накрутки таймера T/C0
volatile unsigned char _backL=0;			// флаг стоп-сигналов: 1 - должен гореть; 0 - нет.
volatile unsigned char count_buf=0;

ISR(USART_RXC_vect)
{
 unsigned char status=0x00, data=0x00;

 // включение watchdog на 120 ms если первая посылка
 if(start)
 { 
   wdt_enable(WDTO_120MS);					
   start=0;
 }
    					
 // считывание ошибок
 status=UCSRA;
 if((status&(_BV(FE)|_BV(PE)|_BV(DOR))))
 {
  while(UCSRA&_BV(RXC)) data=UDR;
  return;
 }

 data=UDR;

 // ждем синхропакета
 if(count==0)
 { 
  // пришел синхропакет - изменяем счетчик приема, пинаем сторожевой таймер
  if(data==0xF0)
  {
    count++;
    wdt_reset();
  }
 }
 else
 {
  // если синхропакет не в такт - сброс счетчика
  if(data==0xF0) count=0;
  // иначе - преобразуем данные
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

   // стоп-сигнал
   PORTB&=~_BV(PB4);
   _backL=0;

   // поворотники
   if(signal!=0)
   if(signal==1)PORTD^=_BV(PD6);
   else PORTD^=_BV(PD7);
 }
}

int main(void)
{
 // выключение watchdog на случай сброса
 wdt_disable();

 // выводы OC1B/OC1A для ШИМ DRV; OC2 для SRV;PB0 - задний ход;
 DDRB=0x1F;
 PORTB=0xE0;

 // PortC - дискретка выход (кроме reset)
 DDRC=0x3F;
 PORTC=0x00;

 // PD6, PD7 - выход светодиодов; остальное - входа
 DDRD=0xC0;
 PORTD=0x3F;

 TCCR1A=0x00;
 TCCR1B=0x00;

 // Настройка ШИМ Driver: N=256; ржим=FastPWM; OC1A,OC1B  - неинверт.; Warning COM DESABLED!!!
 TCCR1B|=_BV(WGM13)|_BV(WGM12)|_BV(CS12);
 //TCCR1A|=_BV(WGM11)|_BV(COM1A1)|_BV(COM1B1);
 TCCR1A|=_BV(WGM11);

 // пороги срабатывания для таймера Т/С1
 ICR1=ICR_TOP;
 OCR1A=0x00;
 OCR1B=0x00;

 // выставление настроек Т/С2 - серва
 OCR2=OCR_CEN; 
 TCCR2=0x00;
 // Настройка FastPWM Servo; прямой; N=1024; F=Fcpu/256*N=61 Hz;
 TCCR2|=_BV(WGM21)|_BV(WGM20)|_BV(COM21)|_BV(CS22)|_BV(CS21)|_BV(CS20);

 // скорость передачи 9600 bod
 UBRRH=0;
 UBRRL=103;

 // инициализация UCSRA
 UCSRA=0x00;

 // D1 - выход USART; прерывание по приему;
 UCSRB=0x00;
 UCSRB|=_BV(RXEN)|_BV(RXCIE);

 // UCSRC: 8 бит данных; контроль четности;
 UCSRC=_BV(URSEL)|_BV(UPM1)|_BV(UCSZ1)|_BV(UCSZ0);

 // инициализация константы Servo
 count_srv=9;

 // выключение аналового компаратора (на всякие)
 ACSR|=_BV(ACD);

 // инициализация T/C0
 TCNT0=0;
 TIMSK|=_BV(TOIE0);
 TCCR0|=_BV(CS02)|_BV(CS00);

 // общее разрешение прерываний
 sei();

 while(1)
 { 
  // выставление сервопривода
  OCR2=OCR_MIN+count_srv;

  // работа с двигателем
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

  // к стоп-сигналам
  if(count_buf>count_drv)
  {
	_backL=1;
	PORTB|=_BV(PB4);
  }
  count_buf=count_drv;

  // выставление дискреток
  PORTC=binary;

  // зажигаем поворотники
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
