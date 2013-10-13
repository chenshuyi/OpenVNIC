/*
 * =====================================================================================
 *
 *       Filename:  vnic_dev.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/23/2013 07:18:51 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Andy.S.Y.Chen , <andysychen@gmail.com>
 *   Organization:  
 *
 * =====================================================================================
 */
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <proto/ethernet.h>

#include "vnic_core.h"
#include "vnic_dev.h"

#ifdef BROADCOM

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_get_brcm_port
 *  Description:  
 * =====================================================================================
 */

static unsigned char
vnic_get_brcm_port (struct sk_buff *skb)
{
	const struct brcm_header *pb;
	unsigned char port;
	
	pb = (struct brcm_header *)skb->data;
	
	port = ((char*)(&(pb->brcm_tag)))[3];

	return port;
}	

/* -----  end of function vnic_get_brcm_port  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_skb_rebuild
 *  Description:  Remove the brcm tag which added by switch chip before send the skb to
 *                IP layer
 * =====================================================================================
 */

struct sk_buff*
vnic_skb_rebuild (struct sk_buff *skb)
{

	const struct brcm_header *pb;
	const unsigned char *stmp;
	const unsigned char *dtmp;

	struct ethhdr *eth;

	unsigned char src[6],dst[6];
	unsigned char i;
	unsigned short ptype;

	pb = (struct brcm_header *)skb->data;
 	stmp = pb->ether_shost;
	dtmp = pb->ether_dhost;
	ptype = pb->ether_type;

	for(i = 0; i < ETH_ALEN; i++,stmp++,dtmp++)
	{
		src[i] = *stmp;
		dst[i] = *dtmp;
	}

	skb_pull(skb, BRCM_TAG_LEN);

	eth = (struct ethhdr *)skb->data;

	memcpy(eth->h_dest, dst, ETH_ALEN);
	memcpy(eth->h_source, src, ETH_ALEN);

	eth->h_proto = ptype;

	printk(KERN_INFO "%s: src = %02x:%02x:%02x:%02x:%02x:%02x \n",\
			__FUNCTION__,eth->h_source[0],eth->h_source[1],eth->h_source[2],\
			             eth->h_source[3],eth->h_source[4],eth->h_source[5]);
	

	printk(KERN_INFO "%s: dst = %02x:%02x:%02x:%02x:%02x:%02x \n",\
			__FUNCTION__,eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],\
			             eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);

	printk(KERN_INFO "%s: proto = :%04x \n",__FUNCTION__,ntohs(eth->h_proto));

	return skb;
}

/* -----  end of function vnic_skb_rebuild  ----- */

#endif

int vnic_skb_recv(struct sk_buff *skb, struct net_device *dev, 
		  struct packet_type *ptype, struct net_device *orig_dev)
{

#ifdef BROADCOM

	struct net_device *vdev;

	unsigned char port;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb) 
		goto err_free;

	/* This packet is come form which port of real device */
	port = vnic_get_brcm_port(skb);

	rcu_read_lock();

	vdev = vnic_get_dev(skb->dev->ifindex, port);
        
	if (NULL == vdev) {
		printk(KERN_INFO "%s: can not find this virtual device.\n", __FUNCTION__);
		goto err_unlock;
	}

	skb->dev = vdev;

	if (dev != vnic_dev_info(skb->dev)->real_dev) {
		printk(KERN_INFO "%s: drop the wrong packet.\n", __FUNCTION__);
		skb->dev->stats.rx_errors++;
		kfree_skb(skb);
	}

	skb->dev->stats.rx_packets++;
	skb->dev->stats.rx_bytes += skb->len;

	rcu_read_unlock();

	printk(KERN_INFO "%s: packet send to %s.\n", __FUNCTION__, skb->dev->name);

	skb = vnic_skb_rebuild(skb);

	if (!skb) {
		printk(KERN_INFO "%s: fail to remove brcm tag.\n", __FUNCTION__);
	        goto err_free;
	}

	netif_rx(skb);

#endif
	return NET_RX_SUCCESS;

err_unlock:
	rcu_read_unlock();
err_free:
	kfree_skb(skb);
	return NET_RX_DROP;
}



/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_dev_hard_start_xmit
 *  Description:  
 * =====================================================================================
 */
int
vnic_dev_hard_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
	printk(KERN_INFO "%s: %s send packet.\n", __FUNCTION__, dev->name);

	skb->dev->stats.tx_packets++;
	skb->dev->stats.tx_bytes += skb->len;

	skb->dev = vnic_dev_info(dev)->real_dev;

	dev_queue_xmit(skb);

	return 0;
}		
/* -----  end of function vnic_dev_xmit  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_dev_set_mac_address
 *  Description:  
 * =====================================================================================
 */
int
vnic_dev_set_mac_address (struct net_device *dev, void *address)
{

	struct sockaddr *addr = (struct sockaddr *)(address);

	if (netif_running(dev))
		return -EBUSY;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	printk(KERN_INFO "%s: Set Mac address for %s .\n", __FUNCTION__, dev->name);

	return 0;
}		
/* -----  end of function vnic_dev_set_mac_address  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_dev_init
 *  Description:  
 * =====================================================================================
 */
static int
vnic_dev_init (struct net_device *dev)
{
	struct net_device* real_dev = vnic_dev_info(dev)->real_dev;

	printk(KERN_INFO "%s: dev = %s, real_dev = %s .\n", __FUNCTION__, dev->name, real_dev->name);

	memcpy(dev->dev_addr, real_dev->dev_addr, dev->addr_len);

	return 0;
}

/* -----  end of function vnic_dev_init  ----- */


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_dev_uninit
 *  Description:  
 * =====================================================================================
 */
static void
vnic_dev_uninit (struct net_device *dev)
{
}	

/* -----  end of function vnic_dev_uninit  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_dev_open
 *  Description:  
 * =====================================================================================
 */
static int
vnic_dev_open (struct net_device *dev)
{
	if (!(vnic_dev_info(dev)->real_dev->flags & IFF_UP))
		return -ENETDOWN;

	return 0;
}	
/* -----  end of function vnic_dev_open  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  vnic_netdev_setup
 *  Description:  
 * =====================================================================================
 */
void
vnic_netdev_setup (struct net_device *dev)
{
	SET_MODULE_OWNER(dev);
	ether_setup(dev);

	dev->priv_flags      |= IFF_VNIC;
	dev->tx_queue_len     = 0;

	dev->open             = vnic_dev_open;
	dev->init             = vnic_dev_init;
	dev->uninit           = vnic_dev_uninit;
	dev->hard_start_xmit  = vnic_dev_hard_start_xmit;
	dev->set_mac_address  = vnic_dev_set_mac_address;
	dev->destructor       = free_netdev;

}	

/* -----  end of function vnic_netdev_setup  ----- */
