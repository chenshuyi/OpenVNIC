/*
 * =====================================================================================
 *
 *       Filename:  vnic.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/23/2013 10:51:33 AM
 *       Revision:  none
 *       Compiler:  mipsel-linux-uclibc-gcc
 *
 *         Author:  Andy.S.Y.Chen , <andysychen@gmail.com>
 *   Organization:  
 *
 * =====================================================================================
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/if_vnic.h>
#include <linux/netdevice.h>
#include <asm/uaccess.h>
#include <net/rtnetlink.h>

#include "vnic_core.h"
#include "vnic_dev.h"
#include "vnic_proc.h"

static struct hlist_head vnic_group_list_head[VNIC_GRP_LIST_HEAD_LEN];

unsigned char vdev_id_priv;

static inline void vnic_grp_list_init(void)
{
	unsigned char i;

	for (i = 0; i < VNIC_GRP_LIST_HEAD_LEN; i++) {
		vnic_group_list_head[i].first = NULL;
	}

}

static struct vnic_group *vnic_grp_alloc(struct net_device *real_dev, const char *name) 
{
	struct vnic_group *grp;

	grp = kzalloc(sizeof(struct vnic_group), GFP_KERNEL);
	if (!grp)
		return NULL;
	
	grp->real_dev = real_dev;
	
	if (strcmp("brcm", name)) {
		printk(KERN_INFO "BROADCOM device.\n");
		grp->gid = VNIC_GRP_ID_BROADCOM;
	}

	hlist_add_head_rcu(&grp->list_node, &vnic_group_list_head[real_dev->ifindex]);

	return grp;
}

struct vnic_group *vnic_find_grp(struct net_device *virt_dev)
{
	unsigned char vtype = vnic_dev_info(virt_dev)->vtype;
        unsigned char index = vnic_dev_info(virt_dev)->real_dev->ifindex;

	struct vnic_group *grp;
	struct hlist_node *n;

	hlist_for_each_entry_rcu(grp, n, &vnic_group_list_head[index], list_node) {
		if (grp->gid == vtype) {
			printk(KERN_INFO "This is BROADCOM group.\n");
			return grp;
		}
	}

	return NULL;
}

struct net_device* vnic_get_dev(int index, int vid)
{
	struct vnic_group *grp;
	struct hlist_node *n;

	hlist_for_each_entry_rcu(grp, n, &vnic_group_list_head[index], list_node) {
		
		return (grp->brcm_device_array[vid]);
	}

	return NULL;
}

extern void vnic_ioctl_set (int (*hook) (void __user *));

static int vnic_ioctl_handler (void __user *arg);
static int vnic_register_vdev (struct net_device *real_dev, char *vdev_name, unsigned char vdev_id, unsigned char vtype);
static int vnic_unregister_vdev (const char * vdev_name, const unsigned char vdev_id);


static struct packet_type vnic_packet_type __read_mostly = {
	.type           = cpu_to_be16(0x8874),
	.func           = vnic_skb_recv,
	.af_packet_priv = &vdev_id_priv,
};

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_module_init
 *  Description:  
 * =====================================================================================
 */
static int __init
vnic_module_init (void)
{
	vnic_proc_init();

	vnic_grp_list_init();

	dev_add_pack(&vnic_packet_type);

	vnic_ioctl_set(vnic_ioctl_handler);
	return 0;
}

/* -----  end of function vnic_module_init  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_module_exit
 *  Description:  
 * =====================================================================================
 */
static void __exit
vnic_module_exit (void)
{
	int i;

	i = VNIC_GRP_LIST_HEAD_LEN;

	vnic_proc_cleanup();
	
	dev_remove_pack(&vnic_packet_type);

  	while(i) {
        
		if (!hlist_empty(&vnic_group_list_head[i])) {
	
			struct vnic_group *grp;
			struct hlist_node *n, *m;

			hlist_for_each_entry_safe(grp, n, m, &vnic_group_list_head[i], list_node) {
				kfree(grp);
			}
		}

		--i;
	}
	
}

/* -----  end of function vnic_module_exit  ----- */

static int vnic_ioctl_handler(void __user *arg)
{
	struct vnic_ioctl_args args;
	struct net_device *dev = NULL;

	if (copy_from_user(&args, arg, sizeof(struct vnic_ioctl_args)))
		return -EFAULT;

	rtnl_lock();

	switch (args.cmd) {
		case ADD_BRCM_CMD:
			dev = dev_get_by_name(args.real_dev);

			if (!dev) {
				return -ENODEV;
			}

			printk("real dev :%s \n",dev->name);
			printk("virt dev :%s \n",args.virt_dev);
			printk("vid : %d \n", args.vdev_id);

			vnic_register_vdev (dev, args.virt_dev, args.vdev_id, VNIC_GRP_ID_BROADCOM);
			
			break;

		case DEL_BRCM_CMD:
			printk("virt dev :%s \n",args.virt_dev);
			printk("vid : %d \n", args.vdev_id);

			vnic_unregister_vdev (args.virt_dev, args.vdev_id);
			break;
		default:
			printk(KERN_WARNING "This virtual device is not supported.\n");
			return -ENODEV;
	}

	rtnl_unlock();
	return 0;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_register_vdev
 *  Description:  
 * =====================================================================================
 */
int
vnic_register_vdev (struct net_device *real_dev, char *vdev_name, unsigned char vdev_id, unsigned char vtype)
{
	struct net_device *new_dev;
	struct vnic_group *grp;
	unsigned char name[IFNAMSIZ];
	int err;

	vdev_id_priv = vdev_id;

	sprintf(name, "%s%d", vdev_name, vdev_id);
	printk("name : %s \n", name);

	new_dev = alloc_netdev(sizeof(struct vnic_device), name, vnic_netdev_setup);

	if (!new_dev)
		return -ENODEV;

	vnic_dev_info(new_dev)->real_dev = real_dev;
	vnic_dev_info(new_dev)->vid = vdev_id;
	vnic_dev_info(new_dev)->vtype = vtype;

	strcpy(new_dev->dev_addr, real_dev->dev_addr);

	printk(KERN_INFO "vnic: real dev ifindex is %d.\n", real_dev->ifindex);

	err = register_netdevice(new_dev);
	if (err < 0)
		goto out_free_newdev;

	err = vnic_proc_add_dev(new_dev);
	if (err < 0)
		printk(KERN_WARNING "vnic: failed to add proc entry for %s.\n", new_dev->name);

        /* Add in vnic_group_list */
	grp = vnic_find_grp(new_dev);
	if (!grp) {
		grp = vnic_grp_alloc(real_dev, vdev_name);
		printk(KERN_INFO "Create vnic group for %s.\n", grp->real_dev->name);
	}
	grp->brcm_device_array[vdev_id] = new_dev;

	printk(KERN_INFO "vnic: Add %s in VNIC_GROUP_LIST.\n", new_dev->name);
	
	return 0;

out_free_newdev:
	free_netdev(new_dev);
	return err;
}

/* -----  end of function vnic_register_vdev  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_unregister_vdev
 *  Description:  
 * =====================================================================================
 */
int
vnic_unregister_vdev (const char * vdev_name, const unsigned char vdev_id)
{
	struct net_device *dev = NULL;
	struct vnic_group *grp;
	unsigned char name[IFNAMSIZ];

	sprintf(name, "%s%d", vdev_name, vdev_id);
	dev = dev_get_by_name(name);

	if (dev) {

		vnic_proc_rem_dev(dev);

		dev_put(dev);

		grp = vnic_find_grp(dev);
		if (grp)
			grp->brcm_device_array[vdev_id] = NULL;
		
		unregister_netdevice(dev);

	}else {
		dev_put(dev);
		printk(KERN_ERR "Could not find this virtual device.\n");
		return -ENODEV;
	}

	return 0;
}	

/* -----  end of function vnic_register_vdev  ----- */


module_init(vnic_module_init);
module_exit(vnic_module_exit);


