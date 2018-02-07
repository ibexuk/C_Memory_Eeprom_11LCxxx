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
//Project Name: 11LC010T EEPROM USING UNI/O 1 WIRE BUS



#include "main.h"					//Global data type definitions (see https://github.com/ibexuk/C_Generic_Header_File )


#define	MEM_EXT_UNIO_EEPROM_C		//(Our header file define)

#include "mem-11lcxxx.h"


//**********************************************
//**********************************************
//********** SETUP TIMER FOR UNIO USE **********
//**********************************************
//**********************************************
//We need a PR based timer that can cause a flag set every 10uS to allow us to create our 50kHz (20uS) clock
//This timer could be shared with other things as long as we get exclusive use while accessing our bus and this function is
//called to set the timer up for us before access.
void unio_setup_timer_for_unio_use (void)
{

	OpenTimer2((T2_ON | T2_IDLE_CON | T2_GATE_OFF | T2_PS_1_1 | T2_SOURCE_INT), (uint16_t)UNIO_EEPROM_TIMER_QUARTER_PERIOD);		//<<SET PRx VALUE TO GIVE #uS ROLL OVER AND SETTING OF IRQ FLAG

}



//*********************************************
//*********************************************
//********** DELAY FOR AT LEAST # uS **********
//*********************************************
//*********************************************
//Min 5uS, may produce longer delay for slower bus speeds (timer is only used for min times, longer times don't matter)
void unio_delay_5us (uint16_t delay_5us)
{

	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	UNIO_EEPROM_CLEAR_IRQ_FLAG();

	while (delay_5us)
	{
		while (!UNIO_EEPROM_READ_IRQ_FLAG())
			;
		UNIO_EEPROM_CLEAR_IRQ_FLAG();
		delay_5us--;
	}
}






//*****************************************
//*****************************************
//********** INITIALISE UNIO BUS **********
//*****************************************
//*****************************************
void unio_eeprom_init (void)
{
	UNIO_SCIO_TRIS(0);
	UNIO_SCIO_OUTPUT(0);

	//Setup the timer ready for us to use
	unio_setup_timer_for_unio_use();

	unio_delay_5us(1);				//Delay to ensure minimum pulse width of 125 ns

	UNIO_SCIO_OUTPUT(1);			//Bring high to release from POR

	unio_delay_5us(120);			//Hold SCIO high for min 600uS (Tstby) to generate standby pulse.
}



//***************************************
//***************************************
//********** IS EEPROM PRESENT **********
//***************************************
//***************************************
//Carries out a quick read of status register to see if device is present
//Returns:
//	1 if present, 0 if not present
BYTE unio_is_eeprom_present (void)
{
	unio_delay_5us(2);						//Observe Tss time (min 10uS, no max)

	//----- HEADER -----
	unio_send_mak = 1;
	unio_start_header();					//Output Start Header

	//----- DEVICE ADDRESS -----
	unio_data_out = UNIO_EEPROM_ADDRESS;
	unio_output_byte();
	if (!unio_input_bit_read)				//Got SAK?
	{
		unio_idle();
		return(0);
	}

	//----- WE GOT AN ACK - DEVICE PRESENT -----
	//COMPLETE THE READ SEQUENCE TO END THE BUS ACCESS

	unio_data_out = 0b00000101;				//RDSR command
	unio_output_byte();

	unio_input_byte();						//Input byte
	unio_send_mak = 0;
	unio_ack_sequence();

	unio_idle();
	return(1);
}


//*********************************
//*********************************
//********** EEPROM READ **********
//*********************************
//*********************************
//Returns:
//	1 is sucessful, 0 if failed (all bytes will be set to 0x00)
BYTE unio_eeprom_read (uint16_t address, uint8_t *data, uint8_t length)
{
	uint8_t count;
	uint8_t retry_count = 3;
	

	if (length < 1)
		return;

	if (length > UNIO_EEPROM_PAGE_SIZE)
		length = UNIO_EEPROM_PAGE_SIZE;

	while (retry_count--)
	{
		unio_comms_error = 0;
		
		DISABLE_INT;
		unio_delay_5us(2);						//Observe Tss time (min 10uS, no max)

		//----- HEADER -----
		unio_start_header();

		//----- DEVICE ADDRESS -----
		unio_data_out = UNIO_EEPROM_ADDRESS;
		unio_output_byte();
		if (!unio_input_bit_read)				//Got SAK?
			unio_comms_error = 1;

		//----- COMMAND -----
		unio_data_out = 0b00000011;				//READ command
		unio_output_byte();
		if (!unio_input_bit_read)				//Got SAK?
			unio_comms_error = 1;

		//----- START ADDRESS H -----
		unio_data_out = (uint8_t)((address & 0xff00) >> 8);
		unio_output_byte();
		if (!unio_input_bit_read)				//Got SAK?
			unio_comms_error = 1;

		//----- START ADDRESS L -----
		unio_data_out = (uint8_t)(address & 0x00ff);
		unio_output_byte();
		if (!unio_input_bit_read)				//Got SAK?
			unio_comms_error = 1;

		//----- DATA BYTES -----
		unio_read_error = 0;
		for (count = 0; count < length; count++)
		{
			unio_input_byte();
			data[count] = unio_data_in;
			if (count < (length - 1))
				unio_send_mak = 1;
			else
				unio_send_mak = 0;
			unio_ack_sequence();
			if (!unio_input_bit_read)				//Got SAK?
				unio_comms_error = 1;
		}
		if (unio_read_error)
			unio_comms_error = 1;

		unio_idle();
		ENABLE_INT;

		if (unio_comms_error)
		{
			//There was a read error, try again
			unio_standby_pulse();
			continue;
		}

		//----- SUCCESS ALL DONE -----
		return(1);

	}
	//----- FAILED -----
	ENABLE_INT;
	for (count = 0; count < length; count++)
	{
		data[count] = 0x00;
	}
	return(0);
}


//**********************************
//**********************************
//********** EEPROM WRITE **********
//**********************************
//**********************************
//Pages of 16 bytes may be written in a single operation, but they must be within the same 16 byte page (0x00-0x0F, 0x10-0x1F, etc)
//Returns:
//	1 is sucessful, 0 if failed
BYTE unio_eeprom_write (uint16_t address, uint8_t *data, uint8_t length)
{
	uint8_t count;
	uint8_t retry_count = 3;
	

	if (length < 1)
		return;

	if (length > UNIO_EEPROM_PAGE_SIZE)
		length = UNIO_EEPROM_PAGE_SIZE;

	while (retry_count--)
	{
		unio_comms_error = 0;
		DISABLE_INT;
		//----- ENABLE WRITES -----
		unio_write_enable();
		unio_delay_5us(2);							//Observe Tss time (min 10uS, no max)

		//----- HEADER -----
		unio_start_header();

		//----- DEVICE ADDRESS -----
		unio_data_out = UNIO_EEPROM_ADDRESS;		//Load DEVICE_ADDR into unio_data_out
		unio_output_byte();
		if (!unio_input_bit_read)					//Got SAK?
			unio_comms_error = 1;

		//----- COMMAND -----
		unio_data_out = 0b01101100;					//WRITE command
		unio_output_byte();
		if (!unio_input_bit_read)					//Got SAK?
			unio_comms_error = 1;

		//----- START ADDRESS H -----
		unio_data_out = (BYTE)((address & 0xff00) >> 8);	//Address MSB
		unio_output_byte();
		if (!unio_input_bit_read)					//Got SAK?
			unio_comms_error = 1;

		//----- START ADDRESS L -----
		unio_data_out = (BYTE)(address & 0x00ff);	//Address LSB
		unio_output_byte();
		if (!unio_input_bit_read)					//Got SAK?
			unio_comms_error = 1;

		//----- DATA BYTES -----
		for (count = 0; count < length; count++)
		{
			unio_data_out = data[count];
			if (count < (length - 1))
				unio_send_mak = 1;
			else
				unio_send_mak = 0;						//Send NoMAK on last byte to trigger write
			unio_output_byte();
			if (!unio_input_bit_read)					//Got SAK?
				unio_comms_error = 1;
		}

		unio_idle();

		unio_wait_for_write_complete();				//Perform WIP Polling
		ENABLE_INT;

		if (unio_comms_error)
		{
			//There was a write error, try again
			continue;
		}

		//----- WRITE SUCCESS - NOW READ BACK AND VERIFY DATA WAS CORRECT -----
		if (!unio_eeprom_read (address, &unio_temp_data_buffer[0], length))
		{
			//Read failed
			unio_standby_pulse();
			continue;
		}

		for (count = 0; count < length; count++)
		{
			if (unio_temp_data_buffer[count] != data[count])
			{
				//READ VERIFY FAILED
				unio_comms_error = 1;
				break;
			}
		}
		if (unio_comms_error)
		{
			unio_standby_pulse();
			continue;
		}


		//----- SUCCESS ALL DONE -----
		return(1);
	}
	//----- FAILED -----
	ENABLE_INT;
	return(0);
}













//**********************************
//**********************************
//********** WRITE ENABLE **********
//**********************************
//**********************************
//Set the WEL bit in the Status Register.
void unio_write_enable (void)
{
	unio_delay_5us(2);						//Observe Tss time (min 10uS, no max)

	//----- HEADER -----
    unio_start_header();

	//----- DEVICE ADDRESS -----
    unio_data_out = UNIO_EEPROM_ADDRESS;
    unio_output_byte();
	if (!unio_input_bit_read)				//Got SAK?
		unio_comms_error = 1;

	//----- COMMAND -----
    unio_data_out = 0b10010110;				//WREN command
    unio_send_mak = 0;
    unio_output_byte();
	if (!unio_input_bit_read)				//Got SAK?
		unio_comms_error = 1;

    unio_idle();
}


//**********************************
//**********************************
//********** START HEADER **********
//**********************************
//**********************************
//Hold SCIO low for Thdr time period (min 5uS) and output Start Header ('01010101').
//Takes 5 us (including call to OutputByte)
void unio_start_header (void)
{
	UNIO_SCIO_OUTPUT(0);
	UNIO_SCIO_TRIS(0);

	UNIO_EEPROM_CLEAR_IRQ_FLAG();		//Force min 5uS Thdr time period

	unio_data_out = 0x55;				//Load Start Header value

	unio_send_mak = 1;
	
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	UNIO_EEPROM_CLEAR_IRQ_FLAG();

	unio_output_byte();
	//if (!unio_input_bit_read)				//Got SAK? (no SAK sent after header as device not addressed yet)
	//	unio_comms_error = 1;
}




//*********************************
//*********************************
//********** OUTPUT BYTE **********
//*********************************
//*********************************
//Manchester-encodes & outputs the value in unio_data_out to SCIO.
void unio_output_byte(void)
{
	UNIO_SCIO_TRIS(0);					//Ensure SCIO is outputting

	unio_count = 8;
    while (unio_count--)
    {
		while (!UNIO_EEPROM_READ_IRQ_FLAG())
			;
		//Output first half of bit
		if (unio_data_out & 0x80)
			UNIO_SCIO_OUTPUT(0);			//If 1, set SCIO low
		else
			UNIO_SCIO_OUTPUT(1);			//If 0, set SCIO high

		UNIO_EEPROM_CLEAR_IRQ_FLAG();
		while (!UNIO_EEPROM_READ_IRQ_FLAG())
			;

		UNIO_EEPROM_CLEAR_IRQ_FLAG();
		while (!UNIO_EEPROM_READ_IRQ_FLAG())
			;

		//Output second half of bit
		if (unio_data_out & 0x80)
			UNIO_SCIO_OUTPUT(1);			//If 1, set SCIO high
		else
			UNIO_SCIO_OUTPUT(0);			//If 0, set SCIO low

		UNIO_EEPROM_CLEAR_IRQ_FLAG();
		while (!UNIO_EEPROM_READ_IRQ_FLAG())
			;

		UNIO_EEPROM_CLEAR_IRQ_FLAG();
        
		//Shift data left 1 bit
		unio_data_out <<= 1;
    }

    unio_ack_sequence();                 // Perform Acknowledge Sequence
}

//**************************************************
//**************************************************
//********** PERFORM ACKNOWLEDGE SEQUENCE **********
//**************************************************
//**************************************************
//Performs Acknowledge sequence, including MAK/NoMAK as specified by SEND_MAK flag, and SAK
void unio_ack_sequence(void)
{
	UNIO_SCIO_TRIS(0);					//Ensure SCIO is outputting

	//-------------------------------
	//----- DO MAK (Master ACK) -----
	//-------------------------------

    //Send MAK/NoMAK bit
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	//Output first half of bit
	if (unio_send_mak)
		UNIO_SCIO_OUTPUT(0);			//If 1, set SCIO low
	else
		UNIO_SCIO_OUTPUT(1);			//If 0, set SCIO high

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;

	//Output second half of bit
	if (unio_send_mak)
		UNIO_SCIO_OUTPUT(1);			//If 1, set SCIO high
	else
		UNIO_SCIO_OUTPUT(0);			//If 0, set SCIO low

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;

	//------------------------------
	//----- DO SAK (Slave ACK) -----
	//------------------------------
	UNIO_SCIO_TRIS(1);				//Set SCIO to be an input

	//Input SAK bit
	unio_input_bit();
}




//********************************
//********************************
//********** INPUT BYTE **********
//********************************
//********************************
//Inputs and Manchester-decodes from SCIO a byte of data and stores it in unio_data_in.
void unio_input_byte (void)
{
	UNIO_SCIO_TRIS(1);				//Set SCIO to be an input
	//Loop through byte
	for (unio_count = 0; unio_count < 8; unio_count++)
	{
		while (!UNIO_EEPROM_READ_IRQ_FLAG())
			;
		unio_data_in <<= 1;
		unio_input_bit();
	}
}


//*******************************
//*******************************
//********** INPUT BIT **********
//*******************************
//*******************************
//Inputs and Manchester-decodes from SCIO a bit of data and stores it in the LSb of dataIn
//Setup before calling:
//	unio_read_error = 0;
//Returns:
//	unio_read_error			Set if there was not a valid bit read (didn't see both states of the bit)
//	unio_data_in			Bit 0 updated with read bit if read was good
//	unio_input_bit_read		State of bit read, 1 if saw 01 sequence, 0 otherwise
void unio_input_bit (void)
{
	unio_input_bit_read = 0;

	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	//We are now at start of bit period

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	//We are now 1/4 into bit period

	if (UNIO_SCIO_INPUT)
		unio_input_bit_read |= 0x02;			//Bit1 = first half of bit

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;

	UNIO_EEPROM_CLEAR_IRQ_FLAG();
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	//We are now 3/4 into bit period
	if (UNIO_SCIO_INPUT)
		unio_input_bit_read |= 0x01;			//Bit0 = second half of bit

	UNIO_EEPROM_CLEAR_IRQ_FLAG();

	//We should see both states, otherwise there has been an error
	if (unio_input_bit_read == 0x01)
	{
		unio_data_in |= 0x01;
		unio_input_bit_read = 1;
	}
	else if (unio_input_bit_read == 0x02)
	{
		unio_data_in &= 0xfe;
		unio_input_bit_read = 0;
	}
	else
	{
		unio_read_error = 1;
		unio_input_bit_read = 0;
	}

}




//**************************
//**************************
//********** IDLE **********
//**************************
//**************************
//Waits until end of current bit period has been reached, ensures SCIO is high for bus idle
void unio_idle (void)
{
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	UNIO_EEPROM_CLEAR_IRQ_FLAG();

	UNIO_SCIO_OUTPUT(1);				//Ensure SCIO is high for bus idle
	UNIO_SCIO_TRIS(0);					//Ensure SCIO is outputting
}

//*****************************
//*****************************
//********** STANDBY **********
//*****************************
//*****************************
//Wait Tstby with line high between communicating with one device and a new device (not requried if communicating with same device)
void unio_standby_pulse (void)
{
	while (!UNIO_EEPROM_READ_IRQ_FLAG())
		;
	UNIO_EEPROM_CLEAR_IRQ_FLAG();

	UNIO_SCIO_OUTPUT(1);				//Ensure SCIO is high for bus idle
	UNIO_SCIO_TRIS(0);					//Ensure SCIO is outputting

	unio_delay_5us(120);				//Tstby min 600uS, no max
}



//*********************************************
//*********************************************
//********** WAIT FOR WRITE COMPLETE **********
//*********************************************
//*********************************************
//This function performs WIP polling to determine the end of the current write cycle. It does this by continuously
//executing a Read Status Register operation until the WIP bit (bit 0 of the Status Register) is read low.
void unio_wait_for_write_complete (void)
{
	uint16_t count;
	uint8_t retry_count = 3;
	uint8_t error_occured;

	while (retry_count--)
	{
		unio_delay_5us(2);						//Observe Tss time (min 10uS, no max)

		//----- HEADER -----
		unio_send_mak = 1;
		unio_start_header();					//Output Start Header

		//----- DEVICE ADDRESS -----
		unio_data_out = UNIO_EEPROM_ADDRESS;
		unio_output_byte();
		if (!unio_input_bit_read)				//Got SAK?
		{
			unio_standby_pulse();
			continue;
		}

		unio_data_out = 0b00000101;				//RDSR command
		unio_output_byte();
		if (!unio_input_bit_read)				//Got SAK?
		{
			unio_standby_pulse();
			continue;
		}


		count = 0;
		unio_read_error = 0;
		unio_input_byte();						//Input byte
		if (unio_read_error)
			unio_data_in |= 0x01;
		while ((unio_data_in & 0x01) && (count < 120))		//Max 10mS write, 10mS / 100uS(fastest possible read byte) = 50, add a bit for just in case
		{
			error_occured = 0;

			unio_ack_sequence();
			if (!unio_input_bit_read)				//Got SAK?
				error_occured = 1;

			count++;

			unio_read_error = 0;
			unio_input_byte();						//Input byte
			if (unio_read_error)
				error_occured = 1;

			if (error_occured)
				unio_data_in |= 0x01;
		}
		if (count >= 120)
			unio_comms_error = 1;

		unio_send_mak = 0;
		unio_ack_sequence();

		unio_idle();
	}
}








