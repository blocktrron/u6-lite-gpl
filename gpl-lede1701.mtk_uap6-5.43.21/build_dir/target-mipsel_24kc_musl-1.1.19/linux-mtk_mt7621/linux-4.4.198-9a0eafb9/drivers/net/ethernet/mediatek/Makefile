#
# Makefile for the Mediatek SoCs built-in ethernet macs
#

ccflags-y += -O3
obj-$(CONFIG_NET_MEDIATEK_SOC)			+= mtk_eth_soc.o mtk_sgmii.o \
						   mtk_eth_path.o
obj-$(CONFIG_NET_MEDIATEK_DBG)			+= mtk_eth_dbg.o
obj-$(CONFIG_NET_MEDIATEK_HNAT)                 += mtk_hnat/
