/*
 * =====================================================================================
 *
 *       Filename:  vnic_proc.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/22/2013 10:18:12 AM
 *       Revision:  none
 *       Compiler:  mipsel-linux-uclibc-gcc
 *
 *         Author:  Andy.S.Y.Chen , <andysychen@gmail.com>
 *   Organization:  
 *
 * ====================================================================================/
 */

#include <linux/seq_file.h>

#include "vnic_proc.h"

#define D_NAME "vnic"     /* /proc/net/vnic/<virtual device>  */
#define C_NAME "config"   /* /proc/net/vnic/config            */

static struct proc_dir_entry *proc_vnic_dir;
static struct proc_dir_entry *proc_vnic_conf;

static void *vnic_seq_start(struct seq_file *seq, loff_t *pos);
static void *vnic_seq_next(struct seq_file *seq, void *v, loff_t *pos);
static void  vnic_seq_stop(struct seq_file *seq, void *v);
static int   vnic_seq_show(struct seq_file *seq, void *v);

static int   vnic_dev_seq_show(struct seq_file *seq, void *v);

static struct seq_operations vnic_seq_ops = {
	.start = vnic_seq_start,
	.next  = vnic_seq_next,
	.stop  = vnic_seq_stop,
	.show  = vnic_seq_show,
};

static int vnic_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &vnic_seq_ops);
}

static const struct file_operations vnic_config_fops = {
	.owner   = THIS_MODULE,
	.open    = vnic_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int vnic_dev_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, vnic_dev_seq_show, PDE(inode)->data);
}

static const struct file_operations vnic_dev_fops = {
	.owner   = THIS_MODULE,
	.open    = vnic_dev_seq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_proc_add_dev
 *  Description:  
 * =====================================================================================
 */

int
vnic_proc_add_dev (struct net_device *vnic_dev)
{
	struct vnic_device *vdev = vnic_dev_info(vnic_dev);

	vdev->dent = create_proc_entry(vnic_dev->name, S_IFREG | S_IRUSR | S_IWUSR, proc_vnic_dir);

	if (!vdev->dent) 
		return -ENOBUFS;

	vdev->dent->data = vnic_dev;

#ifdef VNIC_PROC_DEBUG
	printk(KERN_INFO "vnic_proc_add_dev, device -:%s:- being added. \n", vnic_dev->name);
#endif

	return 0;
}	

/* -----  end of function vnic_proc_add_dev  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_proc_rem_dev
 *  Description:  
 * =====================================================================================
 */

int
vnic_proc_rem_dev (struct net_device *vnic_dev)
{
	if (vnic_dev_info(vnic_dev)->dent) {
		remove_proc_entry(vnic_dev_info(vnic_dev)->dent->name, proc_vnic_dir);
		vnic_dev_info(vnic_dev)->dent = NULL;

#ifdef VNIC_PROC_DEBUG
	printk(KERN_INFO "vnic_proc_rem_dev, device -:%s:- being removed. \n", vnic_dev->name);
#endif
	}

	return 0;
}

/* -----  end of function vnic_proc_rem_dev  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_proc_init
 *  Description:  
 * =====================================================================================
 */

int
vnic_proc_init (void)
{
	proc_vnic_dir = proc_mkdir(D_NAME, proc_net);

	if (proc_vnic_dir) {
		
		proc_vnic_conf = create_proc_entry(C_NAME, S_IFREG | S_IRUSR | S_IWUSR, proc_vnic_dir);

		if (proc_vnic_conf) {
			printk(KERN_ERR "Create /proc/net/" C_NAME "\n");
			proc_vnic_conf->proc_fops = &vnic_config_fops;
			return 0;
		}

	}else {
		printk(KERN_ERR "Can not create /proc/net/" D_NAME "\n");
                vnic_proc_cleanup();
		return -ENOMEM;
	}

	return -ENOBUFS;
}		

/* -----  end of function vnic_proc_init  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_proc_exit(void)
 *  Description:  
 * =====================================================================================
 */

void
vnic_proc_cleanup (void)
{
	if (proc_vnic_conf) {
		remove_proc_entry(C_NAME, proc_vnic_dir);
	}

	if (proc_vnic_dir) {
		proc_net_remove(D_NAME);
	}
}
/* -----  end of function vnic_proc_exit(void)  ----- */

static void *vnic_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct net_device *dev;
	loff_t i = 1;

	read_lock(&dev_base_lock);

	if (*pos == 0)
		return SEQ_START_TOKEN;

	for_each_netdev(dev) {
		if (!is_vnic_dev(dev))
			continue;

		if (i++ == *pos)
			return dev;
	}

	return NULL;
}

static void *vnic_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct net_device *dev;

	++pos;

	dev = (struct net_device *)v;

	if (v == SEQ_START_TOKEN)
		dev = net_device_entry(&dev_base_head);

	for_each_netdev_continue(dev) {
		if (!is_vnic_dev(dev))
			continue;

		return dev;
	}

	return NULL;
}

static void vnic_seq_stop(struct seq_file *seq, void *v)
{
	read_unlock(&dev_base_lock);
}

static int vnic_seq_show(struct seq_file *seq, void *v)
{
  	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, "VNIC Device Name | VNIC Device ID | Real Device \n");
	}else {
		struct net_device *dev = v;
		struct vnic_device *vdev = vnic_dev_info(dev);
		seq_printf(seq, "%-15s| %d | %s \n", dev->name, vdev->vid, vdev->real_dev->name);
	}

	return 0;
}

static int vnic_dev_seq_show(struct seq_file *seq, void *v)
{
	return 0;
}
