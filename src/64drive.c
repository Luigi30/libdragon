#include "64drive.h"

// put the debug area at the 63MB area in SDRAM
uint32_t ci_dbg_area = 0x3F00000;
uint32_t ci_dbg_area_size = 1*1024*1024;

// base address for all registers
uint32_t ci_base = CART_BASE_UNCACHED + CI_REG_BASE;

char local_buffer[512];

void _64Drive_putstring(char *str)
{
#ifndef WITH_64DRIVE
	return;
#endif

    _64Drive_usb_spin_write();	
	_64Drive_rom_writable(1);

    strcpy(local_buffer, str);

    int len_old = strlen(str) + 1;
	int len_new = len_old;
    int i;
    if(len_old % 4 != 0) {
		len_new = (len_old/4)*4 + 4;
		for(i = len_old; i < len_new; i++) local_buffer[i] = 0;
	}

    data_cache_hit_writeback_invalidate(local_buffer, 512);
	dma_write(local_buffer, CART_BASE_UNCACHED + ci_dbg_area, len_new);

	io_write(ci_base + CI_REG_USB_PARAM0, (ci_dbg_area) >> 1);
	io_write(ci_base + CI_REG_USB_PARAM1, (len_new & 0xffffff) | (0x01 << 24));
	io_write(ci_base + CI_REG_USB_CMDSTAT, CI_CMD_USB_WR);
}

uint32_t _64Drive_usb_status_write()
{
	uint32_t a;
	a = io_read(ci_base + CI_REG_USB_CMDSTAT);
	return (a >> 4) & 0xf;
}

void _64Drive_usb_spin_write()
{
	while(_64Drive_usb_status_write() != CI_STAT_USB_WR_IDLE);
}

//
// ci_wait
//
// Waits until CI operation is idle or complete.
// Arguments:
//
// Return value:
//  -1 : timed out (several seconds)
//   0 : idle
//
uint32_t _64Drive_wait()
{
	long timeout = 0;
	uint32_t ret;
	do{
		ret = io_read(ci_base + CI_REG_STATUS);
		timeout++;
		if(timeout == 4000000){
			return -1;
		}
	}while( (ret >> 8) & 0x10 );
	return 0;
}

void _64Drive_rom_writable(uint32_t enable)
{
	_64Drive_wait();
	io_write(ci_base + CI_REG_COMMAND, enable ? CI_CMD_ENABLE_ROMWR : CI_CMD_DISABLE_ROMWR);
	_64Drive_wait();
}