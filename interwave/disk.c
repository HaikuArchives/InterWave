#include <KernelExport.h>
#include <SupportDefs.h>
#include <string.h>

#include "interwave.h"
#include "iwprotos.h"

static status_t disk_open(const char *name, uint32 flags, void **cookie);
static status_t disk_close(void *cookie);
static status_t disk_free(void *cookie);
static status_t disk_control(void *cookie, uint32 op, void *data, size_t len);
static status_t disk_read(void *cookie, off_t pos, void *data, size_t *len);
static status_t disk_write(void *cookie, off_t pos, const void *data, size_t *len);

device_hooks disk_hooks = {
    &disk_open,
    &disk_close,
    &disk_free,
    &disk_control,
    &disk_read,
    &disk_write,
    NULL,		/* select */
    NULL,		/* deselect */
    NULL,		/* readv */
    NULL		/* writev */
};

static status_t disk_open(
	const char * name,
	uint32 flags,
	void ** cookie)
{
	int ix;
	disk_dev * disk = NULL;
	char sem_name[DEVNAME];
	int prev_count;
	
#if DEBUG
	thread_id thid;
	thread_info thinfo;
	thid = find_thread(NULL);
	get_thread_info(thid,&thinfo);
	iwprintf("disk_open(%s), thid=%d(%s from team %d)",
		name,thid,
		thinfo.name,thinfo.team);
#endif

	*cookie = NULL;
	for (ix=0; ix<num_cards; ix++) {
		if (!strcmp(name, cards[ix].ramdisk.name)) {
			disk = &cards[ix].ramdisk;
			break;
		}
		if (!strcmp(name, cards[ix].romdisk.name)) {
			disk = &cards[ix].romdisk;
			break;
		}
	}
	if (ix == num_cards) {
		iwprintf("interwave: %s not found",name);
		return ENODEV;
	}

	*cookie = disk;

	iwprintf("disk_dev *disk=%08x",disk);

	if((prev_count=atomic_add(&disk->open_count, 1))==0) { // first client
		disk->card = &cards[ix];
		release_sem(disk->init_sem);
	} else {
		if (acquire_sem(disk->init_sem) < B_OK)
			return B_ERROR;
		release_sem(disk->init_sem);
	}

	iwprintf("disk open count:%d",prev_count+1);
	
	return B_OK;
}

static status_t disk_close(
	void *cookie)
{
	disk_dev *disk = (disk_dev *)cookie;
	int prev_count;

	iwprintf("disk_close");

	if((prev_count=atomic_add(&disk->open_count, -1))==1) { // last client
		/* uh, nothing to do yet */
	} else {
	}

	iwprintf("disk open count:%d",prev_count-1);
	
	return B_OK;
}

static status_t disk_free(
	void *cookie)
{
	disk_dev *disk = (disk_dev *)cookie;

	iwprintf("disk_free");

	return B_OK;	/* already done in close */
}

static status_t
disk_control(
	void * cookie,
	uint32 iop,
	void * data,
	size_t len)
{
	disk_dev * disk = (disk_dev *)cookie;
	status_t err = B_OK;
	device_geometry *geom;

	if (!data) {
		return B_BAD_VALUE;
	}

	iwprintf("disk_control(%d)",iop);

	switch (iop) {
	case B_GET_DEVICE_SIZE:
		iwprintf("B_GET_DEVICE_SIZE"); 
		*((size_t *)data) = disk->size;
		break;
	case B_GET_GEOMETRY:
		iwprintf("B_GET_GEOMETRY"); 

		geom = (device_geometry *)data;
		
		geom->sectors_per_track = disk->size/512;
		
		geom->cylinder_count    = 1;
		geom->head_count        = 1;
		geom->bytes_per_sector = 512;
		geom->removable = FALSE;
		geom->read_only = disk->is_rom;
		geom->device_type = B_DISK;
		geom->write_once = FALSE;
		break;
	case B_GET_ICON:
		iwprintf("B_GET_ICON");
		memset(((device_icon *)data)->icon_data,disk->is_rom?212:125,
			(((device_icon *)data)->icon_size)==32?1024:256);
		break;
	case B_GET_MEDIA_STATUS:
		iwprintf("B_GET_MEDIA_STATUS"); 
		*((status_t *)data) = B_NO_ERROR;
		break;
	default:
		err = B_BAD_VALUE;
		break;
	}
	return err;
}


// TODO: split the read or write in smaller chunks.
static status_t
disk_read(
	void * cookie,
	off_t pos,
	void * data,
	size_t * nread)
{
	disk_dev *disk = (disk_dev *)cookie;
	uint32 start,len;
	
	//iwprintf("read %d bytes at %d",*nread,pos);
	
	if(pos>=disk->size) {
		*nread = 0;
		return B_OK;
	}
	
	len = min_c(*nread,disk->size-pos);
	
	// Must lock the memory !!! No paging is allowed with a spinlock held...
	lock_memory(data,len,0);
		
	iw_lock(disk->card);

	if(disk->is_rom)
		iw_rom_peek_block_8(disk->card,(uchar *)data,len,pos);
	else {
		if(pos&1) {
			*(((uchar *)data)++) = iw_ram_peek_8(disk->card,pos++);
			len--;
		}
		iw_ram_peek_block_16(disk->card,(uint16 *)data,len/2,pos);
		if(len&1)
			*((uchar *)data+len-1) = iw_ram_peek_8(disk->card,pos+len-1);
	}
	
	iw_unlock(disk->card);
	
	unlock_memory(data,len,0);
	
	*nread = len;
	
	return B_OK;
}


static status_t
disk_write(
	void * cookie,
	off_t pos,
	const void * data,
	size_t * nwritten)
{
	disk_dev *disk = (disk_dev *)cookie;
	uint32 len;
	
	//iwprintf("write %d bytes at %d",*nwritten,pos);
	
	if(disk->is_rom) {
		*nwritten = 0;
		return B_READ_ONLY_DEVICE;
	}

	if(pos>=disk->size) {
		*nwritten = 0;
		return B_OK;
	}

	len = min_c(*nwritten,disk->size-pos);

	// Must lock the memory !!! No paging is allowed with a spinlock held...
	lock_memory((void *)data,len,0);
		
	iw_lock(disk->card);
	
	if(pos&1) {
		iw_ram_poke_8(disk->card,pos++,*(((const uchar *)data)++));
		len--;
	}
	iw_ram_poke_block_16(disk->card,(const uint16 *)data,len/2,pos);
	if(len&1)
		iw_ram_poke_8(disk->card,pos+len-1,*((const uchar *)data+len-1));

	iw_unlock(disk->card);
	
	unlock_memory((void *)data,len,0);
	
	*nwritten = len;
	return B_OK;
}
