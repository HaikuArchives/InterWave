#ifndef IWPROTOS_H
#define IWPROTOS_H

#include <SupportDefs.h>

// iwlock.c
void iw_lock(interwave_dev *card);
void iw_unlock(interwave_dev *card);

// iwinit.c
void iw_init(interwave_dev *iw);

// iwcodec.c
void iw_enable_playback(interwave_dev *iw,bool enable);
void iw_enable_record(interwave_dev *iw,bool enable);

void iw_clear_codec_interrupts(interwave_dev *iw);

void iw_enable_mode_change(interwave_dev *iw,bool enable);

void iw_set_playback_format(interwave_dev *iw,uchar format,bool stereo,uchar clock);
void iw_get_playback_format(interwave_dev *iw,uchar *format,bool *stereo,uchar *clock);

void iw_set_record_format(interwave_dev *iw,uchar format,bool stereo,uchar clock);
void iw_get_record_format(interwave_dev *iw,uchar *format,bool *stereo,uchar *clock);

void iw_enable_playback_variable_frequency(interwave_dev *iw,bool enable);
void iw_set_playback_variable_frequency(interwave_dev *iw,uchar freq);
uchar iw_get_playback_variable_frequency(interwave_dev *iw);

// iwdma.c
status_t iw_find_low_memory(interwave_dev * card);
status_t iw_setup_dma(interwave_dev *iw);
status_t iw_start_playback_dma(interwave_dev *iw);
status_t iw_start_record_dma(interwave_dev *iw);

// iwirq.c
int32 iw_handler(void *data);

// iwmem.c
uchar iw_ram_peek_8(interwave_dev *iw,uint32 addr);
uint16 iw_ram_peek_16(interwave_dev *iw,uint32 addr);

uchar iw_rom_peek_8(interwave_dev *iw,uint32 addr);
uint16 iw_rom_peek_16(interwave_dev *iw,uint32 addr);

void iw_ram_poke_8(interwave_dev *iw,uint32 addr,uchar val);
void iw_ram_poke_16(interwave_dev *iw,uint32 addr,uint16 val);

void iw_ram_peek_block_8(interwave_dev *iw,uchar *data,uint32 len,uint32 addr);
void iw_ram_peek_block_16(interwave_dev *iw,uint16 *data,uint32 len,uint32 addr);

void iw_rom_peek_block_8(interwave_dev *iw,uchar *data,uint32 len,uint32 addr);
void iw_rom_peek_block_16(interwave_dev *iw,uint16 *data,uint32 len,uint32 addr);

void iw_ram_poke_block_8(interwave_dev *iw,const uchar *data,uint32 len,uint32 addr);
void iw_ram_poke_block_16(interwave_dev *iw,const uint16 *data,uint32 len,uint32 addr);

// iwmixer.c

void iw_set_adc_source(interwave_dev *iw,uchar left_source,uchar left_gain,uchar right_source,uchar right_gain);
void iw_get_adc_source(interwave_dev *iw,uchar *left_source,uchar *left_gain,uchar *right_source,uchar *right_gain);

void iw_set_aux1_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right);
void iw_get_aux1_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right);

void iw_set_aux2_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right);
void iw_get_aux2_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right);

void iw_set_line_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right);
void iw_get_line_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right);

void iw_set_mic_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right);
void iw_get_mic_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right);

void iw_set_dac_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right);
void iw_get_dac_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right);

void iw_set_output_gain(interwave_dev *iw,bool mute_left,uchar left,bool mute_right,uchar right);
void iw_get_output_gain(interwave_dev *iw,bool *left_muted,uchar *left,bool *right_muted,uchar *right);

void iw_set_loopback_gain(interwave_dev *iw,bool mute,uchar gain);
void iw_get_loopback_gain(interwave_dev *iw,bool *muted,uchar *gain);

void iw_set_mono_gain(interwave_dev *iw,bool mute_input,uchar input_gain,bool mute_output,bool disable_aref);
void iw_get_mono_gain(interwave_dev *iw,bool *mute_input,uchar *input_gain,bool *mute_output,bool *disable_aref);

void iw_mixer_defaults(interwave_dev *iw);

// pcm.c
#ifndef NOTDRIVER
bool iw_playback_handler(pcm_dev *pcm);
bool iw_record_handler(pcm_dev *pcm);
#endif

#endif /* IWPROTOS_H */
