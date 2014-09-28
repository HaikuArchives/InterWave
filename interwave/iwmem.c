#include <KernelExport.h>

#include <string.h>
#include <stdio.h>
#include <byteorder.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

// set LMAHI,LMALI to a local address
static inline void iw_address(interwave_dev *iw,uint32 addr)
{
	iw_poke(iw,LMALI,(uint16)addr);
	iw_poke(iw,LMAHI,(uint8)(addr>>16));
}

uchar iw_ram_peek_8(interwave_dev *iw,uint32 addr)
{
	iw_address(iw,addr);
	return iw_peek(iw,LMBDR);
}

uint16 iw_ram_peek_16(interwave_dev *iw,uint32 addr)
{
	iw_address(iw,addr);
	return iw_peek(iw,LMSBAI);
}

uchar iw_rom_peek_8(interwave_dev *iw,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	uchar val;
	
	iw_poke(iw,LMCI,lmci|ROM_IO); // select ROM
	
	iw_address(iw,addr);
	val = iw_peek(iw,LMBDR);

	iw_poke(iw,LMCI,lmci); // restore LMCI
	
	return val;
}

uint16 iw_rom_peek_16(interwave_dev *iw,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	uint16 val;

	iw_poke(iw,LMCI,lmci|ROM_IO); // select ROM
	
	iw_address(iw,addr);
	val = iw_peek(iw,LMSBAI);

	iw_poke(iw,LMCI,lmci); // restore LMCI
	
	return val;
}

void iw_ram_poke_8(interwave_dev *iw,uint32 addr,uchar val)
{
	iw_address(iw,addr);
	iw_poke(iw,LMBDR,val);
}

void iw_ram_poke_16(interwave_dev *iw,uint32 addr,uint16 val)
{
	iw_address(iw,addr);
	iw_poke(iw,LMSBAI,val);
}

void iw_ram_peek_block_8(interwave_dev *iw,uchar *data,uint32 len,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	
	// select DRAM, auto-increment
	iw_poke(iw,LMCI,(lmci|AUTOI)&DRAM_IO);
	
	iw_address(iw,addr);
	
	while(len--)
		*data++ = iw_peek(iw,LMBDR);

	iw_poke(iw,LMCI,lmci); // restore LMCI
}

void iw_ram_peek_block_16(interwave_dev *iw,uint16 *data,uint32 len,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	
	// select DRAM, auto-increment
	iw_poke(iw,LMCI,(lmci|AUTOI)&DRAM_IO);
	
	iw_address(iw,addr);

	// select LMBSAI only once, for speed
	iw_poke(iw,IGIDXR,_LMSBAI);
	while(len--)
		*data++ = iw_peek(iw,I16DP);

	iw_poke(iw,LMCI,lmci); // restore LMCI
}

void iw_rom_peek_block_8(interwave_dev *iw,uchar *data,uint32 len,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	
	// select ROM, auto-increment
	iw_poke(iw,LMCI,(lmci|AUTOI)|ROM_IO);
	
	iw_address(iw,addr);
	
	while(len--)
		*data++ = iw_peek(iw,LMBDR);

	iw_poke(iw,LMCI,lmci); // restore LMCI
}

void iw_rom_peek_block_16(interwave_dev *iw,uint16 *data,uint32 len,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	
	// select ROM, auto-increment
	iw_poke(iw,LMCI,(lmci|AUTOI)|ROM_IO);
	
	iw_address(iw,addr);

	// select LMBSAI only once, for speed
	iw_poke(iw,IGIDXR,_LMSBAI);
	while(len--)
		*data++ = iw_peek(iw,I16DP);

	iw_poke(iw,LMCI,lmci); // restore LMCI
}

void iw_ram_poke_block_8(interwave_dev *iw,const uchar *data,uint32 len,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	
	// select DRAM, auto-increment
	iw_poke(iw,LMCI,(lmci|AUTOI)&DRAM_IO);
	
	iw_address(iw,addr);
	
	while(len--)
		iw_poke(iw,LMBDR,*data++);

	iw_poke(iw,LMCI,lmci); // restore LMCI
}

void iw_ram_poke_block_16(interwave_dev *iw,const uint16 *data,uint32 len,uint32 addr)
{
	uchar lmci = iw_peek(iw,LMCI); // save LMCI
	
	// select DRAM, auto-increment
	iw_poke(iw,LMCI,(lmci|AUTOI)&DRAM_IO);
	
	iw_address(iw,addr);

	// select LMSBAI only once, for speed
	iw_poke(iw,IGIDXR,_LMSBAI);
	while(len--)
		iw_poke(iw,I16DP,*data++);

	iw_poke(iw,LMCI,lmci); // restore LMCI
}
