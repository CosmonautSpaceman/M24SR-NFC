/*
 * NFC_lib_test.c
 *
 * Created: 3/1/2017 3:53:59 PM
 *  Author: JanAriel
 */ 
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include "m24sr.h"

/*	
	Example: 
	Writing onto Ndef File
	
	Need-to-know functions:
	
		M24SR_init();
			Initializes the necessary parameters for communication between the m24sr, as well as the ATmega328p.
			i2c, interrupts, flags and etc. Check the "m24sr.c" file to see more.
		
		functional_procedure_select_ndef_tag_app();
			Selects the Ndef tag application, checks and reads the CC file and then selects the Ndef File.
		
		write_ndeff("string", integer_value);
			Writes a message onto the m24sr. The integer value determines the type of data being sent to the Ndef File.
				1 for Text
				2 for URL
			Most of the other "write" functions follow the same procedure. Examples are listed below.
			
		command_deselect();
			Good practice to deselect, once finished, since it relieves the I2C token, from the ATmega, allowing for other i2c comms.
			
		Authenticate functions (read/write and hex/ascii) and _superuser_password_hex/ascii
		If your m24sr is password protected, you will need to use the authentication passwords:
		
			authenticate_read_ascii(char *Password);
			authenticate_write_ascii(char *Password);
			
			authenticate_write_hex(uint8_t Password[], uint8_t length);
			authenticate_read_hex(uint8_t Password[], uint8_t length);
			
			_superuser_password_confirmation_hex(uint8_t Password[], uint8_t length);
			_superuser_password_confirmation_ascii(char *Password);
		
		
		To change your password, use the following functions (Note: the password needs to be 16 bytes, and remember to authenticate before changing password):
		
		For Hex (Data array required):

			uint8_t default_password[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
			change_readpass_hex(default_password);
			change_writepass_hex(default_password);
			change_superepass_hex(default_password);
		
		
		For Hex (no array required):
		
			change_readpass_ascii("type here")
			change_writepass_ascii("type here")
			change_superpass_ascii("type here")
			
		
		If you wish to customize your own procedure, the "command_" functions are the way to go. The command functions are listed in the header file.
		
*/

int main(void)
{
	M24SR_init();

	//Example 1:
	//Read Ndef File:
	read_ndeff();
	
	//Example 2:
	//read CC file:
	functional_procedure_cc_file();
	
	//Example 3:
	//read System File:
	read_systemf();
	
	//Example 4:
	//reading proprietary data
	read_proprietary_data();
	

	
	
    while(1)
    {
        //TODO:: Please write your application code 
    }
}