#include <KernelExport.h>

#include <string.h>
#include <stdio.h>
#include <byteorder.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

int32 iw_handler(void *data)
{
	interwave_dev *iw = (interwave_dev *)data;
	bool schedule = false;
	
#if IRQDEBUG
	static uint32 count = 0;
	count++;
	if(count%50 == 0)
		iwkprintf("PONG %d",count++);
#endif

	iw_lock(iw);
	
	// * read appropriate status register
	// * dispatch interrupts
#if DO_PCM
	if(iw_peek(iw,CSR1R)&0x01) { // any codec interrupts ?
		uchar source = iw_peek(iw,CSR3I);
		
		if(source & 0x10) {      // playback interrupt
			schedule = schedule | iw_playback_handler(&iw->pcm);
		}

		if(source & 0x20) {      // record interrupt
			schedule = schedule | iw_record_handler(&iw->pcm);
		}
		
		if(source & 0x40) {      // timer interrupt
			iwprintf("mysterious timer interrupt !");
		}
		
		// * clear interrupts
		iw_clear_codec_interrupts(iw);
	}
#endif

	iw_unlock(iw);
	
	return schedule?B_INVOKE_SCHEDULER:B_HANDLED_INTERRUPT;
}
