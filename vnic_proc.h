
#ifndef __VNIC_PROC_INC__
#define __VNIC_PROC_INC__

#include "vnic_core.h"
#include <linux/proc_fs.h>

int vnic_proc_init (void);
void vnic_proc_cleanup (void);

int vnic_proc_add_dev (struct net_device *vnic_dev);
int vnic_proc_rem_dev (struct net_device *vnic_dev);

#endif

