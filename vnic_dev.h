
#ifndef __VNIC_DEV_INC__
#define __VNIC_DEV_INC__

#include <linux/netdevice.h>

void vnic_netdev_setup(struct net_device *dev);
int vnic_skb_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type *ptype, struct net_device *orig_dev);
#endif
