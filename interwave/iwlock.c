#include <KernelExport.h>

#include <string.h>
#include <stdio.h>
#include <byteorder.h>

#include "interwave.h"
#include "iwprotos.h"

void iw_lock(interwave_dev *card)
{
	card->saved_cpu_status = disable_interrupts();
	acquire_spinlock(&card->hardware_lock);
}

void iw_unlock(interwave_dev *card)
{
	release_spinlock(&card->hardware_lock);
	restore_interrupts(card->saved_cpu_status);
}

