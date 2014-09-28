#include <Drivers.h>
#include <KernelExport.h>
#include <ISA.h>
#include <config_manager.h>
#include <isapnp.h>
#ifndef PRE_GENKI
#include <driver_settings.h>
#endif

// The kernel does export a few useful functions from these, but
// not everything... Beware.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interwave.h"
#include "iwprotos.h"
#include "iwregs.h"

// <Drivers.h> lacks a proper prototype for publish_devices
// (missing 'void')
const char **publish_devices(void);

// The LDN of the Synth/Codec component is 0
#define IW_LOGICAL_DEVICE_NUMBER	0

// see IWPG p. 15-5
const uint16 ram_config_to_size[16] =
	{ 256, 512, 1024, 1024+256, 3*1024+256, 1024+512, 2*1024+512, 1024,
	  2*1024, 4*1024, 4*1024, 8*1024, 16*1024, 0, 0, 0 };
const uint16 rom_config_to_size[8] =
	{ 512, 1024, 2*1024, 4*1024, 8*1024, 0, 0, 0 };

int32 num_cards;
interwave_dev cards[NUM_CARDS];
int num_names;
char * names[NUM_CARDS*7+1];

int32 api_version = B_CUR_DRIVER_API_VERSION;

static isa_module_info *isa;
static config_manager_for_driver_module_info *cfgmgr;

// The vendor ID is encoded at run-time from 'GRV0001'
static uint32 make_iw_vendor_id(void)
{
	EISA_PRODUCT_ID eisa_id;
	MAKE_EISA_PRODUCT_ID(&eisa_id,'G','R','V',0x000,1);
	return eisa_id.id;
}

// playing it safe with lots of eieio's...
// (which do nothing on x86 anyway... just a way to be
//  safe in anticipation of the day I want to port to PPC)
uchar outp(uint16 port, uchar val)
{
	isa->write_io_8(port,val);
	__eieio();
		
	return val;
}

uchar inp(uint16 port)
{
	__eieio();
	return isa->read_io_8(port);
}

uint16 outpw(uint16 port, uint16 val)
{
	isa->write_io_16(port,val);
	__eieio();
	
	return val;
}

uint16 inpw(uint16 port)
{
	__eieio();
	return isa->read_io_16(port);
}

long start_dma(long channel,void *buf,long transfer_count,uchar mode,uchar e_mode)
{
	return isa->start_isa_dma(channel,buf,transfer_count,mode,e_mode);
}


#if DEBUG
void iwprintf(const char *fmt,...)
{
	va_list args;
	char buf[1024];
	
	va_start(args,fmt);
	vsprintf(buf,fmt,args);
	dprintf("interwave:%s\n",buf);
	va_end(args);
}

void iwkprintf(const char *fmt,...)
{
	va_list args;
	char buf[1024];
	
	va_start(args,fmt);
	vsprintf(buf,fmt,args);
	/*k*/dprintf("interwave:%s\n",buf);
	va_end(args);
}
#endif

static void make_device_names(interwave_dev * iw)
{
	char * name = iw->name;
	sprintf(name, "interwave/%d", (int)(iw-cards+1));

#if DO_PCM
	sprintf(iw->pcm.name, "audio/raw/%s", name);
	names[num_names++] = iw->pcm.name;
	
	sprintf(iw->pcm.oldname, "audio/old/%s", name);
	names[num_names++] = iw->pcm.oldname;
#endif

#if DO_MIXER
	sprintf(iw->mixer.name, "audio/mix/%s", name);
	names[num_names++] = iw->mixer.name;
#endif

#if DO_DISK
	sprintf(iw->ramdisk.name, "disk/%s/ram", name);
	names[num_names++] = iw->ramdisk.name;

	sprintf(iw->romdisk.name, "disk/%s/rom", name);
	names[num_names++] = iw->romdisk.name;
#endif

	names[num_names] = NULL;
}

static status_t get_interwave_config(interwave_dev *iw,uint64 device)
{
	struct device_configuration *config;
	resource_descriptor descriptor;
	uint32 len,count;
	void *settings_handle;

	if((len = cfgmgr->get_size_of_current_configuration_for(device)) <= 0) {
		dprintf("interwave:fatal:size of current config = %ld\n",len);
		return ENODEV;
	}
	
	config = (struct device_configuration *)malloc(len);
	if(!config) {
		dprintf("interwave:fatal:uh???couldn't malloc device_configuration!\n");
		goto bail;
	}
	
	if(cfgmgr->get_current_configuration_for(device,config,len) != B_OK) {
		dprintf("interwave:fatal:couldn't get current config\n");
		goto bail;
	}
	
	// get ports
	if((count = cfgmgr->count_resource_descriptors_of_type(config,
				B_IO_PORT_RESOURCE)) != 3) {
		dprintf("interwave:fatal:expected 3 port resources, got %ld\n",count);
		goto bail;
	}
	
	if(cfgmgr->get_nth_resource_descriptor_of_type(config,0,
	   B_IO_PORT_RESOURCE,&descriptor,len) != B_OK) {
		dprintf("interwave:fatal:couldn't get P2XR\n");
		goto bail;
	} else {
		iw->p2xr = descriptor.d.r.minbase;
		iwprintf("P2XR is %3x",iw->p2xr);
	}
	
	if(cfgmgr->get_nth_resource_descriptor_of_type(config,1,
	   B_IO_PORT_RESOURCE,&descriptor,len) != B_OK) {
		dprintf("interwave:fatal:couldn't get P3XR\n");
		goto bail;
	} else {
		iw->p3xr = descriptor.d.r.minbase;
		iwprintf("P3XR is %3x",iw->p3xr);
	}
	
	if(cfgmgr->get_nth_resource_descriptor_of_type(config,2,
	   B_IO_PORT_RESOURCE,&descriptor,len) != B_OK) {
		dprintf("interwave:fatal:couldn't get PCODAR\n");
		goto bail;
	} else {
		iw->pcodar = descriptor.d.r.minbase;
		iwprintf("PCODAR is %3x",iw->pcodar);
	}
	
	// get IRQs
	if((count = cfgmgr->count_resource_descriptors_of_type(config,
				B_IRQ_RESOURCE)) > 2 || count < 1 ) {
		dprintf("interwave:fatal:expected 1 or 2 irq resources, found %ld\n",count);
		return ENODEV;
	}
	
	if(cfgmgr->get_nth_resource_descriptor_of_type(config,0,
	   B_IRQ_RESOURCE,&descriptor,len) != B_OK
	   || descriptor.d.m.mask == 0) {
		dprintf("interwave:fatal:couldn't get IRQ1\n");
		goto bail;
	} else {
		for(iw->irq1=0;!(descriptor.d.m.mask&1);iw->irq1++)
			descriptor.d.m.mask >>= 1;
		iwprintf("IRQ1 is %d",iw->irq1);
	}
	
	iw->irq2 = 0;

	// get DMAs
	if((count = cfgmgr->count_resource_descriptors_of_type(config,
				B_DMA_RESOURCE)) > 2 || count < 1 ) {
		dprintf("interwave:fatal:expected 1 or 2 dma resources, got %ld\n",count);
		return ENODEV;
	}
	
	if(cfgmgr->get_nth_resource_descriptor_of_type(config,0,
	   B_DMA_RESOURCE,&descriptor,len) != B_OK
	   || descriptor.d.m.mask == 0) {
		dprintf("interwave:fatal:couldn't get DMA1\n");
		goto bail;
	} else {
		for(iw->dma1=0;!(descriptor.d.m.mask&1);iw->dma1++)
			descriptor.d.m.mask >>= 1;
		iwprintf("DMA1 is %d",iw->dma1);
	}

	if(cfgmgr->get_nth_resource_descriptor_of_type(config,1,
	   B_DMA_RESOURCE,&descriptor,len) != B_OK
	   || descriptor.d.m.mask == 0) {
		dprintf("interwave:fatal:couldn't get DMA2\n");
		goto bail;
	} else {
		for(iw->dma2=0;!(descriptor.d.m.mask&1);iw->dma2++)
			descriptor.d.m.mask >>= 1;
		iwprintf("DMA2 is %d",iw->dma2);
	}
	
	free(config);
	
	// complete the config
	iw->igidxr = iw->p3xr + 0x03;
	iw->i16dp = iw->p3xr + 0x04;
	iw->i8dp = iw->p3xr + 0x05;
	iw->svsr = iw->p3xr + 0x02;
	iw->cdatap = iw->pcodar + 0x01;
	iw->csr1r = iw->pcodar + 0x02;
	iw->gmxr = iw->p3xr + 0x00;
	iw->gmxdr = iw->p3xr + 0x01;
	iw->lmbdr = iw->p3xr + 0x07;
	
	iw->ram_config = 0;
	iw->rom_config = 1;

#ifndef PRE_GENKI
	if((settings_handle = (void *)load_driver_settings("interwave"))) {
		const char *param;

		param = get_driver_parameter(settings_handle,"ram_config","0","0");
		iwprintf("read param : ram_config=\"%s\"=%d",param,atoi(param));
		
		iw->ram_config = atoi(param) & 0x0f;
		
		if(iw->ram_config > 12) {
			dprintf("interwave: WARNING: unknown ram config %d",iw->ram_config);
		}
		
		param = get_driver_parameter(settings_handle,"rom_config","1","1");
		iwprintf("read param : rom_config=\"%s\"=%d",param,atoi(param));
		
		iw->rom_config = atoi(param) & 0x07;
		
		if(iw->rom_config > 4) {
			dprintf("interwave: WARNING: unknown rom config %d",iw->rom_config);
		}
		
		unload_driver_settings(settings_handle);
	} else {
		iwprintf("no settings file found");
	}
#endif
	
	iw->ram_size = ram_config_to_size[iw->ram_config];
	iw->rom_size = rom_config_to_size[iw->rom_config];
	
	return B_OK;
bail:
	if(config)
		free(config);
	return ENODEV;
}

static status_t setup_interwave(interwave_dev *iw,uint64 device)
{
	status_t err;
	
#if DEBUG
	thread_id thid;
	thread_info thinfo;
	thid = find_thread(NULL);
	get_thread_info(thid,&thinfo);
	iwprintf("setup_interwave(%08x), thid=%d(%s from team %d)",
		iw,thid,thinfo.name,thinfo.team);
#endif

	if(get_interwave_config(iw,device) != B_OK)
		return ENODEV;
	
	make_device_names(iw);
	
	// * reset and prepare the hardware
	iw_init(iw);
	
	iw_mixer_defaults(iw);
	
	if((err=iw_find_low_memory(iw)) < B_OK) {
		return B_ERROR;
	}
	
	// * misc stuff
	iw->romdisk.is_rom = true;
	iw->ramdisk.is_rom = false;

	iw->romdisk.size = 1024*iw->rom_size;
	iw->ramdisk.size = 1024*iw->ram_size;
	
	// * create init_sem's

	iw->pcm.init_sem = iw->pcm.teardown_sem = iw->romdisk.init_sem 
		= iw->ramdisk.init_sem = iw->mixer.init_sem = -1;

	if(((iw->pcm.init_sem = create_sem(0,"iw pcm init")) < B_OK)
		|| ((iw->pcm.teardown_sem = create_sem(1,"iw pcm teardown")) < B_OK)
		|| ((iw->romdisk.init_sem = create_sem(0,"iw romdisk init")) < B_OK)
		|| ((iw->ramdisk.init_sem = create_sem(0,"iw ramdisk init")) < B_OK)
		|| ((iw->mixer.init_sem = create_sem(0,"iw mixer init")) < B_OK)) {
		
		if(iw->pcm.init_sem < B_OK) delete_sem(iw->pcm.init_sem);
		if(iw->pcm.teardown_sem < B_OK) delete_sem(iw->pcm.teardown_sem);
		if(iw->romdisk.init_sem < B_OK) delete_sem(iw->romdisk.init_sem);
		if(iw->ramdisk.init_sem < B_OK) delete_sem(iw->ramdisk.init_sem);
		if(iw->mixer.init_sem < B_OK) delete_sem(iw->mixer.init_sem);
		
		return B_ERROR;
	}
	
	// * install interrupt handler
	if(install_io_interrupt_handler(iw->irq1,iw_handler,iw,0)!= B_OK) {
		dprintf("interwave:failed to install IRQ handler\n");
		return B_ERROR;
	}

	return B_OK;
}

static void teardown_interwave(interwave_dev *iw)
{
	//	"shut down" the card
	//	(tell it to stop generating interrupts, and to shut up)
	
	// * remove interrupt handler
	if(remove_io_interrupt_handler(iw->irq1,iw_handler,iw)!=B_OK)
		iwkprintf("failed to remove handler");
	else
		iwkprintf("handler removed");

	iw_lock(iw); // uh, what's the point, with the handler removed...
	
	// switch off ultra line in/out, mic, and every DMA/IRQ
	iw_poke(iw,UMCR,0x03);// FIXME (write iw_uninit())
		
	iw_unlock(iw);

	// delete init/teardown_sem's
	delete_sem(iw->pcm.init_sem);
	delete_sem(iw->pcm.teardown_sem);
	delete_sem(iw->romdisk.init_sem);
	delete_sem(iw->ramdisk.init_sem);
	delete_sem(iw->mixer.init_sem);
}

status_t init_hardware(void)
{
	status_t err = ENODEV;
	struct device_info info;
	uint64 cookie = 0;
	uint32 vendor_id = make_iw_vendor_id();
	
	iwprintf("init_hardware");

	if(get_module(B_ISA_MODULE_NAME,(module_info **)&isa))
		return ENOSYS;

	if(get_module(B_CONFIG_MANAGER_FOR_DRIVER_MODULE_NAME,
		(module_info **)&cfgmgr)) {
		put_module(B_ISA_MODULE_NAME);
		return ENOSYS;
	}

	// find the hardware
	while(cfgmgr->get_next_device_info(B_ISA_BUS,
		  &cookie,&info,sizeof(info)) == B_OK) {
		struct device_info *dinfo; 
		struct isa_info *iinfo;
		
		/* only worry about configured devices */ 
		if (info.config_status != B_OK) continue; 
		
		dinfo = malloc(info.size); 
		cfgmgr->get_device_info_for(cookie,
			dinfo, info.size); 
		iinfo = (struct isa_info *)((char *)dinfo + 
			info.bus_dependent_info_offset); 

		if(iinfo->vendor_id == vendor_id &&
		   iinfo->ldn == IW_LOGICAL_DEVICE_NUMBER) {
			dprintf("interwave:InterWave found at csn %d",iinfo->csn);
			err = B_OK;
		}

		free(dinfo);
	}
	err = B_OK;

	put_module(B_CONFIG_MANAGER_FOR_DRIVER_MODULE_NAME);
	cfgmgr = NULL;
	
	put_module(B_ISA_MODULE_NAME);
	isa = NULL;

	return err;
}

// init_driver: very much like init_hardware, except that we actually
// do something with the cards we find
status_t init_driver(void)
{
	struct device_info info;
	uint64 cookie = 0;
	uint32 vendor_id = make_iw_vendor_id();

	iwprintf("init_driver");
	
	num_cards = 0;
	
	if(get_module(B_ISA_MODULE_NAME,(module_info **)&isa))
		return ENOSYS;

	if(get_module(B_CONFIG_MANAGER_FOR_DRIVER_MODULE_NAME,
		(module_info **)&cfgmgr)) {
		put_module(B_ISA_MODULE_NAME);
		return ENOSYS;
	}

	// find the hardware
	while(cfgmgr->get_next_device_info(B_ISA_BUS,
		  &cookie,&info,sizeof(info)) == B_OK) {
		struct device_info *dinfo; 
		struct isa_info *iinfo;
		
		/* only worry about configured devices */ 
		if (info.config_status != B_OK) continue; 
		
		dinfo = malloc(info.size); 
		cfgmgr->get_device_info_for(cookie,
			dinfo, info.size); 
		iinfo = (struct isa_info *)((char *)dinfo + 
			info.bus_dependent_info_offset); 

		if(iinfo->vendor_id == vendor_id &&
		   iinfo->ldn == IW_LOGICAL_DEVICE_NUMBER) {
			dprintf("interwave:InterWave found at csn %d\n",iinfo->csn);

			memset(&cards[num_cards],0,sizeof(interwave_dev));

			if (setup_interwave(&cards[num_cards],cookie) != B_OK)
				dprintf("interwave:setup of InterWave %ld failed\n", num_cards+1);
			else
				num_cards++;
		}

		free(dinfo);
	}

	put_module(B_CONFIG_MANAGER_FOR_DRIVER_MODULE_NAME);
	cfgmgr = NULL;

	if (!num_cards) {
		put_module(B_ISA_MODULE_NAME);
		isa = NULL;
		dprintf("interwave:no suitable cards found\n");
		return ENODEV;
	}

#if DEBUG
//	add_debugger_command(...)
#endif

	return B_OK;
}

void uninit_driver(void)
{
	int ix, cnt = num_cards;
	num_cards = 0;

	iwprintf("uninit_driver");
	
//	remove_debugger_command(...)

//	destroy device structures
	for (ix=0; ix<cnt; ix++) {
		teardown_interwave(&cards[ix]);
	}
	memset(&cards, 0, sizeof(cards));

	put_module(B_ISA_MODULE_NAME);
}

const char **publish_devices(void)
{
	int ix = 0;
	iwprintf("publish_devices");
	
	for (ix=0; names[ix]; ix++) {
		iwprintf("publish %s", names[ix]);
	}
	return (const char **)names;
}

device_hooks *
find_device(
	const char * name)
{
	int ix;

	iwprintf("find_device(%s)", name);

	for (ix=0; ix<num_cards; ix++) {
#if DO_PCM
		if (!strcmp(cards[ix].pcm.name, name)) {
			return &pcm_hooks;
		}
		if (!strcmp(cards[ix].pcm.oldname, name)) {
			return &pcm_hooks;
		}
#endif
#if DO_MIXER
		if (!strcmp(cards[ix].mixer.name, name)) {
			return &mixer_hooks;
		}
#endif
#if DO_DISK		
		if (!strcmp(cards[ix].ramdisk.name, name)) {
			return &disk_hooks;
		}
		if (!strcmp(cards[ix].romdisk.name, name)) {
			return &disk_hooks;
		}
#endif
	}

	iwprintf("find_device(%s) failed", name);
	return NULL;
}
