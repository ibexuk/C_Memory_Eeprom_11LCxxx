/*
Provided by IBEX UK LTD http://www.ibexuk.com
Electronic Product Design Specialists
RELEASED SOFTWARE

The MIT License (MIT)

Copyright (c) IBEX UK Ltd, http://ibexuk.com

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//Visit http://www.embedded-code.com/source-code/memory/eeprom/microchip-11lcxxx-eeprom-with-uni-o-1-wire-port for more information
//
//Project Name:	11LC010T EEPROM USING UNI/O 1 WIRE BUS



//####################
//####################
//##### UNIO BUS #####
//####################
//####################
//- Serial bus frequency: 10kHz to 100kHz (you decide)
//- Bit period 10uS to 100uS
//- 10bits per byte transfer (Byte + MAK + SAK), so takes 0.1mS to 1mS per byte transferred over bus



//##############################
//##############################
//##### USING IN A PROJECT #####
//##############################
//##############################

//N.B. THIS DRIVER REQURIES A HARDWARE TIMER AND DISABLES INTERRUPTS DURING READ AND WRITES TO ACHIIVE THE REQUIRED ACCURATE BIT TIMING.
/*
	//----- INITIALISE -----
	unio_eeprom_init();


	//----- 1 WIRE EEPROM ------
	//unio_standby_pulse();		//Use if changing to communicate with a different device, not needed between comms with same device

	uint8_t data[UNIO_EEPROM_PAGE_SIZE];
	uint8_t count;

	if (!unio_is_eeprom_present())
	{
		//EEPROM NOT PRESENT
		Nop();
	}

	for (count = 0; count < UNIO_EEPROM_PAGE_SIZE; count++)
		data[count] = 0x40 + count;
	if (unio_eeprom_write(0x0000, &data[0], UNIO_EEPROM_PAGE_SIZE))
	{
		//Write Success
		Nop();
	}

	for (count = 0; count < UNIO_EEPROM_PAGE_SIZE; count++)
		data[count] = 0xff;
	if (unio_eeprom_read(0x0000, &data[0], UNIO_EEPROM_PAGE_SIZE))
	{
		//Read Success
		Nop();
	}
	Nop();
*/


//********************************
//********************************
//********** MEMORY MAP **********
//********************************
//********************************
//24LC010T
//Each eeprom:
//1K bits = 128 x 8bit (0x00 - 0x7f)
//Pages of 16 bytes may be written in a single operation, but they must be within the same 16 byte page (0x00-0x0F, 0x10-0x1F, etc)
//Eeprom max writes 1M cycles

#define	UNIO_EEPROM_PAGE_SIZE				16

//#define	UNIO_EEPROM_VALUE_0				0x0000
//#define	UNIO_EEPROM_VALUE_0_LEN			4



//*****************************
//*****************************
//********** DEFINES **********
//*****************************
//*****************************
#ifndef MEM_EXT_UNIO_EEPROM_C_INIT		//(Do only once)
#define	MEM_EXT_UNIO_EEPROM_C_INIT

#define	UNIO_EEPROM_ADDRESS		0xa0

//PIC32:
#define	UNIO_SCIO_TRIS(data)					(data ? mPORTESetPinsDigitalIn(0x0004) : mPORTESetPinsDigitalOut(0x0004))
#define	UNIO_SCIO_OUTPUT(data)					(data ? mPORTESetBits(0x0004) : mPORTEClearBits(0x0004))
#define	UNIO_SCIO_INPUT							mPORTEReadBits(BIT_2)
#define	UNIO_EEPROM_CLEAR_IRQ_FLAG()			INTClearFlag(INT_T2)
#define	UNIO_EEPROM_READ_IRQ_FLAG()				INTGetFlag(INT_T2)
#define	UNIO_EEPROM_WRITE_TIMER(data)			WriteTimer2(data)
#define	UNIO_EEPROM_TIMER_QUARTER_PERIOD		500		//2.5uS - 25uS to give 10kHz to 100kHz.  Our 20Mhz peripheral bus clock = 50nS.  2.5uS / 50nS = 50.  25uS / 50nS = 500
														//You must ensure bit timing it complelty accurate, make slower if there is risk of fucntion calls etc being too slow for bitrate you have set.
//Also set for this device/project:
//	unio_setup_timer_for_unio_use()		<<<Setup hardware timer
//	unio_delay_5us()




#endif


//*******************************
//*******************************
//********** FUNCTIONS **********
//*******************************
//*******************************
#ifdef MEM_EXT_UNIO_EEPROM_C
//-----------------------------------
//----- INTERNAL ONLY FUNCTIONS -----
//-----------------------------------
void unio_delay_5us (uint16_t delay_5us);
void unio_start_header (void);
void unio_output_byte(void);
void unio_input_byte (void);
void unio_input_bit (void);
void unio_write_enable (void);
void unio_ack_sequence (void);
void unio_standby (void);
void unio_wait_for_write_complete (void);


//-----------------------------------------
//----- INTERNAL & EXTERNAL FUNCTIONS -----
//-----------------------------------------
//(Also defined below as extern)
BYTE unio_is_eeprom_present (void);
BYTE unio_eeprom_write (uint16_t address, uint8_t *data, uint8_t length);
BYTE unio_eeprom_read (uint16_t address, uint8_t *data, uint8_t length);
void unio_standby_pulse (void);

#else
//------------------------------
//----- EXTERNAL FUNCTIONS -----
//------------------------------
extern BYTE unio_is_eeprom_present (void);
extern BYTE unio_eeprom_write (uint16_t address, uint8_t *data, uint8_t length);
extern BYTE unio_eeprom_read (uint16_t address, uint8_t *data, uint8_t length);
void unio_standby_pulse (void);

#endif




//****************************
//****************************
//********** MEMORY **********
//****************************
//****************************
#ifdef MEM_EXT_UNIO_EEPROM_C
//--------------------------------------------
//----- INTERNAL ONLY MEMORY DEFINITIONS -----
//--------------------------------------------
uint8_t unio_data_out;
uint8_t unio_data_in;
uint8_t unio_send_mak;
uint8_t unio_comms_error;
uint8_t unio_read_error;
uint8_t unio_input_bit_read;
uint8_t unio_count;
uint8_t unio_temp_data_buffer[UNIO_EEPROM_PAGE_SIZE];


//--------------------------------------------------
//----- INTERNAL & EXTERNAL MEMORY DEFINITIONS -----
//--------------------------------------------------
//(Also defined below as extern)



#else
//---------------------------------------
//----- EXTERNAL MEMORY DEFINITIONS -----
//---------------------------------------




#endif







