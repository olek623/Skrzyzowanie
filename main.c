#include "stm32f0xx_tim.h"
#include "stm32f0xx_rcc.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_misc.h"
#include "stm32f0xx_usart.h"
#include <string.h>
#include <stdlib.h>

#define czerwone GPIOA
#define zolte GPIOB
#define zielone GPIOC

#define zieloneprzejscie GPIOC
#define czerwoneprzjecie GPIOB

#define SYG0 GPIO_Pin_0
#define SYG1 GPIO_Pin_1
#define SYG2 GPIO_Pin_2
#define SYG3 GPIO_Pin_3
#define SYG4 GPIO_Pin_4
#define SYG5 GPIO_Pin_5
#define SYG6 GPIO_Pin_6
#define SYG7 GPIO_Pin_7
#define SYG8 GPIO_Pin_8
#define SYG9 GPIO_Pin_9
#define SYG10 GPIO_Pin_10
#define SYG11 GPIO_Pin_11
#define SYG12 GPIO_Pin_12
#define SYG13 GPIO_Pin_13
#define SYG14 GPIO_Pin_14
#define SYG15 GPIO_Pin_15

//0-127
unsigned char buforRx[128]; //BUFOR ODBIORCZY
unsigned char buforTx[128]; //BUFOR NADAWCZY
unsigned char odebranedane[128]; //TABLICA DO KTOREJ TRAFIAJA DANE Z BUFORA ODBIORCZEGO
//0-255
volatile uint8_t IndexRxEnd=0; //LICZNIK SLUZACY DO ODBIERANIA DANYCH W PRZERWANIU
volatile uint8_t IndexRxStart=0; //LICZNIK PRZEPISUJACY POBRANE DANE W FUNKCJI ODBIOR
volatile uint8_t licznikodbioru; //LICZNIK PRZEPISUJACY DANE DO ODEBRANEDANE

volatile uint8_t IndexTxStart=0; //LICZNIK UZYWANY PRZY WYSYLANIU DANYCH W PRZERWANIU
volatile uint8_t IndexTxEnd=0; //LICZNIK PRZEPISUJACY DANE DO BUFORA NADAWCZEGO

//CZASY SWIATEL
volatile uint8_t czaszolte=2;
volatile uint8_t czaszielone=5;

//CZASY TYMCZASOWE-POMAGAJA W ZMIENIE CZASU W ODPOWIEDNIM MOMENCIE
volatile uint8_t tempzolte=2;
volatile uint8_t tempzielone=5;

//LICZNIK TIMERA-CZASU
volatile uint8_t timerValue=0;

typedef enum {TRUE = 1, FALSE = 0} bool;

volatile bool a=FALSE,b=FALSE,c=FALSE,d=FALSE; //zmienne od stanu przyciskow

//ZMIENNE, KTORE INFORMUJ¥ KTÓRE PRZEJSCIA MOGA ZOSTAC URUCHOMIONE
bool sprA=FALSE;
bool sprB=FALSE;

//PRZE£¥CZANIE TRYBÓW
bool automanual=TRUE;

//ZMIENIE ODPOWIADAJACE ZA DOPROWADZENIE CYKLU DO KONCA
bool dalej1=FALSE;
bool dalej2=FALSE;

//ZMIENNA BLOKUJ¥CA ZMIANE SYGNALIZATORA W TRYBIE MANUALNYM W CZASIE GDY NASTEPUJE PRZELACZANIE
bool zmiana=FALSE;

//ZMIENNE PRZECHOWYWUJ¥CE WYBRANY SYGNALIZATOR ORAZ POPRZEDNI
char dowylaczenia;
char dowlaczenia;

//DEKLARACJA PROTOKOLU KOMUNIKACYJNEGO USART
//PORT A-PIN2-NADAWANIE TX
//PORT A-PIN3-ODBIERANIE RX
void USART2FUNC()
{
	  GPIO_InitTypeDef GPIO_InitStructure;

	  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	  GPIO_Init(GPIOA, &GPIO_InitStructure);

	  USART_InitTypeDef USART_InitStructure;
	  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);

	  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);
	  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);

	  USART_InitStructure.USART_BaudRate = 115200;  //PREDKOSC
	  USART_InitStructure.USART_WordLength = USART_WordLength_8b; //DLUGOSC SLOWA
	  USART_InitStructure.USART_StopBits = USART_StopBits_1; //BIT STOPU
	  USART_InitStructure.USART_Parity = USART_Parity_No; //SPOSOB KONTROLI PARZYSTOSCI-BRAK
	  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //OKRESLA SPRZETOWA KONTROLE PRZEPLYWU
	  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	  USART_Init(USART2, &USART_InitStructure);
	  USART_Cmd(USART2,ENABLE);

	  USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	  NVIC_EnableIRQ(USART2_IRQn);
}

//PRZERWANIA POCHODZACE Z USARTA
//ODBIERANIE DANYCH- PRZERWANIE NASTEPUJE, GDY W BUFORZE ODBIORCZYM ZNAJDUJA SIE DANE
//NADAWANIE DANYCH- NASTEPUJE W MOMENCIE, GDY W BUFORZE NADAWCZYM JEST PUSTO
void USART2_IRQHandler(void)
{
	//ODBIOR
	if (USART_GetITStatus(USART2, USART_IT_RXNE)!=RESET)
	{
		buforRx[IndexRxEnd]= USART_ReceiveData(USART2);
		IndexRxEnd++;
		if (IndexRxEnd>=128)
		{
			IndexRxEnd=0;
		}
	}

	//NADAWANIE
	if(USART_GetITStatus(USART2, USART_IT_TXE) != RESET)
	{
			USART_SendData(USART2, buforTx[IndexTxStart++]);
			if (IndexTxStart>=128)
			{
				IndexTxStart = 0;
			}

		if (IndexTxEnd==IndexTxStart)
		{
			//WYLACZENIE PRZERWANIA NADAWANIA
			USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
		}
    }
}

//FUNKCJA PRZEPISUJACA PODANY TEKST DO BUFORA NADAWCZEGO
void wysylanie(char *data)
{
	//OCZEKIWANIE AZ POPRZEDNIE DANE ZOSTANA CALKOWICIE WYSLANE
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET) {  }

	uint8_t z=0;

	while (z<strlen(data))
	{
		buforTx[IndexTxEnd]=data[z];
		IndexTxEnd++;
		z++;
		if (IndexTxEnd>=128)
		{
			IndexTxEnd=0;
		}
	}

	//WLACZENIE PRZERWANIA OD NADAWANIA
	USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

//bool spr=FALSE;
//FUNKCJA ODBIERAJACA DANE I ZAPISUJACA KOMENDY
void odbior()
{
	//ODBIERANIE ODBYWA SIE W MOMENCIE ROZNYCH ZNACZNIKOW
	//ZNACZNIK END PRZESUWA SIE W PRZERWANIU PODCZAS ODBIORU
	//ZNACZNIKI OKRESLAJA DLUGOSC ODEBRANYCH DANYCH
	if (IndexRxEnd != IndexRxStart)
	{

		if (buforRx[IndexRxStart]!=';')
		{
		//PRZEPISYWANIE DANYCH
		odebranedane[licznikodbioru]=buforRx[IndexRxStart];
		licznikodbioru++;
		}
		else
		{
			//KONIEC KOMENDY
			licznikodbioru=0;
			komendy();
		}
		IndexRxStart++;
		if (IndexRxStart>=128)
		{
			IndexRxStart=0;
		}
	}
	/*
	if (IndexRxEnd != IndexRxStart)
	{

			if (buforRx[IndexRxStart]=='[')
			{
				licznikodbioru=0;
				memset(odebranedane,0,strlen(odebranedane));
				spr=TRUE;

			}
			else if (buforRx[IndexRxStart]==']')
			{
				spr=FALSE;
				licznikodbioru=0;
				komendy();
			}
			else if (spr==TRUE)
			{
				//Przepisywanie odebranych danych
				odebranedane[licznikodbioru]=buforRx[IndexRxStart];
				licznikodbioru++;
			}
			IndexRxStart++;
			if (IndexRxStart>=128)
			{
				IndexRxStart=0;
			}
		}
	*/
}

//DEKLARACJA LICZNIKA TIMER1 KANAL1
//LICZENIE W GORE CO 1 SEKUNDE
void TIM()
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    TIM_TimeBaseInitTypeDef timerInitStructure;  // 48 000 000/1000=48 000 //48 000 000/48 000=1000
    timerInitStructure.TIM_Prescaler = 48000-1; //ZEGAR TIMERA WYNOSI 48Mhz/ -1 TABLICOWANIE OD 0
    timerInitStructure.TIM_CounterMode = TIM_CounterMode_Up; //LICZENIE W GORE
    timerInitStructure.TIM_Period = 1000-1;  //DLA 1 SEKUNDY
    timerInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    timerInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &timerInitStructure);

    TIM_ITConfig(TIM1, TIM_IT_CC1, ENABLE); //TIMER 1 KANAL  1
    TIM_Cmd(TIM1, ENABLE);
}

//PRZERWANIE OD TIMERA
//NASTEPUJE W WYNIKU OSI¥GNIÊCIA PERIOD - CO 1 SEKUNDA
void TIM1_CC_IRQHandler()
{
    if (TIM_GetITStatus(TIM1, TIM_IT_CC1) == SET)
    {
        timerValue++;
        if (timerValue>=((8*czaszolte)+(4*czaszielone)))
        {
        	timerValue=0;
        	czaszielone=tempzielone;
        	czaszolte=tempzolte;
        }

        TIM_ClearITPendingBit(TIM1, TIM_IT_CC1);
    }
}

//DEKLARACJA NVIC DLA TIMERA
void TIMNVIC(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  NVIC_InitStructure.NVIC_IRQChannel =  TIM1_CC_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

//DEKLARACJA DIOD NA PORCIE PC
void LEDPC()
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);
	GPIO_InitTypeDef MGPIO;
	MGPIO.GPIO_Speed = GPIO_Speed_50MHz;
	MGPIO.GPIO_Pin=   GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |GPIO_Pin_3 |GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15 ;
	MGPIO.GPIO_Mode = GPIO_Mode_OUT; //tryb wyjscia
	MGPIO.GPIO_OType = GPIO_OType_PP;; //wyjscie alternatywne push-pull
	GPIO_Init(GPIOC,&MGPIO);
}

//DEKLARACJA DIOD NA PORCIE PA
void LEDPA()
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	GPIO_InitTypeDef MGPIO;
	MGPIO.GPIO_Speed = GPIO_Speed_50MHz;
	MGPIO.GPIO_Pin= GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15 ;
	MGPIO.GPIO_Mode = GPIO_Mode_OUT;
	MGPIO.GPIO_OType = GPIO_OType_PP;;
	GPIO_Init(GPIOA,&MGPIO);
}

//DEKLARACJA DIOD NA PORCIE PB
void LEDPB()
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
	GPIO_InitTypeDef MGPIO;
	MGPIO.GPIO_Speed = GPIO_Speed_50MHz;
	MGPIO.GPIO_Pin= GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |GPIO_Pin_3 |GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12| GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15 ;
	MGPIO.GPIO_Mode = GPIO_Mode_OUT;
	MGPIO.GPIO_OType = GPIO_OType_PP;;
	GPIO_Init(GPIOB,&MGPIO);
}

//FUNCKJA WLACZAJACA I WYLACZAJACA DANY SYGNALIZATOR
void SYGNALIZATOR(bool a, uint16_t numer,GPIO_TypeDef* kolor)
{
	switch (a)
	{
		case 1:
		{
			GPIO_SetBits(kolor, numer);
			break;
		}

		case 0:
		{
			GPIO_ResetBits(kolor, numer);
			break;
		}
	}
}


//DEKLARACJA PRZYCISKOW
void PRZYCISKINIT()
{
	GPIO_InitTypeDef MGPIO;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE);
	MGPIO.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	MGPIO.GPIO_Mode = GPIO_Mode_IN; //TRYB WEJSCIA
	MGPIO.GPIO_OType = GPIO_OType_PP;
	MGPIO.GPIO_Speed = GPIO_Speed_50MHz;
	MGPIO.GPIO_PuPd = GPIO_PuPd_DOWN; //PODPIECIE DO MASY
	GPIO_Init(GPIOF, &MGPIO);

}

//FUNCJA ODCZYTUJACA STANY PRZYCISKOW
void PRZYCISKFUNC()
{
	if(GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_4))
	{
		a=TRUE; //SYG0
	}
	if(GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_5))
	{
		b=TRUE; //SYG1
	}
	if(GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_6))
	{
		c=TRUE; //SYG2
	}
	if(GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_7))
	{
		d=TRUE; ///SYG3
	}
}

//FUNKCJA URUCHAMIAJACA SYGNALIZACJE DLA PIESZYCH
//ZAPALA ZIELONE W MOMENCIE GDY ZOSTAL NADUSZONY PRZYCISK
//GDY JEST TO MOZLIWE-BEZPIECZNE
void PRZEJSCIE()
{
	if (sprB==TRUE)
	    	{
	    		if (a==TRUE)
	    		{
	    			SYGNALIZATOR(1,SYG0,zieloneprzejscie);
	    			SYGNALIZATOR(0,SYG0,czerwoneprzjecie);

	    		}
	    		if (b==TRUE)
	    		{
	    			SYGNALIZATOR(1,SYG1,zieloneprzejscie);
	    			SYGNALIZATOR(0,SYG1,czerwoneprzjecie);

	    		}
	    	}
	    	else
	    	{
	    		SYGNALIZATOR(1,SYG0,czerwoneprzjecie);
	    		SYGNALIZATOR(1,SYG1,czerwoneprzjecie);
	    		SYGNALIZATOR(0,SYG1,zieloneprzejscie);
	    		SYGNALIZATOR(0,SYG0,zieloneprzejscie);
			}

	    	if (sprA==TRUE)
	    	{
	    		if (c==TRUE)
	    		{
	    			SYGNALIZATOR(1,SYG2,zieloneprzejscie);
	    			SYGNALIZATOR(0,SYG2,czerwoneprzjecie);

	    		}
	    		if (d==TRUE)
	    		{
	    			SYGNALIZATOR(1,SYG3,zieloneprzejscie);
	    			SYGNALIZATOR(0,SYG3,czerwoneprzjecie);
	    		}

	    	}
	    	else
	    	{
	    		SYGNALIZATOR(1,SYG2,czerwoneprzjecie);
	    		SYGNALIZATOR(1,SYG3,czerwoneprzjecie);
	    		SYGNALIZATOR(0,SYG3,zieloneprzejscie);
	    		SYGNALIZATOR(0,SYG2,zieloneprzejscie);
	    	}

}

//FUNKCJA USTAWIAJACA POCZATKOWY STAN SYGNALIZACJI
//WSZYTSKIE SYGNALIZATORY WSKAZUJA STOP
//WSZYTSKIE STANY SA FALSE
void CZERWONEALL()
{

	SYGNALIZATOR(1,SYG4,czerwone);
	SYGNALIZATOR(1,SYG5,czerwone);
	SYGNALIZATOR(1,SYG6,czerwone);
	SYGNALIZATOR(1,SYG7,czerwone);
	SYGNALIZATOR(1,SYG8,czerwone);
	SYGNALIZATOR(1,SYG9,czerwone);
	SYGNALIZATOR(1,SYG10,czerwone);
	SYGNALIZATOR(1,SYG11,czerwone);
	SYGNALIZATOR(1,SYG12,czerwone);
	SYGNALIZATOR(1,SYG13,czerwone);
	SYGNALIZATOR(1,SYG14,czerwone);
	SYGNALIZATOR(1,SYG15,czerwone);

	SYGNALIZATOR(0,SYG4,zielone);
	SYGNALIZATOR(0,SYG5,zielone);
	SYGNALIZATOR(0,SYG6,zielone);
	SYGNALIZATOR(0,SYG7,zielone);
	SYGNALIZATOR(0,SYG8,zielone);
	SYGNALIZATOR(0,SYG9,zielone);
	SYGNALIZATOR(0,SYG10,zielone);
	SYGNALIZATOR(0,SYG11,zielone);
	SYGNALIZATOR(0,SYG12,zielone);
	SYGNALIZATOR(0,SYG13,zielone);
	SYGNALIZATOR(0,SYG14,zielone);
	SYGNALIZATOR(0,SYG15,zielone);

	SYGNALIZATOR(0,SYG4,zolte);
	SYGNALIZATOR(0,SYG5,zolte);
	SYGNALIZATOR(0,SYG6,zolte);
	SYGNALIZATOR(0,SYG7,zolte);
	SYGNALIZATOR(0,SYG8,zolte);
	SYGNALIZATOR(0,SYG9,zolte);
	SYGNALIZATOR(0,SYG10,zolte);
	SYGNALIZATOR(0,SYG11,zolte);
	SYGNALIZATOR(0,SYG12,zolte);
	SYGNALIZATOR(0,SYG13,zolte);
	SYGNALIZATOR(0,SYG14,zolte);
	SYGNALIZATOR(0,SYG15,zolte);

	SYGNALIZATOR(1,SYG0,czerwoneprzjecie);
	SYGNALIZATOR(1,SYG1,czerwoneprzjecie);
	SYGNALIZATOR(1,SYG2,czerwoneprzjecie);
	SYGNALIZATOR(1,SYG3,czerwoneprzjecie);

	SYGNALIZATOR(0,SYG0,zieloneprzejscie);
	SYGNALIZATOR(0,SYG1,zieloneprzejscie);
	SYGNALIZATOR(0,SYG2,zieloneprzejscie);
	SYGNALIZATOR(0,SYG3,zieloneprzejscie);

	a=FALSE; b=FALSE; c=FALSE; d=FALSE;
	sprA=FALSE; sprB=FALSE;
}

//FUNKCJA POWODUJACA ZMIANE
//DO ZMIENNEJ 'DOWLACZENIA' PRZYPISYWANY JEST ZNAK SYGNALIZATORA, KTORY MA ZOSTAC URUCHOMIONY
//ZMIANAN JEST MOZLIWA W TRYBIE MANUALNYM ORAZ GDY NIE TRWA PRZELACZANIE SYGNALIZATOROW
void uruchamianie(char znak)
{
	if (automanual==FALSE)
		{
			if (zmiana==TRUE)
			{
				if (znak==dowylaczenia)
				{
					wysylanie("WYBRALES URUCHOMIONY SYGNALIZATOR \r\n");
				}
				else
				{
					TIM_ITConfig(TIM1, TIM_IT_CC1, ENABLE);
				///////////////////////////////////////////////
					wysylanie("URUCHOMIONO \r\n");
					dowlaczenia=znak;
				}
			}
			else
			{
				wysylanie("NIE MOZESZ W TEJ CHWILI DOKONAC ZMIANY-TRWA PRZELACZANIE  \r\n");
			}
		}
		else
		{
			wysylanie("OPCJA DOSTEPNA TYLKO W TRYBIE MANUALNYM \r\n");
		}
}

//PARSOWANIE DANYCH
void komendy()
{
		//ZMIANA CZASOW PRACY SYGNALIZACJI
		if (strncmp(odebranedane,"CZASZIELONE=",12)==0)
		{
			if (automanual==FALSE)
			{
				wysylanie("OPCJA DOSTEPNA TYLKO W TRYBIE AUTOMATYCZNYM \r\n");
			}
			else
			{

				wysylanie("USTAWIAM NASTEPUJ¥CE WARTOSCI: \r\n");
				wysylanie(odebranedane);
				wysylanie("\r\n");

				uint8_t start=0;
				tempzielone=0;

				while (start<strlen(odebranedane)-12)
				{
					tempzielone=((tempzielone*10)+(odebranedane[start+12]-48)); //-48 wedlug tablicy ascii 0 ma nr 48
					start++;
				}
			}
		}
		else if (strncmp(odebranedane,"CZASZOLTE=",10)==0)
		{
			if ((automanual==TRUE) || (automanual==FALSE && zmiana==TRUE))
			{
				wysylanie("USTAWIAM NASTEPUJ¥CE WARTOSCI: \r\n");
				wysylanie(odebranedane);
				wysylanie("\r\n");

				uint8_t start=0;
				tempzolte=0;

				while (start<strlen(odebranedane)-10)
				{
					tempzolte=((tempzolte*10)+(odebranedane[start+10]-48));
					start++;
				}
			}
			else
			{
				wysylanie("NIE MOZESZ W TEJ CHWILI DOKONAC ZMIANY-TRWA PRZELACZANIE  \r\n");
			}

			if(automanual==FALSE && zmiana==TRUE)
			{
				czaszolte=tempzolte;
			}

		}

		//WLACZENIE TRYBU AUTOMATYCZNEGO
		else if (strncmp(odebranedane,"AUTO",4)==0)
		{

			if (automanual==FALSE)
			{
				if (zmiana==TRUE)
				{
					dowlaczenia='Z';
					TIM_ITConfig(TIM1, TIM_IT_CC1, ENABLE);
					wysylanie("TRYB AUTO WLACZONY \r\n");
				}
				else
				{
					wysylanie("NIE MOZESZ W TEJ CHWILI DOKONAC ZMIANY-TRWA PRZELACZANIE  \r\n");
				}

			}
			else
			{
				wysylanie("SYGNALIZACJA JUZ DZIALA W TRYBIE AUTOMATYCZNYM \r\n");
			}

		}

		//WLACZENIE TRYBU MANUALNEGO
		else if (strncmp(odebranedane,"MANUAL",6)==0)
		{
			if (automanual==TRUE)
			{
				dowlaczenia='G';
				zmiana=TRUE;
				automanual=FALSE;
				wysylanie("TRYB MANUAL WLACZONY \r\n");
			}
			else
			{
				wysylanie("SYGNALIZACJA JUZ DZIALA W TRYBIE MANUALNYM  \r\n");
			}

		}
		//MANUALNE WLACZANIE SYGNALIZATOROW
		else if (strncmp(odebranedane,"A-WLACZ",7)==0)
		{
			uruchamianie('A');
		}
		else if (strncmp(odebranedane,"B-WLACZ",7)==0)
		{
			uruchamianie('B');
		}
		else if (strncmp(odebranedane,"C-WLACZ",7)==0)
		{
			uruchamianie('C');
		}
		else if (strncmp(odebranedane,"D-WLACZ",7)==0)
		{
			uruchamianie('D');
		}

		//WYSIWETLENIE AKTUALNYCH CZASOW Z JAKIMI PRACUJE SYGNALIZACJA
		else if (strncmp(odebranedane,"AKTUALNYCZAS",12)==0)
		{
			wysylanie("CZAS PALENIA SIÊ ZIELONEGO: ");
			char snum[5];
			itoa(czaszielone, snum, 10);
			wysylanie(snum);
			wysylanie(" SEKUND \r\n");

			wysylanie("CZAS PALENIA SIÊ ¯ÓLTEGO: ");
			char snumM[5];
			itoa(czaszolte, snumM, 10);
			wysylanie(snumM);
			wysylanie(" SEKUND \r\n");
		}

		//WLACZENIE SYGNALIZACJI DLA PIESZYCH (SYMULACJA NACISNIECIA PRZYCISKU)
		else if (strncmp(odebranedane, "A=1",3)==0)
		{
			if (automanual==FALSE)
			{
				wysylanie("OPCJA DOSTEPNA TYLKO W TRYBIE AUTOMATYCZNYM \r\n");
			}
			else
			{
				a=TRUE;
				wysylanie("SYGNALIZACJA 'A' ZOSTANIE URUCHOMIONA \r\n");
			}
		}

		else if (strncmp(odebranedane, "B=1",3)==0)
		{
			if (automanual==FALSE)
			{
				wysylanie("OPCJA DOSTEPNA TYLKO W TRYBIE AUTOMATYCZNYM \r\n");
			}
			else
			{
				b=TRUE;
				wysylanie("SYGNALIZACJA 'B' ZOSTANIE URUCHOMIONA \r\n");
			}
		}
		else if (strncmp(odebranedane, "C=1",3)==0)
		{
			if (automanual==FALSE)
			{
				wysylanie("OPCJA DOSTEPNA TYLKO W TRYBIE AUTOMATYCZNYM \r\n");
			}
			else
			{
				c=TRUE;
				wysylanie("SYGNALIZACJA 'C' ZOSTANIE URUCHOMIONA \r\n");
			}
		}
		else if (strncmp(odebranedane, "D=1",3)==0)
		{
		if (automanual==FALSE)
			{
				wysylanie("OPCJA DOSTEPNA TYLKO W TRYBIE AUTOMATYCZNYM \r\n");
			}
			else
			{
				d=TRUE;
				wysylanie("SYGNALIZACJA 'D' ZOSTANIE URUCHOMIONA \r\n");
			}
		}

		//W PRZYPADKU WPROWADZENIE BLEDNYCH DANYCH
		else
		{
			wysylanie("B£ÊDNIE WPROWADZONE DANE \r\n");
		}

		//CZYSZCZENIE TABLICY ODEBRANEDANE
		memset(odebranedane,0,strlen(odebranedane));

}

//FUNKCJA WYLACZAJACA SYGNALIZATOR W TRYBIE MANUALNYM
void ZGAS(uint16_t a,uint16_t b,uint16_t c)
{
	if (timerValue==czaszolte)
	{
		SYGNALIZATOR(1,a,zolte);
		SYGNALIZATOR(1,b,zolte);
		SYGNALIZATOR(1,c,zolte);

		SYGNALIZATOR(0,a,zielone);
		SYGNALIZATOR(0,b,zielone);
		SYGNALIZATOR(0,c,zielone);
	}
	if (timerValue==2*czaszolte)
	{
		SYGNALIZATOR(0,a,zolte);
		SYGNALIZATOR(0,b,zolte);
		SYGNALIZATOR(0,c,zolte);

		SYGNALIZATOR(1,a,czerwone);
		SYGNALIZATOR(1,b,czerwone);
		SYGNALIZATOR(1,c,czerwone);
	}

}

//FUNKCJA ZAPALAJACA SYGNALIZATOR W TRYBIE MANUALNYM
void ZAPAL(uint16_t a,uint16_t b,uint16_t c, char znak)
{
	//WYLACZENIE SYGNALIZATOROW PRZEJSCIA DLA PIESZYCH
	if (timerValue==2*czaszolte)
	{
		if (znak=='A' || znak=='B')
			{
				sprB=FALSE;
			}
			else if (znak=='C' || znak=='D')
			{
				sprA=FALSE;
			}

	}

	if (timerValue==3*czaszolte)
	{
		SYGNALIZATOR(1,a,zolte);
		SYGNALIZATOR(1,b,zolte);
		SYGNALIZATOR(1,c,zolte);
	}
	if (timerValue==4*czaszolte)
	{
		SYGNALIZATOR(0,a,czerwone);
		SYGNALIZATOR(0,b,czerwone);
		SYGNALIZATOR(0,c,czerwone);

		SYGNALIZATOR(0,a,zolte);
		SYGNALIZATOR(0,b,zolte);
		SYGNALIZATOR(0,c,zolte);

		SYGNALIZATOR(1,a,zielone);
		SYGNALIZATOR(1,b,zielone);
		SYGNALIZATOR(1,c,zielone);

		dowylaczenia=znak;

		//ZAPALENIE SYGNALIZATOROW OD PRZEJSCIA DLA PIESZYCH
		if (znak=='A' || znak=='B')
		{
			sprA=TRUE;
		}
		else if (znak=='C' || znak=='D')
		{
			sprB=TRUE;
		}
	}
}

//FUNKCJA GLOWNA MAIN
int main()
{
	LEDPC();
	LEDPA();
	LEDPB();
	PRZYCISKINIT();
	CZERWONEALL();
	USART2FUNC();
	TIM();
	TIMNVIC();

	//GLOWNE CIALO PROGRAMU
    while(1)
    {
    	PRZYCISKFUNC();
    	odbior();
    	PRZEJSCIE();

    	//TRYB AUTOMATYCZNY
    	if (automanual==TRUE || dalej1==TRUE || dalej2==TRUE)
    	{

			if (timerValue == 0)
			{
				//z poprzedniego cyklu
				//zaplaenie czerwonego
				SYGNALIZATOR(1,SYG12,czerwone);
				SYGNALIZATOR(1,SYG15,czerwone);

				//zgaszenie zoltego
				SYGNALIZATOR(0,SYG12,zolte);
				SYGNALIZATOR(0,SYG15,zolte);

				dalej2=FALSE;
				//////////////////////////////////////////////////////////////////////
				if (automanual!=FALSE)
				{
					dalej1=TRUE;
					SYGNALIZATOR(1,SYG4,zolte);
					SYGNALIZATOR(1,SYG5,zolte);
					SYGNALIZATOR(1,SYG7,zolte);
					SYGNALIZATOR(1,SYG8,zolte);
				}
			}

			if (timerValue == czaszolte)
			{
				sprA=TRUE;
				//zgaszenie czerwonego prosto
				SYGNALIZATOR(0,SYG4,czerwone);
				SYGNALIZATOR(0,SYG5,czerwone);
				SYGNALIZATOR(0,SYG7,czerwone);
				SYGNALIZATOR(0,SYG8,czerwone);

				//zgaszenie zó³tego prosto
				SYGNALIZATOR(0,SYG4,zolte);
				SYGNALIZATOR(0,SYG5,zolte);
				SYGNALIZATOR(0,SYG7,zolte);
				SYGNALIZATOR(0,SYG8,zolte);

				// zapalenie zielonego prosto
				SYGNALIZATOR(1,SYG4,zielone);
				SYGNALIZATOR(1,SYG5,zielone);
				SYGNALIZATOR(1,SYG7,zielone);
				SYGNALIZATOR(1,SYG8,zielone);
			}
			//jad¹ prosto i w prawo
			if (timerValue==(czaszolte+czaszielone))
			{
				//zgaszenie zielonego prosto
				SYGNALIZATOR(0,SYG4,zielone);
				SYGNALIZATOR(0,SYG5,zielone);
				SYGNALIZATOR(0,SYG7,zielone);
				SYGNALIZATOR(0,SYG8,zielone);

				//zapalenie zó³tego prosto
				SYGNALIZATOR(1,SYG4,zolte);
				SYGNALIZATOR(1,SYG5,zolte);
				SYGNALIZATOR(1,SYG7,zolte);
				SYGNALIZATOR(1,SYG8,zolte);

			}

			if (timerValue==(2*czaszolte+czaszielone))
			{
				//zgaszenie ¿ó³tego prosto
				SYGNALIZATOR(0,SYG4,zolte);
				SYGNALIZATOR(0,SYG5,zolte);
				SYGNALIZATOR(0,SYG7,zolte);
				SYGNALIZATOR(0,SYG8,zolte);

				//zpalenie czerwonego prosto
				SYGNALIZATOR(1,SYG4,czerwone);
				SYGNALIZATOR(1,SYG5,czerwone);
				SYGNALIZATOR(1,SYG7,czerwone);
				SYGNALIZATOR(1,SYG8,czerwone);

				dalej1=FALSE;
				if (automanual!=FALSE)
				{
					dalej2=TRUE;
					//zapalnie ¿ó³tego w lewo
					SYGNALIZATOR(1,SYG6,zolte);
					SYGNALIZATOR(1,SYG9,zolte);
				}
			}


			if (timerValue==(3*czaszolte+czaszielone))
			{
				//zgaszenie ¿ó³tego i czerwonego lewo
				SYGNALIZATOR(0,SYG6,zolte);
				SYGNALIZATOR(0,SYG9,zolte);
				SYGNALIZATOR(0,SYG6,czerwone);
				SYGNALIZATOR(0,SYG9,czerwone);
				//zapalenie zielonego w lewo
				SYGNALIZATOR(1,SYG6,zielone);
				SYGNALIZATOR(1,SYG9,zielone);

			}

			if (timerValue==(3*czaszolte+2*czaszielone))
			{
				//zgaszenie zielonego
				SYGNALIZATOR(0,SYG6,zielone);
				SYGNALIZATOR(0,SYG9,zielone);
				//zapalnie ¿ó³tego
				SYGNALIZATOR(1,SYG6,zolte);
				SYGNALIZATOR(1,SYG9,zolte);
				sprA=FALSE;
				c=FALSE;
				d=FALSE;

			}

			if (timerValue==(4*czaszolte+2*czaszielone))
			{

				/////////////////////////////////////////////////////////////////////////////////////
				//zgaszenie zó³tego dla lewo
				SYGNALIZATOR(0,SYG6,zolte);
				SYGNALIZATOR(0,SYG9,zolte);
				// zapalenie czerwonego dla lewo
				SYGNALIZATOR(1,SYG6,czerwone);
				SYGNALIZATOR(1,SYG9,czerwone);

				dalej2=FALSE;
				//¿ó³te dla prosto
				if (automanual!=FALSE)
				{
					dalej1=TRUE;
					SYGNALIZATOR(1,SYG10,zolte);
					SYGNALIZATOR(1,SYG11,zolte);
					SYGNALIZATOR(1,SYG13,zolte);
					SYGNALIZATOR(1,SYG14,zolte);
				}
			}

			if (timerValue==(5*czaszolte+2*czaszielone))
			{
				sprB=TRUE;
				//zgaszenie zó³tego prosto
				SYGNALIZATOR(0,SYG10,zolte);
				SYGNALIZATOR(0,SYG11,zolte);
				SYGNALIZATOR(0,SYG13,zolte);
				SYGNALIZATOR(0,SYG14,zolte);

				//zgaszenie czerwonego prosto
				SYGNALIZATOR(0,SYG10,czerwone);
				SYGNALIZATOR(0,SYG11,czerwone);
				SYGNALIZATOR(0,SYG13,czerwone);
				SYGNALIZATOR(0,SYG14,czerwone);

				// zapalenie zielonego prosto
				SYGNALIZATOR(1,SYG10,zielone);
				SYGNALIZATOR(1,SYG11,zielone);
				SYGNALIZATOR(1,SYG13,zielone);
				SYGNALIZATOR(1,SYG14,zielone);
			}

			if (timerValue==(5*czaszolte+3*czaszielone))
			{
				//zgaszenie zielonego prosto
				SYGNALIZATOR(0,SYG10,zielone);
				SYGNALIZATOR(0,SYG11,zielone);
				SYGNALIZATOR(0,SYG13,zielone);
				SYGNALIZATOR(0,SYG14,zielone);
				//zapalenie ¿ó³tego prosto
				SYGNALIZATOR(1,SYG10,zolte);
				SYGNALIZATOR(1,SYG11,zolte);
				SYGNALIZATOR(1,SYG13,zolte);
				SYGNALIZATOR(1,SYG14,zolte);

			}
			if (timerValue==(6*czaszolte+3*czaszielone))
			{
				//zgaszenie ¿ó³tego prosto
				SYGNALIZATOR(0,SYG10,zolte);
				SYGNALIZATOR(0,SYG11,zolte);
				SYGNALIZATOR(0,SYG13,zolte);
				SYGNALIZATOR(0,SYG14,zolte);

				//zpalenie czerwonego prosto
				SYGNALIZATOR(1,SYG10,czerwone);
				SYGNALIZATOR(1,SYG11,czerwone);
				SYGNALIZATOR(1,SYG13,czerwone);
				SYGNALIZATOR(1,SYG14,czerwone);

				dalej1=FALSE;
				if (automanual!=FALSE)
				{
					dalej2=TRUE;
					//zapalnie ¿ó³tego w lewo
					SYGNALIZATOR(1,SYG12,zolte);
					SYGNALIZATOR(1,SYG15,zolte);
				}
			}

			if (timerValue==(7*czaszolte+3*czaszielone))
			{
				//zgaszenie ¿ó³tego i czerwonego lewo
				SYGNALIZATOR(0,SYG12,zolte);
				SYGNALIZATOR(0,SYG15,zolte);

				SYGNALIZATOR(0,SYG12,czerwone);
				SYGNALIZATOR(0,SYG15,czerwone);

				//zapalenie zielonego lewo
				SYGNALIZATOR(1,SYG12,zielone);
				SYGNALIZATOR(1,SYG15,zielone);
			}

			if (timerValue==(7*czaszolte+4*czaszielone))
			{
				//zgaszenie zielonego
				SYGNALIZATOR(0,SYG12,zielone);
				SYGNALIZATOR(0,SYG15,zielone);

				//zapalenie zoltego
				SYGNALIZATOR(1,SYG12,zolte);
				SYGNALIZATOR(1,SYG15,zolte);
				sprB=FALSE;
				a=FALSE;
				b=FALSE;
			}
		//8*czaszolte+4*czaszielone
    	}
    	//TRYB MANUALNY
    	else
    	{
    		if (dowlaczenia!=dowylaczenia)
    		{
    			//GASZENIE
				if (dowylaczenia=='A')
				{
					ZGAS(SYG4,SYG5,SYG6);
				}
				else if (dowylaczenia=='B')
				{
					ZGAS(SYG7,SYG8,SYG9);
				}
				else if (dowylaczenia=='C')
				{
					ZGAS(SYG10,SYG11,SYG12);
				}
				else if (dowylaczenia=='D')
				{
					ZGAS(SYG13,SYG14,SYG15);
				}

				//ZAPALANIE
				if (dowlaczenia=='A')
				{
					zmiana=FALSE;
					ZAPAL(SYG4,SYG5,SYG6,'A');

				}
				else if (dowlaczenia=='B')
				{
					zmiana=FALSE;
					ZAPAL(SYG7,SYG8,SYG9,'B');
				}
				else if (dowlaczenia=='C')
				{
					zmiana=FALSE;
					ZAPAL(SYG10,SYG11,SYG12,'C');
				}
				else if (dowlaczenia=='D')
				{
					zmiana=FALSE;
					ZAPAL(SYG13,SYG14,SYG15,'D');
				}
				else if (dowlaczenia=='Z' && timerValue==3*czaszolte )
				{
					//WLACZANIE TRYBU AUTO
					timerValue=0;
					dowylaczenia='Y';
					dowlaczenia='V';
					automanual=TRUE;
					CZERWONEALL();
				}
				else if (dowlaczenia!='Z')
				{
					//WCHODZI PO WYBORZE TRYNU MANUAL
					CZERWONEALL();
					timerValue=0;
					//WYLACZENIE TIMERA
					TIM_ITConfig(TIM1, TIM_IT_CC1, DISABLE);
				}


				if (timerValue>4*czaszolte)
				{
					//WCHODZI W MOMENCIE GDY ZASTOSUJE SIE DWIE KOMENDY MANUAL;WLACZ;
					timerValue=0;
				}

    		}
    		else
    		{
    			//WCHODZI PO WYBORZE SYGNALIZATORA
    			a=TRUE;
    			b=TRUE;
    			c=TRUE;
    			d=TRUE;

    			zmiana=TRUE;
    			timerValue=0;
    			//WYLACZENIE TIMERA
    			TIM_ITConfig(TIM1, TIM_IT_CC1, DISABLE);
    		}
    	}
    }
}
