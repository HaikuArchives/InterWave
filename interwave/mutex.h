#ifndef MUTEX_H
#define MUTEX_H

typedef struct {
	sem_id sem;
	int32 waiting_count;
} mutex;

status_t mutex_create(mutex *m,const char *name);
status_t mutex_acquire(mutex *m);
bool mutex_release(mutex *m);
status_t mutex_delete(mutex *m);

#endif /* MUTEX_H */
