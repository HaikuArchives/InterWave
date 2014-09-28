#ifndef INTERWAVE_H
#define INTERWAVE_H

#include <Debug.h>
#include <Drivers.h>

#include "mutex.h"

#define DO_PCM 1
#define DO_DISK 0
#define DO_MIXER 0

#if DEBUG
void iwprintf(const char *,...);
void iwkprintf(const char *,...);
#else
#define iwprintf (void)
#define iwkprintf (void)
#endif

// A few functions that are routed to the ISA module
uchar outp(uint16 port, uchar val);
uchar inp(uint16 port);

uint16 outpw(uint16 port, uint16 val);
uint16 inpw(uint16 port);

long start_dma(long channel,void *buf,long transfer_count,uchar mode,uchar e_mode);
//

#define NUM_CARDS 1
#define DEVNAME 32

#define MIN_MEMORY_SIZE 65536

typedef struct _mixer_dev {
	struct _interwave_dev *card;
	char		name[DEVNAME];
	sem_id		init_sem;
	int32		open_count;
} mixer_dev;

typedef struct _pcm_dev {
	struct _interwave_dev *card;
	char		name[DEVNAME];
	char		oldname[DEVNAME];
	sem_id		init_sem;
	sem_id		teardown_sem;
	int32		open_count;
	
	int32		wr_lock;	// spinlock protecting the playback area
	size_t		wr_size;
	vuchar *	wr_1;		// the 2 buffers, each wr_size bytes
	vuchar *	wr_2;
	vuchar *	wr_cur;		// wr_1 or wr2 : where we are writing (the "back" buffer)
	size_t		wr_silence; // how many silence bytes there are (up to wr_size*2)
	bigtime_t	wr_time;	// approximate time at which the latest buffer started playing
	uint64		wr_total;	// total bytes written

	mutex		wr_mutex;   // mutex

	size_t		was_written; // how wr_cur is full (up to wr_size)
	
	int32		rd_lock;	// spinlock protecting the record area
	size_t		rd_size;
	vuchar *	rd_1;		// the 2 buffers, each rd_size bytes
	vuchar *	rd_2;
	vuchar *	rd_cur;		// rd_1 or rd2 : where we are reading from (the "back" buffer)
	bigtime_t	rd_time;	// approximate time at which the latest buffer started playing
	uint64		rd_total;	// total bytes read

	mutex		rd_mutex;   // mutex

	size_t		was_read;	// how much of rd_cur has been consumed (up to rd_size)
	
	sem_id		old_play_sem;
	sem_id		old_cap_sem;
} pcm_dev;

typedef struct _disk_dev {
	struct _interwave_dev *card;
	char		name[DEVNAME];
	sem_id		init_sem;
	int32		open_count;
	bool		is_rom;
	uint32		size;
} disk_dev;

typedef struct _interwave_dev {
	char		name[DEVNAME];
	int32		hardware_lock;		/* spinlock */
	cpu_status	saved_cpu_status;/* when disabling interrupts */
	
	uint16		pcodar;         /* Base Port for Codec */
	uint16		p2xr;           /* Compatibility Base Port */
	uint16		p3xr;           /* MIDI and Synth Base Port */
	uint16		igidxr;         /* Gen Index Reg at P3XR+0x03 */
	uint16		i16dp;          /* 16-bit data port at P3XR+0x04 */
	uint16		i8dp;           /* 8-bit data port at P3XR+0x05 */
	uint16		svsr;           /* Synth Voice Select at P3XR+0x02 */
	uint16		cdatap;         /* Codec Indexed Data Port at PCODAR+0x01 */
	uint16		csr1r;          /* Codec Stat Reg 1 at PCODAR+0x02 */
	uint16		gmxr;           /* GMCR or GMSR at P3XR+0x00 */
	uint16		gmxdr;          /* GMTDR or GMRDR at P3XR+0x01 */
	uint16		lmbdr;          /* LMBDR at P3XR+0x07 */

	uint16		dma1;           /* DMA channel 1 (local mem/system & codec rec)*/
	uint16		dma2;           /* DMA channel 2 (codec play) */

	uint16		irq1;			/* IRQ channel 1 (everything) */
	uint16		irq2;			/* IRQ channel 2 (I don't use it) */

	uint16		ram_config;		/* Bitmask describing the DRAM banks config - see IWPG p.15-5 */
	uint16		ram_size;       /* Total RAM in Kbytes */

	uint16		rom_config;		/* Bitmask describing the ROM banks config - see IWPG p.15-4 */
	uint16		rom_size;       /* Total ROM in Kbytes */

	size_t		low_size;		/* size of low memory */
	vuchar *	low_mem;
	vuchar *	low_phys;		/* physical address */
	area_id		low_area;		/* area pointing to low memory */

	mixer_dev	mixer;
	pcm_dev		pcm;
	disk_dev	ramdisk;			/* the groovy ramdisk ! */
	disk_dev	romdisk;			/* the groovy romdisk ! */
} interwave_dev;

extern int32 num_cards;
extern interwave_dev cards[NUM_CARDS];

extern device_hooks pcm_hooks;
extern device_hooks mixer_hooks;
extern device_hooks disk_hooks;

#endif /* INTERWAVE_H */
