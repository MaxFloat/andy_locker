/* Name: main.c
 * Project: hid-data, example how to use HID for data transfer
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-11
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 */

/*
This example should run on most AVRs with only little changes. No special
hardware resources except INT0 are used. You may have to change usbconfig.h for
different I/O pins for USB. Please note that USB D+ must be the INT0 pin, or
at least be connected to INT0 as well.
*/

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */
#include <avr/eeprom.h>
#include <avr/interrupt.h> 
//#include <avr/mega32a.h>

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
#include "oddebug.h"        /* This is also an example for using debug macros */

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

//#define MY_CONFIG_TOTAL_LENGTH 34 //надо заменить на нормальное значение

PROGMEM const char usbDescriptorConfiguration[DESCRIPTOR_SIZE] = {
    0x09,                  // length of descriptor in bytes
    0x02,    // descriptor type
    DESCRIPTOR_SIZE, 0x00,
	0x01,         // bNumInterfaces: в конфигурации всего один интерфейс
    0x01,         // bConfigurationValue: индекс данной конфигурации
    0x00,         // iConfiguration: индекс строки, которая описывает эту конфигурацию
    0xE0,         // bmAttributes: признак того, что устройство будет питаться от шины USB
	0xee, //около ??? mA должно хватить
	/************** Дескриптор интерфейса ****************/
		0x09,         // bLength: размер дескриптора интерфейса
		0x04,	//тип дескриптора - интерфейс
		0x00,         // bInterfaceNumber: порядковый номер интерфейса - 0
        0x00,         // bAlternateSetting: признак альтернативного интерфейса, у нас не используется
        0x01,         // bNumEndpoints - количество эндпоинтов.

        0x03,         // bInterfaceClass: класс интерфеса - HID
        0x00,         // bInterfaceSubClass : подкласс интерфейса.
        0x00,         // nInterfaceProtocol : протокол интерфейса
        0,            // iInterface: индекс строки, описывающей интерфейс

                    // теперь отдельный дескриптор для уточнения того, что данный интерфейс - это HID устройство
                    /******************** HID дескриптор ********************/
                    0x09,         // bLength: длина HID-дескриптора
                    0x21, // bDescriptorType: тип дескриптора - HID
                    0x01, 0x01,   // bcdHID: номер версии HID 1.1
                    0x00,         // bCountryCode: код страны (если нужен)
                    0x01,         // bNumDescriptors: Сколько дальше будет report дескрипторов
                    0x22,         // bDescriptorType: Тип дескриптора - report
					0x16, 0x00, // wItemLength: длина report-дескриптора
                    //0x1f, 0x00, // wItemLength: длина report-дескриптора


                    /******************** дескриптор конечных точек (endpoints) ********************/
                    0x07,          // bLength: длина дескриптора
                    0x05, // тип дескриптора - endpoints
                    0x81,          // bEndpointAddress: адрес конечной точки и направление 1(IN)
                    0x03,          // bmAttributes: тип конечной точки - Interrupt endpoint
                    0x04, 0x00,    // wMaxPacketSize:  Bytes max
                    0x20          // bInterval: Polling Interval (32 ms)
};

PROGMEM const char usbHidReportDescriptor[22] = {   //  USB report descriptor 
    0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor defined page 1)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

/* Since we define only one feature report, we don't use report-IDs (which
 * would be the first byte of the report). The entire report consists of 128
 * opaque data bytes.
 */

/* The following variables store the status of the current data transfer */
static uchar    currentAddress;
static uchar    bytesRemaining;

static uchar cur_pin = 0;

static uchar cur_mode = 0;

void switchOnForASecond(uchar pin, int latency)
{
	//bingo
	PORTA |= pin; // Turn LED off, led2 - on
	cur_pin = pin;
	
	TCCR1A=0x00; //настройка таймера
	TCCR1B=0x05; //предделитель 1024
	TCNT1=0x00; //здесь увеличиваются тики
	OCR1A=latency;//0x2dc6; //записываем число в регистр сравнения
	//OCR1A=0x16e3;
	TIMSK=0x10; //запускаем таймер
}

/* ------------------------------------------------------------------------- */

/* usbFunctionRead() is called when the host requests a chunk of data from
 * the device. For more information see the documentation in usbdrv/usbdrv.h.
 */
USB_PUBLIC uchar usbFunctionRead(uchar *data, uchar len)
{
	uchar i = 0;
	uchar res = '0';
	
	if(cur_mode == 3)
		res = '2';
	else	
	if(!(PINA & (1 << PA1)) || cur_mode == 1)
	{
		res = '1';
		//button is pressed
		//switchOnForASecond(4, 0x5b8c);
		PORTA |= 4; // Turn led on
		cur_mode = 1; // pass waining mode
	}
	else
	{
		//button is NOT pressed
		res = '0';
	}
	
	for(i = 0; i!=len; i++)
	{
		data[i] = res;
	}
	
	bytesRemaining = 0;
    return len;
}

USB_PUBLIC void usbFunctionWriteOut(uchar *data, uchar len)
{
	
}

// Обработка прерывания по совпадению
//interrupt [TIM1_COMPA] void timer1_compa_isr(void)
//SIGNAL (SIG_TIMER1_COMPA)
SIGNAL( TIMER1_COMPA_vect )
{
   
   PORTA &= ~cur_pin; // Turn LED off
   cur_pin = 0;
   
   //TCNT1=0; //обнуляем таймер
   TCCR1A=0x00;
   TCCR1B=0x00; 
   TCNT1H=0x00;
   TCNT1L=0x00; 
   TIMSK=0x00; 
}

/* usbFunctionWrite() is called when the host sends a chunk of data to the
 * device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar   usbFunctionWrite(uchar *data, uchar len)
{
	if(cur_mode == 3)
		return 1; //заблокировано. Не открываем
	if(cur_mode == 1)
	{
		//режим ожидания нового пароля
		//записываем новый пароль в eeprom
		eeprom_write_block(data, (uchar *)0, len);
		
		cur_mode = 0;
		PORTA &= ~4;
		
		return 1;
	}
	//switchOnForASecond();
	
	// режим открывания
	//достаём пароль из eeprom
	//максимальная длина пароля - 8 символов
	uchar pass[8] = {0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33};
	
	len = 8;
	eeprom_read_block(pass, (uchar *)0, len);
	
	
	
	uchar b = 1;
	uchar i = 0;
	for(i = 0; i!=8; i++)
	{
		if(pass[i] == 0)
			break; //конец пароля
		
		if(data[i] != pass[i])
			b = 0;
	}
	
	if(b == 0) //Не совпало
	{
		cur_mode = 3; //блокируем до перезапуска
		return 1;
	}
	//открываем замок
	switchOnForASecond(1, 0x5b8c);
	
	return 1;
}

/* ------------------------------------------------------------------------- */
#define USB_GET_BUTTON_STATE 0x77
//static unsigned char replyBuf[16] = "000000000000";

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	//uchar i = 0;
	usbRequest_t    *rq = (void *)data;

//	return 0xff;
	
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
	{    /* HID class request */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* since we have only one report type, we can ignore the report-ID */
            bytesRemaining = 128;
            currentAddress = 0;
            return 0xff;  /* use usbFunctionRead() to obtain data */
        }else if(rq->bRequest == USBRQ_HID_SET_REPORT){
            /* since we have only one report type, we can ignore the report-ID */
            bytesRemaining = 128;
            currentAddress = 0;
            return 0xff;  /* use usbFunctionWrite() to receive data from host */
        }
    }
	else
		//if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR)
	{
		//будем работать как вендор
		return 0xff;
    }
	
    return 0xff;
}

/* ------------------------------------------------------------------------- */
ISR(INT1_vect)
{
	if ( PIND & PD3 )
	{
		//входим в режим ожидания нового пароля
		cur_mode = 1; //режим ожидания нового пароля
	}
}

int main(void)
{
uchar   i;
	// Function: Bit7=Out Bit6=Out Bit5=Out Bit4=Out Bit3=Out Bit2=Out Bit1=Out Bit0=Out 
	DDRA=(1<<DDA7) | (1<<DDA6) | (1<<DDA5) | (1<<DDA4) | (1<<DDA3) | (1<<DDA2) | (1<<DDA0);
	
	DDRA &= ~(1<<PA1); /* Set PA1 as input */
    	PORTA |= (1<<PA1); /* Activate PULL UP resistor */ 

	
    
	wdt_enable(WDTO_1S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
	cur_mode = 0; //режим открывания 
	
	TCCR1A=0x00; //настройка таймера
	TCCR1B=0x00;
	TCNT1=0x00; //здесь увеличиваются тики
	OCR1A=0x2dc6; //записываем число в регистр сравнения
    
	//switchOnForASecond(4, 0x16e3); // - diode

	odDebugInit();
    DBG1(0x00, 0, 0);       /* debug output: main starts */
    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    uchar pass[8] = {0x30, 0x31, 0x32, 0x33, 0x30, 0x31, 0x32, 0x33};
	eeprom_write_block(pass, (uchar *)0, 8);
	i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
	
	//в проектах V-USB нельзя использовать прерывания.
	
	DDRD &= ~(1<<PD3); /* Set PB0 as input */
    PORTD |= (1<<PD3); /* Activate PULL UP resistor */ 
    //разрешаем внешнее прерывание на int0
	GICR |= (1<<INT1);
	//настраиваем условие прерывания  
	MCUCR |= (0<<ISC01)|(1<<ISC00);
	
    sei();
    DBG1(0x01, 0, 0);       /* debug output: main loop starts */
    for(;;){                /* main event loop */
        DBG1(0x02, 0, 0);   /* debug output: main loop iterates */
        wdt_reset();
        usbPoll();
        
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
