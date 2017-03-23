#ifndef F_CPU
#define F_CPU 16000000UL
#endif

//Libraries

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include "crc16.h"
#include "i2c_master.h"
#include "m24sr.h"
// Defines
#define NFC_WRITE 0xAC
#define NFC_READ 0xAD
#define CLA_ENABLE_PERM_VER 0xA2
#define TYPE_URL 0x55		//letter "U"
#define TYPE_TEXT 0x54		//letter "T"
#define TYPE_SP_H 0x53		//letter "S"
#define TYPE_SP_L 0x70		//letter "p"
//#define URL 1
#define LC_BLANK 0xFF
#define LE_BLANK 0xFF
#define P_BYTE_BLANK 0xFF
#define SIZE_BLANK 0
#define PASS_READ_H 0x00
#define PASS_READ_L 0x01
#define PASS_WRITE_H 0x00
#define PASS_WRITE_L 0x02
#define PASS_READ 0
#define PASS_WRITE 1

//Boolean
bool R_Apdu_Data;
bool nextInstr = true;
bool nextRec = false;
bool nextProceed = false;
bool wtx = true;
bool crc_test_flag = false;
bool interrupt_setup = false;
bool prog_gpo_Init = true;
bool record_header_init = false;
//bool password_setr = false;
//bool password_setw = false;



PROGMEM const uint8_t prog_select_ndef_tag_app[7] = {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
PROGMEM const uint8_t prog_select_cc_file[2] = {0xE1, 0x03};
PROGMEM const uint8_t prog_select_ndef_file[2] = {0x00,0x01};
PROGMEM const uint8_t prog_update_ndef_file[2] = {0x00,0x00};
PROGMEM const uint8_t prog_select_system_file[2] = {0xE1,0x01};
PROGMEM const uint8_t prog_gpo[1] = {0x33};
PROGMEM const uint8_t prog_data_blank[1] = {0xFF};


enum {
	select_file = 0xA4,
	update_binary = 0xD6,
	read_binary = 0xB0,
	verify_password = 0x20,
	change_reference = 0x24,
	enable_verification = 0x28,
	disable_verification = 0x26,
}instruction;

enum {
	type0 = 0,
	type1 = 1,
	type2 = 2,
	type3 = 3
}c_apdu_catagory;

struct buffer {
	uint8_t message[128];
	uint8_t data[128];				//maybe use for smart poster instead of sp_message 128 bytes (to save memory)
	uint8_t data_retrieved[128];
	uint8_t message_total_length;
	uint8_t proprietary_data_pos;
	uint8_t proprietary_msg_length;
	uint8_t sp_record_count;
	uint8_t sp_pos;
	uint8_t sp_message_length;
	uint8_t sp_message[128];
} buffer;

struct ndef_setting {
	uint8_t ndef_length;
	uint8_t ndef_length_received;
	uint8_t ndef_data_type;
} ndef_setting;

struct functional_setting{
	uint8_t _block_no;
	uint8_t len1_rec;
	uint8_t len2_rec;
} functional_setting;

struct password_state {
	uint8_t read_access;
	uint8_t write_access;
	uint8_t incorrect_password;
	uint8_t authorization;
	bool read_lock;
	bool write_lock;
	bool perm_lock;
}password_state;

uint8_t _default_password[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t _read_password[16];// = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t _superuser_password[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t _write_password[16];// = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t _change_password[16]; //= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t _read_len = 16;
uint8_t _write_len = 16;
uint8_t _superuser = 16;
uint8_t _change_len = 16;
volatile uint8_t portbhistory = 0xFF;

//_read_password.uint_pass = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//_write_password.uint_pass = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};






void crc(uint8_t *payloadData, uint8_t length){	
	uint8_t v;
	uint8_t b;
	int crcCheck = crcsum((unsigned char*) payloadData, length, 0x6363);
	v = crcCheck & 0xff;
	b = (crcCheck >> 8) & 0xff;
	payloadData[length] = v;
	payloadData[length+1] = b;
	crc_test_flag = true;
}


void c_apdu(uint8_t CLA, uint8_t INS, uint8_t P1, uint8_t P2, uint8_t LC, const uint8_t *DataC, uint8_t size,uint8_t LE, uint8_t type_pay){
	buffer.data[1] = CLA;
	buffer.data[2] = INS;
	buffer.data[3] = P1;
	buffer.data[4] = P2;
	uint8_t pos;
	bool fourByte = false;
	
	switch (type_pay){
		case 0:
			buffer.data[5] = LC;
			pos = 6;
			for(uint8_t i = 0; i < 7; i++){
				buffer.data[pos] = pgm_read_byte_near(&(DataC[i]));
				pos++;
			}
			buffer.data[pos+1] = LE;
			break;
		case 1:
			buffer.data[5] = LC;
			pos = 6;
			for(uint8_t i = 0; i < size; i++){
				buffer.data[pos] = pgm_read_byte_near(&(DataC[i]));
				pos++;
			}
			break;
		case 2:
			buffer.data[5] = LE;
			break;
		case 3:
			fourByte = true;
			break;
		default:
			//Error
			buffer.data[5] = LC;
			uint8_t pos = 6;
			for(uint8_t i = 0; i < 7; i++){
				buffer.data[pos] = pgm_read_byte_near(&(DataC[i]));
				pos++;
			}
			buffer.data[pos+1] = LE;
			break;
	}
	send_payload(buffer.data, size,true, fourByte);
}

void c_apdu_d(uint8_t CLA, uint8_t INS, uint8_t P1, uint8_t P2, uint8_t LC, uint8_t DataC[], uint8_t size,uint8_t LE, uint8_t type_pay){
	buffer.data[1] = CLA;
	buffer.data[2] = INS;
	buffer.data[3] = P1;
	buffer.data[4] = P2;
	uint8_t pos;
	bool fourByte = false;
	
	switch (type_pay){
		case 0:
			buffer.data[5] = LC;
			pos = 6;
			for(uint8_t i = 0; i < 7; i++){
				buffer.data[pos] = DataC[i];
				pos++;
			}
			buffer.data[pos+1] = LE;
			break;
		case 1:
			buffer.data[5] = LC;
			pos = 6;
			for(uint8_t i = 0; i < size; i++){
				buffer.data[pos] = DataC[i];
				pos++;
			}
			break;
		case 2:
			buffer.data[5] = LE;
			break;
		case 3:
			fourByte = true;
			break;
		default:
			//Error
			buffer.data[5] = LC;
			uint8_t pos = 6;
			for(uint8_t i = 0; i < 7; i++){
				buffer.data[pos] = DataC[i];
				pos++;
			}
			buffer.data[pos+1] = LE;
			break;
	}
	send_payload(buffer.data, size,true, fourByte);
}

void s_block(uint8_t WTX,uint8_t DID){
	uint8_t SBlock[2] = {WTX, DID};
	crc(SBlock,2);
	i2c_transmit(NFC_WRITE,SBlock,4);
	//wtx = false;
}

void r_apdu(unsigned int len, bool retrieve, uint8_t retrieveLen){
	
	i2c_receive(0xAD, buffer.data, len);
	uint8_t WTX = buffer.data[0];
	
	if(WTX == 0xF2){
		uint8_t DID = buffer.data[1];
		if (wtx == true){
			s_block(WTX,DID);
			wtx = false;
			receive_payload(4,false,0);
		}else{
			wtx = true;
			_delay_ms((9.6*0x0b));
		}
	}
	if (retrieve){
		uint8_t pos = 1;
		for (uint8_t i = 0; i < retrieveLen; i++){
			buffer.data_retrieved[i] = buffer.data[pos];
			pos++;
		}
	}
}

void send_payload(uint8_t *_data, uint8_t len, bool setPCB, bool fourByte){
	
	uint8_t transmitLen;
	if(setPCB){
		if (functional_setting._block_no == 0){
			buffer.data[0] = 0x02;
			functional_setting._block_no = 1;
			}else {
			buffer.data[0] = 0x03;
			functional_setting._block_no = 0;
		}
	}
	if (fourByte == true){
		transmitLen = (5+len);
	} 
	else{
		transmitLen = (6+len);
	}
	
	crc(buffer.data,transmitLen);
	transmitLen = transmitLen+2;
	i2c_transmit(NFC_WRITE,buffer.data,transmitLen);
	
}

void receive_payload(uint8_t R_Apdu_length, bool R_Apdu_Data, uint8_t R_Apdu_Data_Length){
	if (interrupt_setup == true){
		nextRec = true;
		while (nextProceed == false){
			_delay_us(1);
		}
		if (nextProceed == true){
			nextRec = false;
			r_apdu(R_Apdu_length,R_Apdu_Data,R_Apdu_Data_Length);
		}
		nextProceed = false;
	}else{
		_delay_ms(20);
		r_apdu(R_Apdu_length,R_Apdu_Data,R_Apdu_Data_Length);
	}
}

void prog_gpo_interrupt_setup(){
	cli();
	DDRB &=~(1<<DDB4);
	PORTB |=(1<<PORTB4);
	PCICR |= (1<<PCIE0);
	PCMSK0 |= (1<<PCINT4);
	sei();
}

void M24SR_init(){
	
	prog_gpo_interrupt_setup();
	functional_setting._block_no = 0;
	password_state.read_lock = false;
	password_state.write_lock = false;
	password_state.perm_lock = false;
	buffer.sp_record_count = 0;
	
	
	i2c_init();
	functional_procedure_select_system_file();
	interrupt_setup = true;
}


/////////////////////////////////////////////////////////
// ------------------ I2C Message ---------------------//
/////////////////////////////////////////////////////////


void message_init(char *msg,uint8_t x, bool edit){
	uint8_t pos = 0;
	uint8_t record_header = 0xD1;
	if (record_header_init == true){
		switch (x){
		case 1:
			record_header = 0x91;
			break;
		case 2:
			record_header = 0x51;
			break;
		default:
			break;
		}
	}
	
	
	//char msg[32] = "akvagroup.com";
	uint8_t message_length = strlen(msg);
	uint8_t typeText[7] = {record_header, 0x01, (message_length+3), TYPE_TEXT, 0x02, 0x65, 0x6E};
	uint8_t typeURL[5] = {record_header, 0x01, (message_length+1), TYPE_URL, 0x01};			// Message length does not include type "(0x55)", but does include "0x01"
	buffer.message_total_length = 0;
	//uint8_t convMsg[];
	//memcpy(&msg,convMsg,sizeof(uint8_t));

	if (edit == true){
		switch (x){
			case 1:
				buffer.message_total_length = (message_length);
				for (uint8_t i = 0; i < (message_length+3); i++){
					buffer.message[i] = msg[i];
				}
				break;
			case 2:
				buffer.message_total_length = (message_length);
				for (uint8_t i = 0; i < (message_length+1); i++){
					buffer.message[i] = msg[i];
				}
				break;
			default:
				break;
		}
	} 
	else{
		switch (x){
			case 1:
				buffer.message_total_length = (message_length+7);
				for(uint8_t i = 0; i < buffer.message_total_length; i++){
					if (i < 7){
						buffer.message[i] = typeText[i];
						}else if (i >=7){
						buffer.message[i] = msg[pos];
						pos++;
					}
				}
			
				break;
			
			case 2:
				buffer.message_total_length = (message_length+5);
				for(int i = 0; i < buffer.message_total_length; i++){
					if (i < 5){
						buffer.message[i] = typeURL[i];
					}else if (i >=5){
						buffer.message[i] = msg[pos];
						pos++;
					}
				}
				break;
			
			default:
				break;
		}
	}
		
	
}

void write_ndeff_smartposter_init(void){
	record_header_init = true;
	buffer.sp_message_length = 0;
	buffer.sp_record_count = 0;
	buffer.sp_pos = 5;
}

void write_ndeff_smartposter_message(char* msg, uint8_t x){
	//buffer.sp_record_count = buffer.sp_record_count+1;
	message_init(msg, x, false);
	for (uint8_t i = 0; i < buffer.message_total_length; i++){
		buffer.sp_message[buffer.sp_pos] = buffer.message[i];
		buffer.sp_pos++;
	}
	buffer.sp_message_length = buffer.sp_message_length + buffer.message_total_length;
}

void write_ndeff_smartposter(void){
	uint8_t sp[5] = {0xD1, 0x02, buffer.sp_message_length, TYPE_SP_H, TYPE_SP_L};
	for (uint8_t i = 0; i < 5; i++){
		buffer.sp_message[i] = sp[i];
	}
	buffer.message_total_length = 5 + buffer.sp_message_length;
	for (uint8_t i = 0; i < buffer.message_total_length; i++){
		buffer.message[i] = buffer.sp_message[i];
	}
	
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}
	functional_procedure_update_ndef_file();
}


void proprietary_msg(char *msg){
	uint8_t message_length = strlen(msg);
	buffer.proprietary_msg_length = 0;
	
	buffer.proprietary_msg_length = (message_length);
	for (uint8_t i = 0; i < (message_length); i++){
		buffer.message[i] = msg[i];
	}
}


/////////////////////////////////////////////////////////
// --------------- I2C Transmissions ------------------//
/////////////////////////////////////////////////////////

void i2c_begin_transimission(void){
	i2c_start(NFC_WRITE);
	i2c_write(0x26);
	i2c_stop();
}

//Functional procedure: Selection of NDEF message

void command_select_ndef_tag_app(void){
	instruction = select_file;
	c_apdu_catagory = type0;
	c_apdu(0x00, select_file, 0x04, 0x00, 0x07, prog_select_ndef_tag_app, 7, 0x00, type0);
	receive_payload(5, false, 0);
}


/////////////////////////////////////////////////////////
// -------------------- CC File -----------------------//
/////////////////////////////////////////////////////////

void command_select_cc_file(void){
	instruction = select_file;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu(0x00, select_file, 0x00, 0x0C, 0x02, prog_select_cc_file, 2, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_read_cc_length(void){
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0x00, read_binary, 0x00, 0x00, LC_BLANK, prog_data_blank, SIZE_BLANK, 0x02, type2);
	receive_payload((3+2+2), true, 2);
	
	functional_setting.len1_rec = buffer.data_retrieved[0];
	functional_setting.len2_rec = buffer.data_retrieved[1];
}

void command_read_cc_data(void){
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0x00, read_binary, 0x00, functional_setting.len1_rec, LC_BLANK, prog_data_blank, SIZE_BLANK, functional_setting.len2_rec, type2);
	receive_payload((2+3+15), true, 15);
	
	password_state.read_access = buffer.data_retrieved[13];
	password_state.write_access = buffer.data_retrieved[14];
	
	if (password_state.read_access == 0x80){
		password_state.read_lock = true;
	}else if (password_state.read_access == 0xFF){
		password_state.perm_lock = true; 
	}
	
	if (password_state.write_access == 0x80){
		password_state.write_lock = true;
	}else if (password_state.write_access == 0xFF){
		password_state.perm_lock = true;
	}
}
	



/////////////////////////////////////////////////////////
// ------------------- NDEF File ----------------------//
/////////////////////////////////////////////////////////


void command_select_ndef_file(void){
	instruction = select_file;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu(0x00, select_file, 0x00, 0x0C, 0x02, prog_select_ndef_file, 2, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_read_ndef_file_length(void){
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0x00, read_binary, 0x00, 0x00, LC_BLANK, prog_data_blank, SIZE_BLANK, 0x02, type2);
	receive_payload(7, true, 2);
	//NdefLengthByte = buffer.data_retrieved[1];
	//functional_setting.len1_rec = buffer.data_retrieved[0];
	
	functional_setting.len2_rec = buffer.data_retrieved[1];
	ndef_setting.ndef_length_received = (3 + 2 + buffer.data_retrieved[1]);
}

void command_read_ndef_file_data(){
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0x00, read_binary, 0x00, 0x02, LC_BLANK, prog_data_blank, SIZE_BLANK, functional_setting.len2_rec, type2);
	receive_payload(ndef_setting.ndef_length_received, true, functional_setting.len2_rec);
	
	ndef_setting.ndef_data_type = buffer.data_retrieved[3];
}

void command_update_ndef_file_clearlength(void){
	instruction = update_binary;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu(0x00, update_binary, 0x00, 0x00, 0x02, prog_update_ndef_file, 2, LE_BLANK, type1);
	receive_payload(5, false, 0);
}


void command_update_ndef_file_data(void){
	instruction = update_binary;
	c_apdu_catagory = type1;
	ndef_setting.ndef_length = (5 + 1 + buffer.message_total_length);
	
	i2c_begin_transimission();
	c_apdu_d(0x00, update_binary, 0x00, 0x02, buffer.message_total_length, buffer.message, buffer.message_total_length, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_update_ndef_file_data_offset_resize(uint8_t offsetPos){
	instruction = update_binary;
	c_apdu_catagory = type1;
	//Remember +2 offset from top
	if ((offsetPos + buffer.message_total_length)< functional_setting.len2_rec){
		functional_setting.len2_rec = (offsetPos + buffer.message_total_length);
	}else if ((offsetPos + buffer.message_total_length)> functional_setting.len2_rec){
		functional_setting.len2_rec = (offsetPos + buffer.message_total_length);
	}
	offsetPos = offsetPos+2;
	
	i2c_begin_transimission();
	c_apdu_d(0x00,update_binary,0x00,offsetPos,buffer.message_total_length,buffer.message,buffer.message_total_length,LE_BLANK,type1);
	receive_payload(5, false, 0);
	command_update_ndef_file_msg_len(ndef_setting.ndef_data_type, functional_setting.len2_rec);
	command_update_ndef_file_length_offset(functional_setting.len2_rec);
}

void command_update_ndef_file_data_offset(uint8_t offsetPos){
	instruction = update_binary;
	c_apdu_catagory = type1;
	//Remember +2 offset from top
	if ((offsetPos + buffer.message_total_length)< functional_setting.len2_rec){
		functional_setting.len2_rec = (offsetPos + buffer.message_total_length);
	}else if ((offsetPos + buffer.message_total_length)> functional_setting.len2_rec){
		functional_setting.len2_rec = (offsetPos + buffer.message_total_length);
	}
	offsetPos = offsetPos+2;
	
	i2c_begin_transimission();
	c_apdu_d(0x00,update_binary,0x00,offsetPos,buffer.message_total_length,buffer.message,buffer.message_total_length,LE_BLANK,type1);
	receive_payload(5, false, 0);
	command_update_ndef_file_msg_len(ndef_setting.ndef_data_type, functional_setting.len2_rec);
	command_update_ndef_file_length_offset(functional_setting.len2_rec);
}

void command_update_ndef_file_msg_len(uint8_t DataType, uint8_t lengthOfData){
	uint8_t DataC[1];
	instruction = update_binary;
	c_apdu_catagory = type1;
	if (DataType == TYPE_TEXT){
		//0 = text, for now
		lengthOfData = (lengthOfData - 4);
	}else if (DataType == TYPE_URL){
		lengthOfData = (lengthOfData - 4);
	}else{
		lengthOfData = (lengthOfData-4);
	}
	DataC[0] = lengthOfData;
	
	
	i2c_begin_transimission();
	//Remember +2 offset from top
	c_apdu_d(0x00,update_binary,0x00,0x04,0x01,DataC,1,LE_BLANK,type1);
	receive_payload(5, false, 0);
}

void command_update_ndef_file_length_offset(uint8_t length){
	uint8_t DataC[2] = {0x00, length};
	instruction = update_binary;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, update_binary, 0x00, 0x00, 0x02, DataC, 2, LE_BLANK, type1);
	receive_payload(5, false, 0);
}


void command_update_ndef_file_length(void){
	uint8_t DataC[2] = {0x00,(buffer.message_total_length)};
	instruction = update_binary;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, update_binary, 0x00, 0x00, 0x02, DataC, 2, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_update_proprietary_info(void){
	buffer.proprietary_data_pos = buffer.message_total_length + 5;
	instruction = update_binary;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, update_binary, 0x00, buffer.proprietary_data_pos, buffer.proprietary_msg_length, buffer.message, buffer.proprietary_msg_length, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_read_proprietary_info(uint8_t msg_length){
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0xA2, read_binary, 0x00, buffer.proprietary_data_pos, LC_BLANK, prog_data_blank, SIZE_BLANK, msg_length, type2);
	receive_payload(ndef_setting.ndef_length_received, true, functional_setting.len2_rec);
}


void command_deselect(void){
	uint8_t Deselect[3] = {0xC2};
		
	i2c_begin_transimission();
	crc(Deselect,1);
	_delay_ms(10);
	i2c_transmit(NFC_WRITE,Deselect,3);
	receive_payload(3, false, 0);
}

/////////////////////////////////////////////////////////
// ------------------ System File ---------------------//
/////////////////////////////////////////////////////////

void passwordRotator(uint8_t x, uint8_t *Password){
	switch (x){
		case 0:
			for (uint8_t i = 0; i < 16; i++){
				Password[i] = 0x00;
			}
			break;
		case 1:
			for (uint8_t i = 0; i < 16; i++){
				Password[i] = 0x55;
			}
			break;
	
		case 2:
			for (uint8_t i = 0; i < 16; i++){
				Password[i] = 0xAA;
			}
			break;
	
		default:
			for (uint8_t i = 0; i < 16; i++){
				Password[i] = 0x00;
			}
			break;
	}
}

void command_select_system_file(void){
	instruction = select_file;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu(0x00, select_file, 0x00, 0x0C, 0x02, prog_select_system_file, 2, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_read_system_file_length(void){
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0x00, read_binary, 0x00, 0x00, LC_BLANK, prog_data_blank, SIZE_BLANK, 0x02, type2);
	receive_payload(7, true, 2);
	
	functional_setting.len1_rec = buffer.data_retrieved[0];
	functional_setting.len2_rec = buffer.data_retrieved[1];
}

void command_read_system_file_data(void){
	uint16_t Length = ((functional_setting.len1_rec << 8) | functional_setting.len2_rec );
	instruction = read_binary;
	c_apdu_catagory = type2;
	
	i2c_begin_transimission();
	c_apdu(0x00, read_binary, 0x00, 0x00, LC_BLANK, prog_data_blank, SIZE_BLANK, functional_setting.len2_rec, type2);
	receive_payload((3+2+Length), true, Length);

}

void command_update_system_file_prog_gpo(void){
	instruction = update_binary;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu(0x00, update_binary, 0x00, 0x04, 0x01, prog_gpo, 1, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void command_update_change_gpo_byte(uint8_t gpo[]){
	instruction = update_binary;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, update_binary, 0x00, 0x04, 0x01, gpo, 1, LE_BLANK, type1);
	receive_payload(5, false, 0);
}


/////////////////////////////////////////////////////////
// ------------ NFC Password Procedure ----------------//
/////////////////////////////////////////////////////////


// TODO: MAKE THIS DYNAMIC

void command_verify_i2c_superuser_password(uint8_t Password[], uint8_t size){
	uint8_t passwordType_i2cTransmit[2] = {0x00,0x03};
	instruction = verify_password;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	passwordRotator(0, _default_password);
	c_apdu_d(0x00, verify_password, passwordType_i2cTransmit[0], passwordType_i2cTransmit[1], size, Password, size, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.authorization = buffer.data_retrieved[0];
}

void changeDefaultI2cPassword(uint8_t Password[], uint8_t size){
	uint8_t passwordType_i2cTransmit[2] = {0x00,0x03};
	instruction = change_reference;
	c_apdu_catagory = type1;
		
	i2c_begin_transimission();
	passwordRotator(0,_default_password);
	c_apdu_d(0x00, change_reference, passwordType_i2cTransmit[0], passwordType_i2cTransmit[1], size, Password, size, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.authorization = buffer.data_retrieved[0];
}






/////////////////////////////////////////////////////////
// --------- NFC Password Procedure VERIFY ------------//
/////////////////////////////////////////////////////////

//////////////// WRITE ///////////////////////

void command_verify_write_password__static(uint8_t x){
	i2c_begin_transimission();
	passwordRotator(x,_default_password);
	instruction = verify_password;
	c_apdu_catagory = type1;
	uint8_t passwordType_WriteNdef[2] = {0x00,0x02};
	c_apdu_d(0x00, verify_password, passwordType_WriteNdef[0], passwordType_WriteNdef[1], 0x10, _default_password, 16, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.authorization = buffer.data_retrieved[0];
}

/////////////// WRITE USER //////////////////

void command_verify_write_password(uint8_t Password[], uint8_t size){
	uint8_t passwordType_WriteNdef[2] = {0x00,0x02};
	instruction = verify_password;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, verify_password, passwordType_WriteNdef[0], passwordType_WriteNdef[1], size, _write_password, size, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.authorization = buffer.data_retrieved[0];
}

//////////// READ //////////////////////

//void command_verify_read_password(void){
void command_verify_read_password__static(uint8_t x){
	uint8_t passwordType_ReadNdef[2] = {0x00,0x01};
	instruction = verify_password;
	c_apdu_catagory = type1;
	passwordRotator(x,_default_password);
	
	i2c_begin_transimission();
	c_apdu_d(0x00, verify_password, passwordType_ReadNdef[0], passwordType_ReadNdef[1], 0x10, _default_password, 16, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.authorization = buffer.data_retrieved[0];
	
}

//////////// READ USER /////////////////

void command_verify_read_password(uint8_t Password[], uint8_t size){
	uint8_t passwordType_ReadNdef[2] = {0x00,0x01};
	instruction = verify_password;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, verify_password, passwordType_ReadNdef[0], passwordType_ReadNdef[1], size, Password, size, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.authorization = buffer.data_retrieved[0];
}

/////////////////////////////////////////////////////////
// --------- NFC Password Procedure CHANGE ------------//
/////////////////////////////////////////////////////////



//////////// WRITE ///////////////////
//TODO
void command_change_ref_write_password(uint8_t Password[], uint8_t size){
	uint8_t passwordType_WriteNdef[2] = {0x00,0x02};
	instruction = change_reference;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, change_reference, passwordType_WriteNdef[0], passwordType_WriteNdef[1], size, Password, size, LE_BLANK, type1);
	receive_payload(5, false, 0);
	password_state.incorrect_password = buffer.data_retrieved[0];
}

/////////////// READ //////////////////////

void command_change_ref_read_password(uint8_t Password[], uint8_t size){
	uint8_t passwordType_ReadNdef[2] = {0x00,0x01};
	instruction = change_reference;
	c_apdu_catagory = type1;
	
	i2c_begin_transimission();
	c_apdu_d(0x00, change_reference, passwordType_ReadNdef[0], passwordType_ReadNdef[1], size, Password, size, LE_BLANK, type1);
	receive_payload(5, false, 0);
}

void enable_VerificationRequirement(uint8_t pass_type){
	instruction = enable_verification;
	c_apdu_catagory = type3;
	
	i2c_begin_transimission();
	switch (pass_type){
	case PASS_READ:
		c_apdu(0x00, enable_verification, PASS_READ_H, PASS_READ_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);	
		break;
	case PASS_WRITE:
		c_apdu(0x00, enable_verification, PASS_WRITE_H, PASS_WRITE_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
		break;
	default:
		break;
	}
	receive_payload(4, false, 0);
}

void disable_VerificationRequirement(uint8_t pass_type){
	instruction = disable_verification;
	c_apdu_catagory = type3;
	
	i2c_begin_transimission();
	switch (pass_type){
		case PASS_READ:
			c_apdu(0x00, disable_verification, PASS_READ_H, PASS_READ_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
			break;
		case PASS_WRITE:
			c_apdu(0x00, disable_verification, PASS_WRITE_H, PASS_WRITE_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
			break;
		default:
			
		break;
	}
	receive_payload(4, false, 0);
}

void enable_Permanent_Verification(uint8_t pass_type){
	instruction = enable_verification;
	c_apdu_catagory = type3;
	
	i2c_begin_transimission();
	switch (pass_type){
		case PASS_READ:
			c_apdu(CLA_ENABLE_PERM_VER, enable_verification, PASS_READ_H, PASS_READ_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
			break;
		case PASS_WRITE:
			c_apdu(CLA_ENABLE_PERM_VER, enable_verification, PASS_WRITE_H, PASS_WRITE_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
			break;
		default:
			
		break;
	}
	receive_payload(4, false, 0);
}


void disable_Permanent_Verification(uint8_t pass_type){
	instruction = disable_verification;
	c_apdu_catagory = type3;
	
	i2c_begin_transimission();
	switch (pass_type){
		case PASS_READ:
		c_apdu(CLA_ENABLE_PERM_VER, disable_verification, PASS_READ_H, PASS_READ_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
		break;
		case PASS_WRITE:
		c_apdu(CLA_ENABLE_PERM_VER, disable_verification, PASS_WRITE_H, PASS_WRITE_L, LC_BLANK, prog_data_blank, SIZE_BLANK, LE_BLANK, type3);
		break;
		default:
		
		break;
	}
	receive_payload(4, false, 0);
}


/////////////////////////////////////////////////////////
// ------------ Functional procedures -----------------//
/////////////////////////////////////////////////////////
//TODO

void functional_procedure_select_ndef_tag_app(void){
	
	i2c_begin_transimission();
	command_select_ndef_tag_app();
	command_select_cc_file();
	command_read_cc_length();
	command_read_cc_data();
	command_select_ndef_file();
}

void functional_procedure_read_ndef_msg(void){
	
	command_read_ndef_file_length();
	command_read_ndef_file_data();
	_delay_ms(5);
	command_deselect();
}

void functional_procedure_update_ndef_file(void){
	
	command_update_ndef_file_clearlength();
	command_update_ndef_file_data();
	command_update_ndef_file_length();
}



void functional_procedure_select_system_file(void){
	
	/* TODO
	 * Fix delays
	 * Initial start appears to require a delay (on average - measured by a logic analyzer), around 5ms
	 */
	
	i2c_begin_transimission();
	if (prog_gpo_Init == true){
		_delay_ms(5);
		prog_gpo_Init = false;
	}
	
	command_select_ndef_tag_app();
	command_select_system_file();
	command_read_system_file_length();
	command_read_system_file_data();
	command_verify_i2c_superuser_password(_superuser_password, 16);
	command_update_system_file_prog_gpo();
}

void functional_procedure_confirmationtest_system_file_update(void){
	
	i2c_begin_transimission();
	command_select_ndef_tag_app();
	command_select_system_file();
	command_read_system_file_length();
	command_read_system_file_data();
}

void functional_procedure_cc_file(void){
	i2c_begin_transimission();
	command_select_ndef_tag_app();
	command_select_cc_file();
	command_read_cc_length();
	command_read_cc_data();
}

void functional_procedure_update_ndef_pos_resize(uint8_t x){
	
	command_update_ndef_file_clearlength();
	command_update_ndef_file_data_offset_resize(x);
	//command_update_ndef_file_length_offset();	
}

void functional_procedure_update_ndef_pos_nonresize(uint8_t x){
	command_update_ndef_file_clearlength();
	command_update_ndef_file_data_offset(x);
	//command_update_ndef_file_length_offset();
}



/*
 * The following list of functions are the functions that are to be formally used when programming the m24sr.
 * 
 * It should be noted that if in case the password verification has been implemented and programmed onto the m24sr, the authentication
 * function "authenticate_read_hex" ("read" and "hex", changeable with "write" and "ascii", depending on the password needed for verification)
 * is to be used, before a function's use. If the password is not recognized, or the wrong password has been sent to the function in need of
 * authentication, the function will automatically send the "deselect" command to the m24sr, which will in turn stop all functions
 * following aforementioned function of concern. 
 * 
 * If one were to forget either the "write" password, or the "read" password, the passwords can be omitted by the use of the i2c superuser
 * password, with the function "_superuser_password_confirmation_hex" (where "hex is interchangeable with ascii"), assuming that the
 * super-user password has not been changed.
 * 
 * At the end of your program (when finished), it is advised to execute the "deselect" command/function. This is helpful for a number of reasons.
 * When executing "deselect" you return the "i2c token", which is what disallows any other device on an i2c network (if connected to one), to 
 * interrupt the "master" device. By returning the i2c token, you also reset the the remaining wrong-password-attempts byte value for a given 
 * password, if a wrong password has been attempted on one of the functions.
 * 
 * 
 * 
 * 
 * Most of the functions listed below require that a file is to be selected, before any of the functions are to be executed - "unless" they are
 * noted otherwise, below. Usually, the functions that have nothing to do with the Ndef file, require file selection and authentication.
 * 
 */



void write_ndeff(char *s, uint8_t x){
	
	bool edit = false;
	message_init(s, x, edit);
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}
	functional_procedure_update_ndef_file();
}

void write_proprietary_data(char *s){
	proprietary_msg(s);
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}
	command_update_proprietary_info();
}

void read_proprietary_data(uint8_t pos){
	command_read_proprietary_info(pos);
}
	
void edit_ndeff_resize(char *s, uint8_t DataType, uint8_t x){
	bool edit = true;
	message_init(s, DataType, edit);
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}
	command_read_ndef_file_length();
	command_read_ndef_file_data();
	functional_procedure_update_ndef_pos_resize(x);
}

void edit_ndeff(char *s, uint8_t DataType, uint8_t x){
	bool edit = true;
	message_init(s, DataType, edit);
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}
	command_read_ndef_file_length();
	command_read_ndef_file_data();
	functional_procedure_update_ndef_pos_nonresize(x);
}

void read_cc(void){
	/* 
	 For this function, there is no need to execute any of the selection functions.
	 Also, there is no need for password authentication, since the CC file is publicly available
	*/
	functional_procedure_cc_file();
	command_deselect();
}

void read_systemf(void){
	/*
	This function, like the read_cc function, is also publicly available to read. Editing the file, however, requires
	password authentication. The following function "change_gpo" is the only example of that in this library, but if you wish to edit another byte,
	you can use the c_apdu function. Instructions on how to use it is described at the location of its code definition, in this file.
	*/	
	command_select_ndef_tag_app();
	command_select_system_file();
	command_read_system_file_length();
	command_read_system_file_data();
}

void change_gpo(uint8_t the_byte){
	/*
	 This function works in junction with the previous function, as well as a superuser authentication process, meaning that both of these requirements
	 need to be fulfilled, before the the gpo file can be edited.
	*/
	uint8_t gpo_byte[1];
	command_update_change_gpo_byte(gpo_byte);
}

void read_ndeff(void){
	if ((password_state.authorization != 0x90) && (password_state.read_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}
	functional_procedure_read_ndef_msg();
	command_deselect();
}



/*
	For all of the following password-concerning functions, the functions have two versions of themselves. Depending on what type of password
	one wishes to implement, change, or authenticate, the functions denote either "ascii" or "hex". It should also be noted
	that the passwords for all password-functions start with 0x00 for 16 bytes, as per default, since the m24sr has this as their
	uses this default - until changed.
	
	Update:
	Since array arguments for functions become reduced to pointers, the size (length) cannot be determined inside of a function. So for the
	following functions, you will notice that the functions denoted with "hex" at the end, require a length parameter, in order to work,
	whereas the ascii functions work, because of the "strel" function, and therefore do not require the aforementioned length parameter.
*/



void change_readpass_ascii(char *s){
	uint8_t length = strlen(s);
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		_change_password[i] = s[i];
		count++;
	}
	_change_len = count;
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}else{
		command_change_ref_read_password(_change_password,_change_len);
	}
}

void change_readpass_hex(uint8_t s[], uint8_t length){
	length = sizeof(s);
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		_change_password[i] = s[i];
		count++;
	}
	_change_len = count;
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}else{
		command_change_ref_read_password(_change_password,16);
	}
}

void change_writepass_ascii(char *s){
	uint8_t length = strlen(s);
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		_change_password[i] = s[i];
		count++;
	}
	_change_len = count;
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}else{
		command_change_ref_write_password(_change_password,_change_len);
	}
}

void change_writepass_hex(uint8_t s[], uint8_t length){
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		_change_password[i] = s[i];
		count++;
	}
	_change_len = length;
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}
	if (password_state.perm_lock == true){
		command_deselect();
	}
	command_change_ref_write_password(_change_password,_change_len);
	
}

void authenticate_read_ascii(char *Password){
	_read_len = strlen(Password);
	uint8_t count = 0;
	for (uint8_t i = 0; i < _read_len; i++){
		_read_password[i] = Password[i];
		count++;
	}
	_read_len = count;
	command_verify_read_password(_read_password,_read_len);
}

void authenticate_read_hex(uint8_t Password[], uint8_t length){
	_read_len = length;
	for (uint8_t i = 0; i < _read_len; i++){
		_read_password[i] = Password[i];
	}
	command_verify_read_password(_read_password, _read_len);
}

void authenticate_write_ascii(char *Password){
	_write_len = strlen(Password);
	uint8_t count = 0;
	for (uint8_t i = 0; i < _write_len; i++){
		_write_password[i] = Password[i];
		count++;
	}
	_write_len = count;
	command_verify_write_password(_write_password, _write_len);
}

void authenticate_write_hex(uint8_t Password[], uint8_t length){
	_write_len = length;
	for (uint8_t i = 0; i < _write_len; i++){
		_write_password[i] = Password[i];
	}
	_write_len = sizeof(_write_password);
	command_verify_write_password(_write_password, _write_len);
}

//i2c super user
void _superuser_password_confirmation_ascii(char *Password){
	_superuser = strlen(Password);
	uint8_t count = 0;
	for (uint8_t i = 0; i < _superuser; i++){
		_superuser_password[i] = Password[i];
		count++;
	}
	_superuser = count;
	command_verify_i2c_superuser_password(_superuser_password,_superuser);
}

void _superuser_password_confirmation_hex(uint8_t Password[], uint8_t length){
	_superuser = sizeof(Password);
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		_superuser_password[i] = Password[i];
		count++;
	}
	_superuser = count;
	command_verify_i2c_superuser_password(_superuser_password, _superuser);
}

void change_superpass_ascii(char *s){
	uint8_t length = strlen(s);
	uint8_t pass[16];
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		pass[i] = s[i];
		count++;
	}
	length = count;
	//command_verify_write_password(_write_password, _write_len);
	//check for i2c receive.
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}else{
		//password_setw = false;
		changeDefaultI2cPassword(pass,length);
	}
}

void change_superepass_hex(uint8_t s[], uint8_t length){
	uint8_t pass[16];
	uint8_t count = 0;
	for (uint8_t i = 0; i < length; i++){
		pass[i] = s[i];
		count++;
	}
	length = count;
	if ((password_state.authorization != 0x90) && (password_state.write_lock == true)){
		command_deselect();
	}else if (password_state.perm_lock == true){
		command_deselect();
	}else{
		//password_setw = false;
		changeDefaultI2cPassword(pass,length);
	}
}

ISR(PCINT0_vect){
	uint8_t changedbits;
	changedbits = PINB ^ portbhistory;
	portbhistory = PINB;

	if(nextRec == true){
		if(changedbits & (1 << PINB4)){
			nextProceed = true;
		}
	}
}