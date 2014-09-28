/*
 * InterWave mixer controls.
 * See p. 6.17 of the Programmer's Guide.
 * Beware of the various scales
 * (-34.5dB..+12dB, -94.5dB..+0dB, -45dB..+0dB).
 */

#include <KernelExport.h>

#include <string.h>
#include <stdio.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

#define RETVAL(ptr,val) if(ptr) *ptr=val

// select the input to the ADC :
// IW_SOURCE_xxx
// and the ADC gain :
// 0x00 = 0dB, 0x0f = +22.5dB (+1.5dB per step)
void iw_set_adc_source(interwave_dev *iw,uchar left_source,uchar left_gain,uchar right_source,uchar right_gain)
{
	iw_poke(iw,CLICI,(left_gain&0x3f)|(left_source<<6));
	iw_poke(iw,CRICI,(right_gain&0x3f)|(right_source<<6));
}

void iw_get_adc_source(interwave_dev *iw,uchar *left_source,uchar *left_gain,uchar *right_source,uchar *right_gain)
{
	uchar clici = iw_peek(iw,CLICI);
	uchar crici = iw_peek(iw,CRICI);
	
	RETVAL(left_source, (clici>>6)&0x03);
	RETVAL(left_gain, clici&0x0f);
	RETVAL(right_source, (crici>>6)&0x03);
	RETVAL(right_gain, crici&0x0f);
}

// set Aux1/Synth gain :
// 0x00 = +12dB, 0x1f = -34.5dB (-1.5dB per step)
void iw_set_aux1_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right)
{
	iw_poke(iw,CLAX1I,(left&0x1f)|(mute_left?0x80:0x00));
	iw_poke(iw,CRAX1I,(right&0x1f)|(mute_right?0x80:0x00));
}

void iw_get_aux1_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right)
{
	uchar clax1i = iw_peek(iw,CLAX1I);
	uchar crax1i = iw_peek(iw,CRAX1I);
	
	RETVAL(left_muted, (clax1i&0x80)?true:false);
	RETVAL(left, clax1i&0x1f);
	RETVAL(right_muted, (crax1i&0x80)?true:false);
	RETVAL(right, crax1i&0x1f);
}

// set Aux2(CD) gain :
// 0x00 = +12dB, 0x1f = -34.5dB (-1.5dB per step)
void iw_set_aux2_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right)
{
	iw_poke(iw,CLAX2I,(left&0x1f)|(mute_left?0x80:0x00));
	iw_poke(iw,CRAX2I,(right&0x1f)|(mute_right?0x80:0x00));
}

void iw_get_aux2_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right)
{
	uchar clax2i = iw_peek(iw,CLAX2I);
	uchar crax2i = iw_peek(iw,CRAX2I);
	
	RETVAL(left_muted, (clax2i&0x80)?true:false);
	RETVAL(left, clax2i&0x1f);
	RETVAL(right_muted, (crax2i&0x80)?true:false);
	RETVAL(right, crax2i&0x1f);
}

// set line input gain :
// 0x00 = +12dB, 0x1f = -34.5dB (-1.5dB per step)
void iw_set_line_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right)
{
	iw_poke(iw,CLLICI,(left&0x1f)|(mute_left?0x80:0x00));
	iw_poke(iw,CRLICI,(right&0x1f)|(mute_right?0x80:0x00));
}

void iw_get_line_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right)
{
	uchar cllici = iw_peek(iw,CLLICI);
	uchar crlici = iw_peek(iw,CRLICI);
	
	RETVAL(left_muted, (cllici&0x80)?true:false);
	RETVAL(left, cllici&0x1f);
	RETVAL(right_muted, (crlici&0x80)?true:false);
	RETVAL(right, crlici&0x1f);
}

// set (stereo) mic gain :
// 0x00 = +12dB, 0x1f = -34.5dB (-1.5dB per step)
void iw_set_mic_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right)
{
	iw_poke(iw,CLMICI,(left&0x1f)|(mute_left?0x80:0x00));
	iw_poke(iw,CRMICI,(right&0x1f)|(mute_right?0x80:0x00));
}

void iw_get_mic_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right)
{
	uchar clmici = iw_peek(iw,CLMICI);
	uchar crmici = iw_peek(iw,CRMICI);
	
	RETVAL(left_muted, (clmici&0x80)?true:false);
	RETVAL(left, clmici&0x1f);
	RETVAL(right_muted, (crmici&0x80)?true:false);
	RETVAL(right, crmici&0x1f);
}

// set DAC gain :
// 0x00 = 0dB, 0x3f = -94.5dB (-1.5dB per step)
void iw_set_dac_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right)
{
	iw_poke(iw,CLDACI,(left&0x3f)|(mute_left?0x80:0x00));
	iw_poke(iw,CRDACI,(right&0x3f)|(mute_right?0x80:0x00));
}

void iw_get_dac_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right)
{
	uchar cldaci = iw_peek(iw,CLDACI);
	uchar crdaci = iw_peek(iw,CRDACI);
	
	RETVAL(left_muted, (cldaci&0x80)?true:false);
	RETVAL(left, cldaci&0x3f);
	RETVAL(right_muted, (crdaci&0x80)?true:false);
	RETVAL(right, crdaci&0x3f);
}

// set output (master) gain :
// 0x00 = 0dB, 0x1f = -46.5dB (-1.5dB per step)
void iw_set_output_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right)
{
	iw_poke(iw,CLOAI,(left&0x1f)|(mute_left?0x80:0x00));
	iw_poke(iw,CROAI,(right&0x1f)|(mute_right?0x80:0x00));
}

void iw_get_output_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right)
{
	uchar cloai = iw_peek(iw,CLOAI);
	uchar croai = iw_peek(iw,CROAI);
	
	RETVAL(left_muted, (cloai&0x80)?true:false);
	RETVAL(left, cloai&0x1f);
	RETVAL(right_muted, (croai&0x80)?true:false);
	RETVAL(right, croai&0x1f);
}

// set loopback gain :
// 0x00 = 0dB, 0x1f = -94.5dB (-1.5dB per step)
void iw_set_loopback_gain(interwave_dev *iw,bool mute,uchar gain)
{
	// NOTE: gain is shifted left, mute is bit 0 and reversed (muted when low)
	iw_poke(iw,CLCI,((gain&0x3f)<<2)|(mute?0x00:0x01));
}

void iw_get_loopback_gain(interwave_dev *iw,bool *muted,uchar *gain)
{
	uchar clci = iw_peek(iw,CLCI);
	
	RETVAL(muted, (clci&0x01)?false:true);
	RETVAL(gain, (clci>>2)&0x3f);
}

// set mono input gain :
// 0x00 = 0dB, 0x0f = -45dB (-3.0dB per step)
// can also mute output, and set AREF (whatever that is) to high-impedance (disabled)
void iw_set_mono_gain(interwave_dev *iw,bool mute_input,uchar input_gain,bool mute_output,bool disable_aref)
{
	iw_poke(iw,CMONOI,(mute_input?0x80:0x00)|(mute_output?0x40:0x00)|(disable_aref?0x20:0x00)
		|(input_gain&0x0f));
}

void iw_get_mono_gain(interwave_dev *iw,bool *mute_input,uchar *input_gain,bool *mute_output,bool *disable_aref)
{
	uchar cmonoi = iw_peek(iw,CMONOI);
	
	RETVAL(mute_input,(cmonoi&0x80)?true:false);
	RETVAL(input_gain,cmonoi&0x0f);
	RETVAL(mute_output,(cmonoi&0x40)?true:false);
	RETVAL(disable_aref,(cmonoi&0x20)?true:false);
}

void iw_mixer_defaults(interwave_dev *iw)
{
	// mute line-in
	iw_set_line_gain(iw,true,0x08,true,0x08);
	
	// mute mic
	iw_set_mic_gain(iw,true,0x08,true,0x08);
	
	// mute mono
	iw_set_mono_gain(iw,true,0x00,true,false);
	
	// mute loopback
	iw_set_loopback_gain(iw,true,0x00);

	// enable synth-in (at +0dB)
	iw_set_aux1_gain(iw,false,0x08,false,0x08);
	
	// enable cd-in (at +0dB)
	iw_set_aux2_gain(iw,false,0x08,false,0x08);

	// enable dac-in (at +0dB)
	iw_set_dac_gain(iw,false,0x00,false,0x00);
	
	//iw_poke(iw,CFIG2I,iw_peek(iw,CFIG2I)|0x80); // 1.3 V boost
	
	// enable line-out (at +0dB)
	iw_set_output_gain(iw,false,0x00,false,0x00);
}
