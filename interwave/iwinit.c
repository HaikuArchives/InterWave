#include <KernelExport.h>

#include <string.h>
#include <stdio.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

// iw_init : performs a GUS reset, then puts the card back
// in enhanced mode and enables everything that is needed
// for smooth operation, while disabling useless stuff (like
// SB emulation...)
// Does not explicitly touch the mixer settings or the contents
// of local memory. The parameters set in this function should
// not need to change later.
void iw_init(interwave_dev *iw)
{
	int voice;
	
	iwkprintf("iw_reset(%08x)",iw);

	// GUS reset first
	iw_poke(iw,URSTI,0x00);       /* Pull reset */
	spin(30);                   /* hold at least 22us */
	iw_poke(iw,URSTI,0x07);       /* Release reset, enable Synth DAC and IRQs */	
	
	// UMCR :
	// * disable MIDI loopback
	// * no synth IRQs to irq2
	// * GUS IRQ+DMA enable
	// * disable stereo mic input
	// * open ultra line-in and line-out
	iw_poke(iw,UMCR,0x08);

	// IDECI :
	//  * codec IRQs on irq1
	//  * enable irq1
	//  * disable irq2
	//  * disable NMI
	//  * enable codec ports (PCODAR)
	//  * disable AdLib (388) and SB (2X8/9/A/C/D/E) ports
	iw_poke(iw,IDECI,0x48);
	
	// UASBCI :
	//  * disable every SB/AdLib emulation feature
	iw_poke(iw,UASBCI,0x00);
	
	// CMODEI :
	//  * put codec in mode 3
	iw_poke(iw,CMODEI,CODEC_MODE3);
	
	// CFIG1I : 
	//  * select 2-channel DMA operation
	//  * disable (for now) the record and playback paths
	iw_poke(iw,CFIG1I,0x00);
	
	// CFIG2I :
	//  * full-scale output voltage 2.0/1.0 V (don't boost)
	//  * disable timer
	//  * enable sample counters
	//  * don't force 0 on playback underrun
	iw_poke(iw,CFIG2I,0x00);

	// CFIG3I : 
	//  * enable record and playback sample counter interrupts
	//  * select Synth (instead of AUX1) input
	iw_poke(iw,CFIG3I,0xc2);
	
	// CEXTI :
	// * Enable codec interrupts
	iw_poke(iw,CEXTI,0x02);
	
	// *** local memory control ***
	
	// FIXME
	// hard-coded config
	iw_poke(iw,LMCFI,(iw->rom_config << 5) | iw->ram_config);
	
	// SGMI :
	//  * put synth in enhanced mode
	//  * DISable all LFOs (they trample all over the RAM !!!)
	iw_poke(iw,SGMI,0x01);
	
	// SMSI :
	// * deactivate all voices
	for(voice=0;voice<32;voice++) {
		iw_poke(iw,SVSR,voice);
		iw_poke(iw,SMSI,0x02);
	}

}
