#ifndef _M24SR_H_
#define _M24SR_H_

//Libraries
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crc16.h"
#include "i2c_master.h"

// Defines
#define NFC_WRITE 0xAC
#define NFC_READ 0xAD
#define CLA_ENABLE_PERM_VER 0xA2
#define TYPE_URL 0x55		//letter "U"
#define TYPE_TEXT 0x54		//letter "T"
#define TYPE_SP_H 0x53		//letter "S"
#define TYPE_SP_L 0x70		//letter "p"
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

//datatype definition
#define bool int
#define true 1
#define false 0

// Function prototypes:
//void init_uart(uint16_t baudrate);
//void uart_putc(unsigned char c);
//void uart_puts(char *s);
void i2c_begin_transimission(void);
void crc(uint8_t *data, uint8_t length);
void M24SR_init();

//Difference between C_Apdu and C_Apdu_D being data decided by user, and not stored in flash
void c_apdu(uint8_t CLA, uint8_t INS, uint8_t P1, uint8_t P2, uint8_t LC, const uint8_t *DataC, uint8_t size,uint8_t LE, uint8_t type_pay);
void c_apdu_d(uint8_t CLA, uint8_t INS, uint8_t P1, uint8_t P2, uint8_t LC, uint8_t DataC[], uint8_t size,uint8_t LE, uint8_t type_pay);
void s_block(uint8_t WTX,uint8_t DID);
void r_apdu(unsigned int len, bool retrieve, uint8_t reciveLen);
void send_payload(uint8_t *_data, uint8_t len, bool setPCB, bool fourByte);
void receive_payload(uint8_t R_Apdu_length, bool R_Apdu_Data, uint8_t R_Apdu_Data_Length);

void message_init(char *msg, uint8_t x, bool edit);
void command_select_ndef_tag_app(void);
void command_select_cc_file(void);
void command_read_cc_length(void);
void command_read_cc_data(void);
void command_select_ndef_file(void);
void command_read_ndef_file_length(void);
void command_read_ndef_file_data();
void command_update_ndef_file_clearlength(void);
void command_update_ndef_file_data(void);
void command_update_ndef_file_data_offset_resize(uint8_t offsetPos);
void command_update_ndef_file_data_offset(uint8_t offsetPos);
void command_update_ndef_file_length_offset(uint8_t length);
void command_update_ndef_file_msg_len(uint8_t DataType, uint8_t lengthOfData);
void command_update_ndef_file_length(void);
void command_deselect(void);
void command_select_system_file(void);
void command_read_system_file_length(void);
void command_read_system_file_data(void);
void command_update_system_file_prog_gpo(void);
void command_verify_i2c_superuser_password(uint8_t Password[], uint8_t size);
void changeDefaultI2cPassword(uint8_t Password[], uint8_t size);
void command_verify_write_password__static(uint8_t x);
void command_verify_write_password(uint8_t Password[], uint8_t size);
void command_verify_read_password__static(uint8_t x);
void command_verify_read_password(uint8_t Password[], uint8_t size);
void command_change_ref_write_password(uint8_t Password[], uint8_t size);
void command_change_ref_read_password(uint8_t Password[], uint8_t size);
void functional_procedure_read_ndef_msg(void);
void functional_procedure_select_ndef_tag_app(void);
void functional_procedure_update_ndef_file(void);
void functional_procedure_select_system_file(void);
void functional_procedure_confirmationtest_system_file_update(void);
void functional_procedure_cc_file(void);
void functional_procedure_update_ndef_pos_resize(uint8_t x);
void functional_procedure_update_ndef_pos_nonresize(uint8_t x);

void read_ndeff(void);
void write_ndeff(char *s, uint8_t x);
void edit_ndeff_resize(char *s, uint8_t DataType, uint8_t x);
void edit_ndeff(char *s, uint8_t DataType, uint8_t x);
void enable_VerificationRequirement(uint8_t pass_type);
void disable_VerificationRequirement(uint8_t pass_type);

void enable_Permanent_Verification(uint8_t pass_type);
void disable_Permanent_Verification(uint8_t pass_type);

void authenticate_read_ascii(char *Password);
void authenticate_write_ascii(char *Password);
void authenticate_write_hex(uint8_t Password[], uint8_t length);
void authenticate_read_hex(uint8_t Password[], uint8_t length);
void _superuser_password_confirmation_hex(uint8_t Password[], uint8_t length);
void _superuser_password_confirmation_ascii(char *Password);

void change_readpass_ascii(char *s);
void change_writepass_ascii(char *s);
void change_writepass_hex(uint8_t s[], uint8_t length);
void change_readpass_hex(uint8_t s[], uint8_t length);
void change_superpass_ascii(char *s);
void change_superepass_hex(uint8_t s[], uint8_t length);
void change_gpo(uint8_t the_byte);
void read_systemf(void);


void command_read_proprietary_info(uint8_t msg_length);
void command_update_proprietary_info(void);
void proprietary_msg(char *msg);
void write_proprietary_data(char *s);
void read_proprietary_data(uint8_t pos);

void write_ndeff_smartposter_init(void);
void write_ndeff_smartposter_message(char* msg, uint8_t x);
void write_ndeff_smartposter(void);


void passwordRotator(uint8_t x, uint8_t *Password);

#endif // _M24SR_H_
