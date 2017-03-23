#ifndef CRC16_H
#define CRC16_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Derived from CRC algorithm for JTAG ICE mkII, published in Atmel
 * Appnote AVR067.  Converted from C++ to C.
 */

  /*
  * March 23 - 2017
  * This is a modified and redistributed version of crc16 from avrdude's repository https://github.com/rwaldron/avrdude
  * I take no credit or claim to the original CRC16, and is only modified with the purpose to make my M24SR library work.
  * The code is only modified to take store the crc_table into the pgmspace.
  * 
  * Sincerely ProfSpaceman. 
  */
 
 
 
 
extern unsigned short crcsum(const unsigned char* message,
			     unsigned long length,
			     unsigned short crc);
/*
 * Verify that the last two bytes is a (LSB first) valid CRC of the
 * message.
 */
extern int crcverify(const unsigned char* message,
		     unsigned long length);
/*
 * Append a two byte CRC (LSB first) to message.  length is size of
 * message excluding crc.  Space for the CRC bytes must be allocated
 * in advance!
 */
extern void crcappend(unsigned char* message,
		      unsigned long length);

#ifdef __cplusplus
}
#endif

#endif
