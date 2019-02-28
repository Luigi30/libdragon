#pragma once

#include <libdragon.h>

#define WITH_64DRIVE

//
// CI base and register locations
//

#define CART_BASE_UNCACHED 	0xB0000000
#define CI_REG_BASE			0x08000000
#define CI_EXT_BASE			0x0F800000 

#define CI_REG_BUF			0x00000000
#define CI_REG_BUF4			0x00000004
#define CI_REG_STATUS		0x00000200
#define CI_REG_COMMAND		0x00000208
#define CI_REG_LBA			0x00000210
#define CI_REG_LENGTH		0x00000218
#define CI_REG_RESULT		0x00000220

#define CI_REG_RAMSIZE		0x000002E8
#define CI_REG_HWMAGIC		0x000002EC
#define CI_REG_HWVARIANT	0x000002F0
#define CI_REG_PERSISTENT	0x000002F4
#define CI_REG_BUTTON		0x000002F8
#define CI_REG_UPGSTATUS	0x000002FA
#define CI_REG_REV			0x000002FC


#define CI_REG_USB_CMDSTAT	0x00000400
#define CI_REG_USB_PARAM0	0x00000404
#define CI_REG_USB_PARAM1	0x00000408


#define CI_EEPROM_BASE		0x00001000
#define CI_LBAWR_BASE		0x00001800


// 
// list of all CI commands
//

#define CI_CMD_READ_SECTOR 		0x01
#define CI_CMD_READ_SECTORS		0x03
#define CI_CMD_WRITE_SECTOR		0x10
#define CI_CMD_WRITE_SECTORS	0x13
#define CI_CMD_REINIT_SD		0x1F

#define CI_CMD_SAVE_TYPE 		0xD0
#define CI_CMD_ENABLE_SWB 		0xD1
#define CI_CMD_DISABLE_SWB		0xD2

#define CI_CMD_DISABLE_SWAP		0xE0
#define CI_CMD_ENABLE_SWAP 		0xE1

#define CI_CMD_ENABLE_ROMWR		0xF0
#define CI_CMD_DISABLE_ROMWR	0xF1

#define CI_CMD_ENABLE_EXT		0xF8
#define CI_CMD_DISABLE_EXT		0xF9

#define CI_CMD_UPGRADE			0xFA
#define CI_CMD_CF_PW			0xFD

#define CI_CMD_ABORT			0xFF

// these are technically under the umbrella of CI, but
// in hardware they are handled independently of the CI commmand/wait system
//
#define CI_CMD_USB_WR 			0x08
#define CI_CMD_USB_ARM			0x0A
#define CI_CMD_USB_DISARM		0x0F


//
// status codes
//

#define CI_STAT_USB_ARM_UNARMED_IDLE	0x0
#define CI_STAT_USB_ARM_ARMED			0x1
#define CI_STAT_USB_ARM_ARMING			0xF
#define CI_STAT_USB_ARM_UNARMED_DATA	0x2

#define CI_STAT_USB_WR_IDLE				0x0
#define CI_STAT_USB_WR_BUSY				0xF

void _64Drive_usb_spin_write();
void _64Drive_putstring(char *str);
uint32_t _64Drive_wait();
void _64Drive_rom_writable(uint32_t enable);
