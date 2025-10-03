/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Intel Corporation */

#ifndef D457_H
#define D457_H

#include <media/v4l2-mediabus.h>

#define D457_NAME "d4xx"
#define MAX9296_NAME "MAX9296"
#define MAX96724_NAME "MAX96724"

#define D457_I2C_ADDRESS 0x10

#define d4xx_subdev_csi_link_id(link) \
 link == GMSL_SERDES_CSI_LINK_A ? "GMSL A" : \
 GMSL_SERDES_CSI_LINK_B ? "GMSL B" : \
 GMSL_SERDES_CSI_LINK_C ? "GMSL C" : "GMSL D"

struct d4xx_subdev_info {
	struct i2c_board_info board_info;
	int i2c_adapter_id;
	unsigned short rx_port;
	unsigned short phy_i2c_addr;
	unsigned short ser_alias;
	char suffix[5]; /* suffix for subdevs */
	int aggregated_link;
	unsigned short ser_phys_addr;
};

struct d4xx_pdata {
	unsigned int subdev_num;
	struct d4xx_subdev_info *subdev_info;
	unsigned int reset_gpio;
	int FPD_gpio;
	const char suffix;
	unsigned int link_freq_mbps;
	unsigned int deser_nlanes;
	unsigned int ser_nlanes;
	struct i2c_board_info *deser_board_info;
	unsigned int des_port;
};
#endif
