#include <KernelExport.h>

#include <string.h>
#include <stdio.h>
#include <byteorder.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

status_t iw_find_low_memory(interwave_dev * iw)
{
	size_t low_size = (MIN_MEMORY_SIZE+(B_PAGE_SIZE-1))&~(B_PAGE_SIZE-1);
	size_t allocate_size;
	physical_entry where;
	uint32 boundary;
	size_t trysize;
	area_id curarea;
	void * addr;
	char name[DEVNAME];

	if (low_size < MIN_MEMORY_SIZE) {
		low_size = MIN_MEMORY_SIZE;
	}
	if (low_size > 65536) {
		iwprintf("too much low memory requested !");
		low_size = 65536;
	}
	
	allocate_size = 2*low_size;

	sprintf(name, "%s_low", iw->name);
	
	curarea = find_area(name);
	if (curarea >= 0) {	/* area there from previous run */
		area_info ainfo;
		iwprintf("testing likely candidate...");
		if (get_area_info(curarea, &ainfo)) {
			iwprintf("no info");
			goto allocate;
		}
		/* test area we found */
		trysize = ainfo.size;
		addr = ainfo.address;
		if (trysize < allocate_size) {
			iwprintf("too small (%x)", trysize);
			goto allocate;
		}
		if (get_memory_map(addr, trysize, &where, 1) < B_OK) {
			iwprintf("no memory map");
			goto allocate;
		}
		if ((uint32)where.address & 0xff000000) {
			iwprintf("bad physical address");
			goto allocate;
		}
		if (ainfo.lock < B_FULL_LOCK || where.size < allocate_size) {
			iwprintf("lock not contiguous");
			goto allocate;
		}
		goto a_o_k;
	}

allocate:
	if (curarea >= 0) {
		delete_area(curarea); /* area didn't work */
		curarea = -1;
	}

	iwprintf("allocating new low area");

	trysize = allocate_size;
	curarea = create_area(name, &addr, B_ANY_KERNEL_ADDRESS, 
		trysize, B_LOMEM, B_READ_AREA | B_WRITE_AREA);
	iwprintf("create_area(%d) returned area %x at logical 0x%08x", trysize, curarea, addr);
	if (curarea < 0) {
		goto oops;
	}
	if (get_memory_map(addr, allocate_size, &where, 1) < 0) {
		delete_area(curarea);
		curarea = B_ERROR;
		goto oops;
	}
	if ((uint32)where.address & 0xff000000) {                 // does not start in low memory
		delete_area(curarea);
		curarea = B_ERROR;
		goto oops;
	}
	if (((uint32)where.address+allocate_size) & 0xff000000) { // does not end in low memory
		delete_area(curarea);
		curarea = B_ERROR;
		goto oops;
	}

oops:
	if (curarea < 0) {
		dprintf("interwave: failed to create low_mem area\n");
		return curarea;
	}
	
a_o_k:
	iwprintf("successfully found or created low area!");
	iwprintf("physical 0x%08x-0x%08x logical 0x%08x size %d", where.address, 
		where.address+trysize-1, addr, trysize);
	
	iw->low_size = low_size;
	iw->low_area = curarea;

	// The resulting double-sized area probably crosses a 64K boundary.
	// Let's change the start address so that the final, normal-sized one does not.

	// The first boundary possibly crossed
	boundary = ((uint32)where.address & 0xffff0000) + 0x00010000;
	
	// The good chunk (low_size bytes not crossing a 64K boundary) may be
	// either below or above the first boundary.
	if((boundary-(uint32)where.address) >= low_size) { // it's below, nothing to change
		iw->low_mem = (uchar *)addr;
		iw->low_phys = (vuchar *)where.address;
		
		iwprintf("current size is %d bytes",trysize);
		iwprintf("keeping %d bytes",low_size);
		if(trysize>low_size)
			resize_area(curarea,low_size);
	} else {                                           // it's above - bump up start address
		uint32 delta = boundary - (uint32)where.address;
	
		iw->low_mem = (uchar *)addr + delta;
		iw->low_phys = (vuchar *)boundary;
		
		// Unfortunately, what's below the boundary (delta bytes) is wasted.
		// We can't truncate an area's bottom.
		iwprintf("current size is %d bytes",trysize);
		iwprintf("keeping %d bytes, waste=%d",low_size+delta,delta);
		if(trysize>low_size+delta)
			resize_area(curarea,low_size+delta);
	}
	
	iwprintf("using physical 0x%08x-0x%08x logical 0x%08x size %d", iw->low_phys,
		iw->low_phys+iw->low_size-1, iw->low_mem, iw->low_size);

	return B_OK;
}

status_t iw_setup_dma(interwave_dev *iw)
{
	return B_OK;
}

status_t iw_start_playback_dma(interwave_dev *iw)
{
	uint16 sample_count;
	
	iwprintf("iw_start_playback_dma");
	
	// * stop the playback path
	iw_enable_playback(iw,false);
	
	// * clear interrupts
	iw_clear_codec_interrupts(iw);
	
	// * set the iw's sample counters to half the playback area

	// The InterWave's sample count interrupt (what we use to know a half of the
	// buffer has been played) fires when the given sample count is *exceeded*,
	// not *reached*. Hence the VITAL -1 below.
	// Also, at 16bit in stereo, a sample is 4 bytes.
	sample_count = iw->pcm.wr_size/4 - 1; // FIXME!!! (check sample size)
	iwprintf("playback sample count:%d",sample_count);
	
	iw_poke(iw,CLPCTI,sample_count & 0xff); // low byte (must be written first, per IWPG)
	iw_poke(iw,CUPCTI,sample_count >> 8);   // high byte
	
	// * start auto-init ISA DMA !

	// The buffer passed below is actually 2*pcm.wr_size bytes, which
	// converted to words (this is 16-bit DMA) yields... pcm.wr_size.
	// Be careful.
	// Also, Trial And Error(TM) determined that start_isa_dma() takes care of
	// subtracting 1 from this count (which is needed by the DMAC).
	start_dma(iw->dma2,
		(void *)(iw->low_phys + (iw->pcm.wr_1 - iw->low_mem)), // virtual->physical mapping
		(2*iw->pcm.wr_size)/2,
		0x58,                                        // upload, auto-init mode
		0);                                          // uh ?
	
	// * start the playback path
	iw_enable_playback(iw,true);
	
	return B_OK;
}

status_t iw_start_record_dma(interwave_dev *iw)
{
	uint16 sample_count;
	
	iwprintf("iw_start_record_dma");
	
	// * stop the record path
	iw_enable_record(iw,false);
	
	// * clear interrupts
	//iw_clear_codec_interrupts(iw); // FIXME: might conflict with already runing playback DMA
	
	// * set the iw's sample counters to half the record area

	// (see playback notes above)
	sample_count = iw->pcm.rd_size/4 - 1; // MUST SUBTRACT 1 // FIXME!!! (check sample size)
	iwprintf("record sample count:%d",sample_count);
	
	iw_poke(iw,CLRCTI,sample_count & 0xff); // low byte (must be written first, per IWPG)
	iw_poke(iw,CURCTI,sample_count >> 8);   // high byte
	
	// * start auto-init ISA DMA !

	// (see playback notes above)
	start_dma(iw->dma1,
		(void *)(iw->low_phys + (iw->pcm.rd_1 - iw->low_mem)), // virtual->physical mapping
		iw->pcm.rd_size,
		0x54,                                        // download, auto-init mode
		0);                                          // extended mode flags ?
	
	// * start the record path
	iw_enable_record(iw,true);
	
	return B_OK;
}
