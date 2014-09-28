#include <KernelExport.h>
#include <string.h>

#include "interwave.h"

static status_t mixer_open(const char *name, uint32 flags, void **cookie);
static status_t mixer_close(void *cookie);
static status_t mixer_free(void *cookie);
static status_t mixer_control(void *cookie, uint32 op, void *data, size_t len);
static status_t mixer_read(void *cookie, off_t pos, void *data, size_t *len);
static status_t mixer_write(void *cookie, off_t pos, const void *data, size_t *len);

device_hooks mixer_hooks = {
    &mixer_open,
    &mixer_close,
    &mixer_free,
    &mixer_control,
    &mixer_read,
    &mixer_write,
    NULL,		/* select */
    NULL,		/* deselect */
    NULL,		/* readv */
    NULL		/* writev */
};

static status_t mixer_open(
	const char * name,
	uint32 flags,
	void ** cookie)
{
	int ix;
	mixer_dev * it = NULL;

	iwprintf("mixer_open");

	*cookie = NULL;
	for (ix=0; ix<num_cards; ix++) {
		if (!strcmp(name, cards[ix].mixer.name)) {
			break;
		}
	}
	if (ix == num_cards) {
		return ENODEV;
	}

	atomic_add(&cards[ix].mixer.open_count, 1);
	cards[ix].mixer.card = &cards[ix];
	*cookie = &cards[ix].mixer;

	return B_OK;
}

static status_t mixer_close(
	void *cookie)
{
	mixer_dev *it = (mixer_dev *)cookie;

	atomic_add(&it->open_count, -1);

	return B_OK;
}

static status_t mixer_free(
	void *cookie)
{
	iwprintf("mixer_free");

	if (((mixer_dev *)cookie)->open_count != 0) {
		dprintf("interwave: mixer open_count is bad in mixer_free()!\n");
	}
	return B_OK;	/* already done in close */
}

static status_t
mixer_control(
	void * cookie,
	uint32 iop,
	void * data,
	size_t len)
{
	mixer_dev * it = (mixer_dev *)cookie;
	status_t err = B_OK;

	if (!data) {
		return B_BAD_VALUE;
	}

	iwprintf("mixer_control"); /* slow printing */

	switch (iop) {
/*	case B_MIXER_GET_VALUES:
		((sonic_vibes_level_cmd *)data)->count = 
			gather_info(it, ((sonic_vibes_level_cmd *)data)->data, 
				((sonic_vibes_level_cmd *)data)->count);
		break;
	case B_MIXER_SET_VALUES:
		((sonic_vibes_level_cmd *)data)->count = 
			disperse_info(it, ((sonic_vibes_level_cmd *)data)->data, 
				((sonic_vibes_level_cmd *)data)->count);
		break;
*/	default:
		err = B_BAD_VALUE;
		break;
	}
	return err;
}


static status_t
mixer_read(
	void * cookie,
	off_t pos,
	void * data,
	size_t * nread)
{
	return EPERM;
}


static status_t
mixer_write(
	void * cookie,
	off_t pos,
	const void * data,
	size_t * nwritten)
{
	return EPERM;
}

