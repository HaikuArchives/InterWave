#include <KernelExport.h>

#include "mutex.h"

// Simple mutex.
// Many threads can block on acquire() and will be serviced in the
// order they arrived, but no two threads can hold the mutex simultaneously.
// (the semaphore's count will never go above 1)

status_t mutex_create(mutex *m,const char *name)
{
	m->waiting_count = 0;
	
	if((m->sem = create_sem(1,name)) < B_OK)
		return m->sem;
		
	set_sem_owner(m->sem, B_SYSTEM_TEAM);
	
	return B_OK;
}

status_t mutex_acquire(mutex *m)
{
	atomic_add(&m->waiting_count,1);
	
	return acquire_sem(m->sem);
}

// Returns true if someone was waiting and has been unblocked
// (so that the interrupt handler knows when to return
//  B_INVOKE_SCHEDULER)
bool mutex_release(mutex *m)
{
	if(atomic_add(&m->waiting_count,-1) > 0) {
		// someone is in the queue
		release_sem_etc(m->sem,1,B_DO_NOT_RESCHEDULE);
		return true;
	}
	else {
		// the queue is empty
		atomic_add(&m->waiting_count,1); // undo damage
		return false;
	}
}

status_t mutex_delete(mutex *m)
{
	return delete_sem(m->sem);
}
