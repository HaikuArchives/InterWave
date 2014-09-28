#include <KernelExport.h>

#include <string.h>
#include <stdio.h>
#include <byteorder.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

void iw_enable_playback(interwave_dev *iw,bool enable)
{
	iw_poke(iw,CFIG1I,(iw_peek(iw,CFIG1I)&~0x01)|(enable?1:0));
}

void iw_enable_record(interwave_dev *iw,bool enable)
{
	iw_poke(iw,CFIG1I,(iw_peek(iw,CFIG1I)&~0x02)|(enable?2:0));
}

void iw_clear_codec_interrupts(interwave_dev *iw)
{
	iw_poke(iw,CSR1R,0x00);
}

// Mode change must be enabled before using iw_set_xxx_format
// and disabled afterwards, otherwise the format will not be changed.
// Enabling mode change mutes the DAC outputs.
void iw_enable_mode_change(interwave_dev *iw,bool enable)
{
	iw_poke(iw,CIDXR,(iw_peek(iw,CIDXR)&~0x40)|(enable?0x40:0x00));
}

void iw_set_playback_format(interwave_dev *iw,uchar format,bool stereo,uchar clock)
{
	iw_poke(iw,CPDFI,(format<<5)|(stereo?0x10:0x00)|(clock&0x0f));
}

void iw_get_playback_format(interwave_dev *iw,uchar *format,bool *stereo,uchar *clock)
{
	uchar cpdfi = iw_peek(iw,CPDFI);
	
	*format = cpdfi>>5;
	*stereo = !!(cpdfi&0x10);
	*clock = cpdfi&0x0f;
}

void iw_set_record_format(interwave_dev *iw,uchar format,bool stereo,uchar clock)
{
	iw_poke(iw,CRDFI,(format<<5)|(stereo?0x10:0x00)|(clock&0x0f));
}

void iw_get_record_format(interwave_dev *iw,uchar *format,bool *stereo,uchar *clock)
{
	uchar crdfi = iw_peek(iw,CRDFI);
	
	*format = crdfi>>5;
	*stereo = !!(crdfi&0x10);
	*clock = crdfi&0x0f;
}

void iw_enable_playback_variable_frequency(interwave_dev *iw,bool enable)
{
	iw_poke(iw,CFIG3I,(iw_peek(iw,CFIG3I)&~0x04)|(enable?4:0));
}

void iw_set_playback_variable_frequency(interwave_dev *iw,uchar freq)
{
	iw_poke(iw,CPVFI,freq);
}

uchar iw_get_playback_variable_frequency(interwave_dev *iw)
{
	return iw_peek(iw,CPVFI);
}
