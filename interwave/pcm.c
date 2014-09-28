#include <KernelExport.h>

#include <string.h>
#include <stdio.h>
#include <byteorder.h>
#include "sound.h"
#include "R3MediaDefs.h"

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

#define IRQDEBUG 0

static void write_silence(pcm_dev *pcm,volatile uchar *where,size_t count);

static status_t pcm_open(const char *name, uint32 flags, void **cookie);
static status_t pcm_close(void *cookie);
static status_t pcm_free(void *cookie);
static status_t pcm_control(void *cookie, uint32 op, void *data, size_t len);
static status_t pcm_read(void *cookie, off_t pos, void *data, size_t *len);
static status_t pcm_write(void *cookie, off_t pos, const void *data, size_t *len);

device_hooks pcm_hooks = {
    &pcm_open,
    &pcm_close,
    &pcm_free,
    &pcm_control,
    &pcm_read,
    &pcm_write,
    NULL,			/* select */
    NULL,			/* deselect */
    NULL,			/* readv */
	NULL			/* writev */
};

static status_t
pcm_open(
	const char * name,
	uint32 flags,
	void ** cookie)
{
	int ix;
	status_t err;
	pcm_dev * pcm = NULL;
	char sem_name[DEVNAME];
	int prev_count;
	
#if DEBUG
	thread_id thid;
	thread_info thinfo;
	thid = find_thread(NULL);
	get_thread_info(thid,&thinfo);
	iwprintf("pcm_open(%s), thid=%d(%s from team %d)",
		name,thid,thinfo.name,thinfo.team);
#endif

	*cookie = NULL;
	for (ix=0; ix<num_cards; ix++) {
		if (!strcmp(name, cards[ix].pcm.name)) {
			pcm = &cards[ix].pcm;
			break;
		}
		if (!strcmp(name, cards[ix].pcm.oldname)) {
			pcm = &cards[ix].pcm;
			break;
		}
	}
	if (ix == num_cards) {
		iwprintf("interwave: %s not found",name);
		return ENODEV;
	}

	*cookie = pcm;

	iwprintf("pcm_dev *pcm=0x%08x",pcm);

	if((prev_count=atomic_add(&pcm->open_count, 1))==0) { // first client
		char name_buf[256];
		interwave_dev *iw;
	
		// a close() might be in progress; let it finish first
		if (acquire_sem(pcm->teardown_sem) < B_OK)
			return B_ERROR;
		release_sem(pcm->teardown_sem);
	
		pcm->card = iw = &cards[ix];
		
		// * setup the pcm_dev (init pointers to buffers, create semaphores)

		// ** setup for playback (writing)
		pcm->wr_lock = 0;

		pcm->wr_size = 2048; // FIXME (config.play_buf_size/2)
		pcm->wr_1 = pcm->card->low_mem;
		pcm->wr_2 = pcm->wr_1 + pcm->wr_size;

		pcm->wr_cur = pcm->wr_2;
		
		write_silence(pcm,pcm->wr_1,pcm->wr_size*2);
		
		pcm->wr_silence = pcm->wr_size*2;
		pcm->was_written = 0;
		
		pcm->wr_total = 0;
		pcm->wr_time = system_time();

		sprintf(name_buf, "WQ:%s", pcm->name);
		name_buf[B_OS_NAME_LENGTH-1] = 0;
		if ((err = mutex_create(&pcm->wr_mutex, name_buf)) < B_OK) {
			pcm->open_count = 0;
			return err;
		}
		
		pcm->old_play_sem = -1;
		
		// ** setup for record (reading)
		pcm->rd_lock = 0;

		pcm->rd_size = 8192; // FIXME (config.rec_buf_size/2)
		pcm->rd_1 = pcm->wr_2 + pcm->wr_size;
		pcm->rd_2 = pcm->rd_1 + pcm->rd_size;

		pcm->rd_cur = pcm->rd_2;
		
		write_silence(pcm,pcm->rd_1,pcm->rd_size*2);

		pcm->was_read = 0;
		
		pcm->rd_total = 0;
		pcm->rd_time = system_time();

		sprintf(name_buf, "RQ:%s", pcm->name);
		name_buf[B_OS_NAME_LENGTH-1] = 0;
		if ((err = mutex_create(&pcm->rd_mutex, name_buf)) < B_OK) {
			pcm->open_count = 0;
			return err;
		}
		
		pcm->old_cap_sem = -1;
		
		// * configure default playback mode
		iw_enable_playback(iw,false); // just in case
		
		iw_enable_mode_change(iw,true);
		
		iw_set_playback_format(iw,IW_DATA_16LE,true,IW_CLOCK_44100);
		iw_set_record_format(iw,IW_DATA_16LE,true,IW_CLOCK_44100);
		
		iw_enable_mode_change(iw,false);
		
		// * get DMA running
		iw_start_playback_dma(iw);
		iw_start_record_dma(iw);

		// * let other clients begin (release init_sem)
		release_sem(pcm->init_sem);
	} else {
		if (acquire_sem(pcm->init_sem) < B_OK)
			return B_ERROR;
		release_sem(pcm->init_sem);
	}
	
	iwprintf("pcm open count:%d",prev_count+1);
	
	return B_OK;
}

static status_t
pcm_close(
	void * cookie)
{
	pcm_dev *pcm = (pcm_dev *)cookie;
	int prev_count;

	iwprintf("pcm_close");
	
	// We must ensure that nobody tries to re-open the device while
	// we are tearing it down (they must wait until it is done).
	if (acquire_sem(pcm->teardown_sem) < B_OK)
		return B_ERROR;

	if((prev_count=atomic_add(&pcm->open_count, -1))==1) { // last client
		iw_lock(pcm->card);
		// TODO:
		// * stop any DMA activity and clear any pending interrupt
		// * fill playback area with silence
		iw_unlock(pcm->card);
		
		// * teardown the pcm_dev (delete semaphores)
		mutex_delete(&pcm->wr_mutex);
		mutex_delete(&pcm->rd_mutex);

		pcm->old_play_sem = -1;
		pcm->old_cap_sem = -1;
		
		// * acquire init_sem
		if (acquire_sem(pcm->init_sem) < B_OK)
			return B_ERROR;
	}
	iwprintf("pcm open count:%d",prev_count-1);
	
	release_sem(pcm->teardown_sem);

	iwprintf("pcm_close:end.");
	
	return B_OK;
}

static status_t
pcm_free(
	void * cookie)
{
	pcm_dev *pcm = (pcm_dev *)cookie;

	iwprintf("pcm_free");
	
	if(pcm->open_count == 0) {
		iw_lock(pcm->card);
		// TODO:
		// * "remove" the interrupt handler ? (decrement its usage count)
		iw_unlock(pcm->card);
	}
	
	return B_OK;
}

static status_t
pcm_control(
	void * cookie,
	uint32 iop,
	void * data,
	size_t len)
{
	pcm_dev *pcm = (pcm_dev *)cookie;
	interwave_dev *iw = pcm->card;
	status_t err = B_BAD_VALUE;
	uchar real_adc_source;
	enum adc_source swapped_adc_source;

	switch(iop) {
	case SOUND_GET_PARAMS:{
		sound_setup * sound = (sound_setup *)data;
		bool b,left_mute,right_mute;

		iwprintf("SOUND_GET_PARAMS");

		err = B_OK;
		
		iw_lock(iw);
		
		sound->sample_rate = kHz_44_1;
		
		sound->playback_format = linear_16bit_big_endian_stereo;
		sound->capture_format = linear_16bit_big_endian_stereo;
		
		sound->dither_enable = false;
		
		iw_get_loopback_gain(iw,&b,&(sound->loop_attn));
		sound->loop_enable = !b;
		
		sound->output_boost = 0;
		
		sound->highpass_enable = 0;
		
		sound->mono_gain = 0;
		iw_get_mono_gain(iw,NULL,NULL,&b,NULL);
		sound->mono_mute = b;
		
		iw_get_adc_source(iw,&real_adc_source,(uchar *)&(sound->left.adc_gain),
			&real_adc_source,(uchar *)&(sound->right.adc_gain));

		// NOTE : the old API expects the CD to be connected to AUX1.
		// But the synth is connected to AUX1 on the Interwave, so we
		// swap aux1 and aux2 below. On the GUS PnP, the CD goes to AUX2.
		// Unfortunately, this means the CD can not be an ADC source by
		// itself.

		switch(real_adc_source) {
		case IW_SOURCE_MIC:
			swapped_adc_source = mic;
			break;
		case IW_SOURCE_AUX1:
			swapped_adc_source = loopback; //aux1; // probably wrong
			break;
		case IW_SOURCE_MIXER:
			swapped_adc_source = loopback;
			break;
		case IW_SOURCE_LINE:
		default:
			swapped_adc_source = line;
			break;
		}
		
		iwprintf("get adc source : iw %d -> %d, gain %d",real_adc_source,
			swapped_adc_source,
			sound->left.adc_gain);
			
		sound->left.adc_source = sound->right.adc_source = swapped_adc_source;
		
		sound->left.mic_gain_enable = 0;
		sound->right.mic_gain_enable = 0;
		
		iw_get_aux2_gain(iw,&left_mute,(uchar *)&(sound->left.aux1_mix_gain),
			&right_mute,(uchar *)&(sound->right.aux1_mix_gain));

		sound->left.aux1_mix_mute = left_mute;
		sound->right.aux1_mix_mute = right_mute;

		iw_get_aux1_gain(iw,&left_mute,(uchar *)&(sound->left.aux2_mix_gain),
			&right_mute,(uchar *)&(sound->right.aux2_mix_gain));

		sound->left.aux2_mix_mute = left_mute;
		sound->right.aux2_mix_mute = right_mute;

		iw_get_line_gain(iw,&left_mute,(uchar *)&(sound->left.line_mix_gain),
			&right_mute,(uchar *)&(sound->right.line_mix_gain));

		sound->left.line_mix_mute = left_mute;
		sound->right.line_mix_mute = right_mute;

		iw_get_dac_gain(iw,&left_mute,(uchar *)&(sound->left.dac_attn),
			&right_mute,(uchar *)&(sound->right.dac_attn));

		sound->left.dac_mute = left_mute;
		sound->right.dac_mute = right_mute;

		iw_unlock(iw);
		break;
		}
	case SOUND_SET_PARAMS:{
		sound_setup * sound = (sound_setup *)data;

		iwprintf("SOUND_SET_PARAMS");

		err = B_OK;
		
		iw_lock(iw);
		
		// FIXME look at the format/rate...?

		iw_set_loopback_gain(iw,!sound->loop_enable,sound->loop_attn);

		iw_set_mono_gain(iw,true,0,sound->mono_mute,0);
		
		switch(sound->left.adc_source) {
		case mic:
			real_adc_source = IW_SOURCE_MIC;
			break;
		case loopback:
			real_adc_source = IW_SOURCE_MIXER;
			break;
		case aux1:
			real_adc_source = IW_SOURCE_MIXER; //AUX1; // hmm...
			break;
		case line:
		default:
			real_adc_source = IW_SOURCE_LINE;
			break;
		}
		
		iwprintf("set adc source : %d -> iw %d, gain %d",sound->left.adc_source,
			real_adc_source,sound->left.adc_gain);
		
		iw_set_adc_source(iw,real_adc_source,sound->left.adc_gain,
			real_adc_source,sound->right.adc_gain);

		// see NOTE above about aux1 and aux2 being swapped
		
		iw_set_aux2_gain(iw,sound->left.aux1_mix_mute,sound->left.aux1_mix_gain,
			sound->right.aux1_mix_mute,sound->right.aux1_mix_gain);

		// aux1 is the synth and we don't use it, so force it muted 
		// to reduce noise
		// iw_set_aux1_gain(iw,sound->left.aux2_mix_mute,sound->left.aux2_mix_gain,
		// 	sound->right.aux2_mix_mute,sound->right.aux2_mix_gain);
		iw_set_aux1_gain(iw,true,0,true,0);

		iw_set_line_gain(iw,sound->left.line_mix_mute,sound->left.line_mix_gain,
			sound->right.line_mix_mute,sound->right.line_mix_gain);

		iw_set_dac_gain(iw,sound->left.dac_mute,sound->left.dac_attn,
			sound->right.dac_mute,sound->right.dac_attn);
			
		iw_unlock(iw);
		break;
		}
	case SOUND_SET_PLAYBACK_COMPLETION_SEM:
		iwprintf("SOUND_SET_PLAYBACK_COMPLETION_SEM");
		pcm->old_play_sem = *(sem_id *)data;
		err = B_OK;
		break;
	case SOUND_SET_CAPTURE_COMPLETION_SEM:
		iwprintf("SOUND_SET_CAPTURE_COMPLETION_SEM");
		pcm->old_cap_sem = *(sem_id *)data;
		err = B_OK;
		break;
	case SOUND_UNSAFE_WRITE: {
		audio_buffer_header * buf = (audio_buffer_header *)data;
		size_t n = buf->reserved_1-sizeof(*buf);

		pcm_write(cookie, 0, buf+1, &n);
		buf->time = pcm->wr_time;
		buf->sample_clock = pcm->wr_total/4 * 10000 / 441;
		err = release_sem(pcm->old_play_sem);
		break;
		}
	case SOUND_UNSAFE_READ: {
		audio_buffer_header * buf = (audio_buffer_header *)data;
		size_t n = buf->reserved_1-sizeof(*buf);

		pcm_read(cookie, 0, buf+1, &n);
		buf->time = pcm->rd_time;
		buf->sample_clock = pcm->rd_total/4 * 10000 / 441;
		err = release_sem(pcm->old_cap_sem);
		break;
		}
	}

	return err;
}

static status_t
pcm_write(
	void * cookie,
	off_t pos,
	const void * data,
	size_t * nwritten)
{
	pcm_dev *pcm = (pcm_dev *)cookie;
	status_t err;
	cpu_status cp;
	int written_so_far = 0;
	int to_write = *nwritten;	/*	 in play bytes, not input bytes!	*/
	int available_block;
	int bytes_xferred;
	vuchar *wr_dest;

//	iwprintf("pcm_write(%Ld,%d)",pos,to_write);
	
	*nwritten = 0;
	
	// FIXME
//	if (pcm->config.format == 0x24) {
//		to_write >>= 1;	/*	floats collapse by 2	*/
//	}
	
	while (to_write > 0) {

		// * wait for some space to become available
//		iwprintf("waiting for the mutex...");
		if ((err = mutex_acquire(&pcm->wr_mutex)) < B_OK) {
			return err;
		}
//		iwprintf("got the mutex !");
		
		cp = disable_interrupts();
		acquire_spinlock(&pcm->wr_lock);
		
		available_block = pcm->wr_size - pcm->was_written;
		if (available_block > to_write) {
			// must let next guy in
			mutex_release(&pcm->wr_mutex);
			available_block = to_write;
		}
		
		// * remember where to write
//		iwkprintf("writing %d bytes... at wr_%d+%d",
//			available_block,
//			(pcm->wr_cur==pcm->wr_1)?1:2,pcm->was_written);
		wr_dest = pcm->wr_cur + pcm->was_written;
		bytes_xferred = available_block;
		
		pcm->was_written += available_block;
		pcm->wr_silence = 0;
		
		release_spinlock(&pcm->wr_lock);
		restore_interrupts(cp);
		
		// * write, with interrupts restored
		memcpy((uchar *)wr_dest, data, available_block);

		data = ((const char *)data)+bytes_xferred;
		written_so_far += bytes_xferred;
		to_write -= available_block;
	}
	
	*nwritten = written_so_far;
	
	return B_OK;
}

static status_t
pcm_read(
	void * cookie,
	off_t pos,
	void * data,
	size_t * nread)
{
	pcm_dev *pcm = (pcm_dev *)cookie;
	status_t err;
	cpu_status cp;
	int read_so_far = 0;
	int to_read = *nread;	/*	 in play bytes, not input bytes!	*/
	int available_block;
	int bytes_xferred;
	vuchar *rd_src;

//	iwprintf("pcm_read(%Ld,%d)",pos,to_read);
	
	*nread = 0;
	
	// FIXME:format
	
	while (to_read > 0) {

		// * wait for some data to become available
//		iwprintf("waiting for the mutex...");
		if ((err = mutex_acquire(&pcm->rd_mutex)) < B_OK) {
			return err;
		}
//		iwprintf("got the mutex !");
		
		cp = disable_interrupts();
		acquire_spinlock(&pcm->rd_lock);
		
		available_block = pcm->rd_size - pcm->was_read;
		if (available_block > to_read) {
			// must let next guy in
			mutex_release(&pcm->rd_mutex);
			available_block = to_read;
		}
		
		// * remember where to read from
//		iwkprintf("reading %d bytes... from rd_%d+%d",
//			available_block,
//			(pcm->rd_cur==pcm->rd_1)?1:2,pcm->was_read);
		rd_src = pcm->rd_cur + pcm->was_read;
		bytes_xferred = available_block;
		
		pcm->was_read += available_block;
		
		release_spinlock(&pcm->rd_lock);
		restore_interrupts(cp);
		
		// * read, with interrupts restored
		memcpy(data,(uchar *)rd_src, available_block);

		data = (char *)data+bytes_xferred;
		read_so_far += bytes_xferred;
		to_read -= available_block;
	}
	
	*nread = read_so_far;
	
	return B_OK;
}

static void write_silence(pcm_dev *pcm,vuchar *where,size_t count)
{
#if 0 // FIXME
	memset(where, (pcm->config.format == 0x11)?0x80:0, count);
#else
	memset((uchar *)where, 0x00, count);
#endif
}

// Called from the top-level interrupt handler
// Thus, runs with interrupts disabled and the iw master spinlock held.
bool iw_playback_handler(pcm_dev *pcm)
{
	bool ret = false;
	bigtime_t st = system_time();
	
	acquire_spinlock(&pcm->wr_lock);
	
	if (pcm->wr_mutex.sem < 0) { // means nobody has the device open
		iwkprintf("spurious playback DMA interrupt!");
		release_spinlock(&pcm->wr_lock);
		return false;
	}
	
	// * fill wr_cur with silence if it is not full
	if (pcm->was_written > 0 && pcm->was_written < pcm->wr_size) {
		write_silence(pcm, pcm->wr_cur+pcm->was_written, pcm->wr_size-pcm->was_written);
	}
	
	// * switch buffers (wr_cur = wr_1/2)
	pcm->wr_cur = (pcm->wr_cur==pcm->wr_1)?pcm->wr_2:pcm->wr_1;
	pcm->was_written = 0; // yeah, of course

	pcm->wr_total += pcm->wr_size;
	
	// * compensate for interrupt latency ??? TODO ?
	// (can't do because I don't know where I am
	//  since I can't poll the DMAC)
	pcm->wr_time = st;
	
	// * if a client is waiting release him
	if(mutex_release(&pcm->wr_mutex)) {
//		kprintf(".");
		ret = true;
	} else {
		// sadly, no-one is here to fill my buffer... enjoy the silence !
#if IRQDEBUG
		kprintf("!");
#endif
		if(pcm->wr_silence < (pcm->wr_size*2)) {
			write_silence(pcm,pcm->wr_cur,pcm->wr_size);
			pcm->wr_silence += pcm->wr_size;
		}
	}
	
//	iwkprintf("playing wr_%d",(pcm->wr_cur==pcm->wr_1)?2:1);

	release_spinlock(&pcm->wr_lock);
	
	return ret;
}

// Called from the top-level interrupt handler
// Thus, runs with interrupts disabled and the iw master spinlock held.
// Happens when the card has just finished filling a record buffer
// (the one that is NOT pcm->wr_cur)
bool iw_record_handler(pcm_dev *pcm)
{
	bool ret = false;
	bigtime_t st = system_time();
	
	acquire_spinlock(&pcm->rd_lock);
	
	if (pcm->rd_mutex.sem < 0) { // means nobody has the device open
		iwkprintf("spurious record DMA interrupt!");
		release_spinlock(&pcm->rd_lock);
		return false;
	}
	
	// * switch buffers (rd_cur = rd_1/2)
	pcm->rd_cur = (pcm->rd_cur==pcm->rd_1)?pcm->rd_2:pcm->rd_1;
	pcm->was_read = 0; // yeah, of course

	pcm->rd_total += pcm->rd_size;
	
	// * compensate for interrupt latency ??? TODO ?
	// (can't do because I don't know where I am
	//  since I can't poll the DMAC)
	pcm->rd_time = st;
	
	// * if a client is waiting release him
	if(mutex_release(&pcm->rd_mutex)) {
//		kprintf(".");
		ret = true;
	} else {
//		kprintf("!");
	}
	
	release_spinlock(&pcm->rd_lock);
	
	return ret;
}
