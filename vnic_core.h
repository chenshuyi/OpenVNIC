
#ifndef __VNIC_CORE_INC__
#define __VNIC_CORE_INC__

#include <linux/netdevice.h>
#include <linux/if_vnic.h>

#define BROADCOM
#define VNIC_PROC_DEBUG


/*  
 *  OpenVNIC use several list to manage the virtual device.
 *
 *  vnic_group_list            vnic_group               vnic_group
 *  ----------------       ------------------       ------------------
 *  |  hlist_head  |------>|      hlist     |------>|      hlist     |
 *  ----------------       ------------------       ------------------
 *  |  hlist_head  |----   |      eth0      |       |      eth0      |
 *  ----------------   |    ------------------       -----------------       -----------
 *  |    ......    |   |   | brcm_dev_array |       |  ar_dev_array  |------>|   ar0   |
 *  ----------------   |   ------------------       ------------------       -----------
 *                     |                                                     |   ar1   |
 *                     |   ------------------                                -----------
 *                     --->|      hlist     |                                |   ar2   |
 *                         ------------------                                -----------
 *                         |      eth1      |                                |   ...   |
 *                         ------------------       -----------              -----------
 *                         | brcm_dev_array |------>|  brcm0  |
 *                         ------------------       -----------
 *                                                  |  brcm1  |
 *                                                  -----------
 *                                                  |  brcm2  |
 *                                                  -----------
 *                                                  |   ...   |
 *                                                  -----------
 *                                                
 */


#define VNIC_GRP_LIST_HEAD_LEN   5
#define VNIC_GRP_ID_BROADCOM     0
#define VNIC_GRP_ID_ATHEROS      1


#ifdef BROADCOM

#define BCM53101
#define BCM53115

#define BRCM_TAG_LEN             4

#endif /* BROADCOM */

struct vnic_device {
	struct net_device *real_dev;
	unsigned int vid;
	unsigned char vtype;
	struct proc_dir_entry *dent;

#ifdef BROADCOM
	struct net_device_stats *brcm_rx_stats;
#endif

};

struct vnic_group {
	struct hlist_node list_node;
	struct net_device *real_dev;
        unsigned char gid;
#ifdef BROADCOM
	struct net_device *brcm_device_array[BRCM_GROUP_ARRAY_LEN];
#endif

#ifdef ATHEROS
	struct net_device *ar_device_array[AR_GROUP_ARRAY_LEN];
#endif
};


static inline struct vnic_device* vnic_dev_info(struct net_device *dev)
{
	return netdev_priv(dev);
}

static inline int is_vnic_dev(struct net_device *dev)
{
	return dev->priv_flags & IFF_VNIC;
}

struct vnic_group *vnic_find_grp(struct net_device *virt_dev);
struct net_device* vnic_get_dev(int index, int vid);

#endif /* __VNIC_CORE_INC__  */
