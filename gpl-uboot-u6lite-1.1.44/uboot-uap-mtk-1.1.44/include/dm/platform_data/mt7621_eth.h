#ifndef _PLAT_MT7621_ETH_H_
#define _PLAT_MT7621_ETH_H_

#ifdef CONFIG_DM_ETH

#include <net.h>

struct mt7621_eth_pdata
{
	struct eth_pdata eth_pdata;
	void __iomem *fe_base;
	void __iomem *gmac_base;
};

#endif

#endif /* _PLAT_MT7621_ETH_H_ */
