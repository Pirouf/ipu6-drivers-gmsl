/*
 * max96712.h - Maxim 96712 registers and constants.
 *
 * Copyright (c) 2020, D3 Engineering. All rights reserved.
 * Copyright (c) 2024, Define Design Deploy Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MAX96712_H_
#define _MAX96712_H_

#include <linux/bitops.h>
#include "serdes.h"

enum max96712_rev {
	MAX96712_REV_A = 1,
	MAX96712_REV_B,
	MAX96712_REV_C,
	MAX96712_REV_D,
	MAX96712_REV_E,
};

enum max96712_phy_mode {
	MAX96712_MIPI_PHY_4X2 = BIT(0), // four 2-lane ports
	MAX96712_MIPI_PHY_1X4 = BIT(1), // one 4-lane (CSI2 is master (TODO: Which port???))
	MAX96712_MIPI_PHY_2X4 = BIT(2), // two 4-lane ports (CSI1 is master for port A, CSI2 for port B)
	MAX96712_MIPI_PHY_1X4A_22 = BIT(3), // one 4-lane (PHY0+PHY1, CSI1 for port A) port and two 2-lane ports
	MAX96712_MIPI_PHY_1X4B_22 = BIT(4), // one 4-lane (PHY2+PHY3, CSI2 for port B) port and two 2-lane ports
};

#define MAX96712_NUM_SERIAL_LINKS 4
#define MAX96712_NUM_VIDEO_PIPES 8
#define MAX96712_NUM_MIPI_MAPS 16
#define MAX96712_NUM_CSI_LINKS 4

#define MAX96712_DEFAULT_SERIAL_LINK_TIMEOUT_MS 200

#define MAX96712_DPLL_FREQ_MHZ_MULTIPLE 100


#define MAX96712_FLD_OFS(n, bits_per_field, count) (((n) % (count)) * (bits_per_field))
#define MAX96712_OFFSET_GENMASK(offset, h, l) GENMASK(offset + h, offset + l)

#define MAX96712_REM_CC 0x03
#define MAX96712_REM_CC_DIS_PORT_FIELD(link, port) BIT(MAX96712_FLD_OFS(link, 2, 4) + (port % 2))
#define MAX96712_LINK_CTRL 0x06
#define MAX96712_LINK_CTRL_GMSL_FIELD(link) BIT(link) << 4
#define MAX96712_LINK_CTRL_EN_FIELD(link) BIT(link)
#define MAX96712_PHY_RATE_CTRL(link) (0x10 + ((link) / 2))
#define MAX96712_PHY_RATE_CTRL_TX_FIELD(link) GENMASK(1, 0) << (MAX96712_FLD_OFS(link, 4, 2) + 2)
#define MAX96712_PHY_RATE_CTRL_RX_FIELD(link) GENMASK(1, 0) << (MAX96712_FLD_OFS(link, 4, 2) + 0)

#define MAX96712_PHY_LOCKED(link) ((link) == 0 ? 0x1A : 0x0A + ((link) - 1))
#define MAX96712_PHY_LOCKED_FIELD BIT(3)
#define MAX96712_RESET_ALL 0x13
#define MAX96712_RESET_ALL_FIELD BIT(6)
#define MAX96712_RESET_CTRL 0x18
#define MAX96712_RESET_CTRL_FIELD(link) BIT(link)
#define MAX96712_DEV_REV 0x4C
#define MAX96712_DEV_REV_FIELD GENMASK(3, 0)

#define MAX96712_VIDEO_PIPE_SEL(pipe) (0xF0 + ((pipe) / 2))
#define MAX96712_VIDEO_PIPE_SEL_LINK_FIELD(link) GENMASK(1, 0) << (MAX96712_FLD_OFS(link, 4, 2) + 2)
#define MAX96712_VIDEO_PIPE_SEL_INPUT_FIELD(link) GENMASK(1, 0) << (MAX96712_FLD_OFS(link, 4, 2) + 0)

#define MAX96712_VIDEO_PIPE_EN(pipe) (0xF4)
#define MAX96712_VIDEO_PIPE_EN_FIELD(pipe) BIT(pipe)

#define MAX96712_DPLL_FREQ(phy) (0x415 + ((phy) * 3))
#define MAX96712_DPLL_FREQ_FIELD GENMASK(4, 0)

#define MAX96712_MIPI_TX_EXT(pipe) (0x800 + ((pipe) * 0x10))

#define MAX96712_MIPI_PHY0 0x8A0
#define MAX96712_MIPI_PHY0_MODE_FIELD GENMASK(4, 0)
#define MAX96712_MIPI_PHY_ENABLE 0x8A2
#define MAX96712_MIPI_PHY_ENABLE_FIELD(csi) BIT((csi) + 4)
#define MAX96712_MIPI_PHY_LANE_MAP(csi) (0x8A3 + (csi) / 2)
#define MAX96712_MIPI_PHY_LANE_MAP_FIELD(csi, lane) GENMASK(1, 0) << (MAX96712_FLD_OFS(csi, 4, 2) + MAX96712_FLD_OFS(lane, 2, 2))

// Note that CSIs and pipes overlap:
// 0x901 thru 0x90A are CSIs, repeated every 0x40 up to 4 times
// 0x90B thru 0x934 are pipes, repeated every 0x40 up to 8 times
#define MAX96712_MIPI_TX(pipe) (0x900 + ((pipe) * 0x40))
#define MAX96712_MIPI_TX_LANE_CNT(csi) (MAX96712_MIPI_TX(csi) + 0x0A)
#define MAX96712_MIPI_TX_LANE_CNT_FIELD GENMASK(7, 6)
#define MAX96712_MIPI_TX_DESKEW_INIT(csi) (MAX96712_MIPI_TX(csi) + 0x03)
#define MAX96712_MIPI_TX_DESKEW_INIT_AUTO_EN BIT(7)
#define MAX96712_MAP_EN_L(pipe) (MAX96712_MIPI_TX(pipe) + 0x0B)
#define MAX96712_MAP_EN_H(pipe) (MAX96712_MIPI_TX(pipe) + 0x0C)
#define MAX96712_MAP_EN_FIELD GENMASK(7, 0)
#define MAX96712_MAP_SRC_L(pipe, map) (MAX96712_MIPI_TX(pipe) + 0x0D + ((map) * 2))
#define MAX96712_MAP_SRC_L_VC_FIELD GENMASK(7, 6)
#define MAX96712_MAP_SRC_L_DT_FIELD GENMASK(5, 0)
#define MAX96712_MAP_DST_L(pipe, map) (MAX96712_MIPI_TX(pipe) + 0x0D + ((map) * 2) + 1)
#define MAX96712_MAP_DST_L_VC_FIELD GENMASK(7, 6)
#define MAX96712_MAP_DST_L_DT_FIELD GENMASK(5, 0)
#define MAX96712_MAP_SRCDST_H(pipe, map) (MAX96712_MIPI_TX_EXT(pipe) + (map))
#define MAX96712_MAP_SRCDST_H_SRC_VC_FIELD GENMASK(7, 5)
#define MAX96712_MAP_SRCDST_H_DST_VC_FIELD GENMASK(4, 2)
#define MAX96712_MAP_DPHY_DEST(pipe, map) (MAX96712_MIPI_TX(pipe) + 0x2D + ((map) / 4))
#define MAX96712_MAP_DPHY_DEST_FIELD(map) GENMASK(1, 0) << MAX96712_FLD_OFS(map, 2, 4)

#define MAX96712_MIPI_TX_ALT_MEM(csi) (MAX96712_MIPI_TX(csi) + 0x33)
#define MAX96712_MIPI_TX_ALT_MEM_FIELD GENMASK(2, 0)
#define MAX96712_MIPI_TX_ALT_MEM_8BPP BIT(1)
#define MAX96712_MIPI_TX_ALT_MEM_10BPP BIT(2)
#define MAX96712_MIPI_TX_ALT_MEM_12BPP BIT(0)

#define MAX96712_DPLL_RESET(phy) (0x1C00 + ((phy) * 0x100))
#define MAX96712_DPLL_RESET_SOFT_RST_FIELD BIT(0)

// VID_TX adjusts slightly between #4 and #5!
#define MAX96712_VID_TX(pipe) ((pipe <= 4) \
	? 0x100 + (0x12 * (pipe)) \
	: 0x160 + (0x12 * ((pipe) - 4)))

#define MAX96712_GET_GPIO_REG(gpio_num) (0x300 + 3 * gpio_num)
#define MAX96712_REG_GPIO2_A 0x306

static const struct reg_sequence max96712_phy_tuning_revABC[] = {
	// PHY A
	{ 0x1458, 0x28, },
	{ 0x1459, 0x68, },
	{ 0x143E, 0xB3, },
	{ 0x143F, 0x72, },
	// PHY B
	{ 0x1558, 0x28, },
	{ 0x1559, 0x68, },
	{ 0x153E, 0xB3, },
	{ 0x153F, 0x72, },
	// PHY C
	{ 0x1658, 0x28, },
	{ 0x1659, 0x68, },
	{ 0x163E, 0xB3, },
	{ 0x163F, 0x72, },
	// PHY D
	{ 0x1758, 0x28, },
	{ 0x1759, 0x68, },
	{ 0x173E, 0xB3, },
	{ 0x173F, 0x72, },
};

static const struct reg_sequence max96712_phy_tuning_revE[] = {
	// Increase CMU regulator output voltage (bit 4)
	{ 0x06C2, 0x10, },
	// Set VgaHiGain_Init_6G (bit 1) and VgaHiGain_Init_3G (bit 0)* for PHY A/B/C/D
	{ 0x14D1, 0x03, },
	{ 0x15D1, 0x03, },
	{ 0x16D1, 0x03, },
	{ 0x17D1, 0x03, },
};

static struct max9x_serdes_rate_table max96712_rx_rates[] = {
	{ .val = 1, .freq_mhz = 3000}, // 3 GHz
	{ .val = 2, .freq_mhz = 6000}, // 6 GHz
};

static struct max9x_serdes_rate_table max96712_tx_rates[] = {
	{ .val = 0, .freq_mhz = 187}, // 187.5 MHz
};

#endif /* _MAX96712_H_ */
