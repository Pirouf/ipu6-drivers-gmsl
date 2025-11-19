/*
 * max96712.c - Maxim MAX96712 GMSL2/GMSL1 to CSI-2 Deserializer
 *
 * Copyright (c) 2020, D3 Engineering.  All rights reserved.
 * Copyright (c) 2023, Define Design Deploy Corp.  All rights reserved.
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

#include "max96712.h"

// Params
int max96712_serial_link_timeout_ms = MAX96712_DEFAULT_SERIAL_LINK_TIMEOUT_MS;
module_param(max96712_serial_link_timeout_ms, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(max96712_serial_link_timeout_ms, "Timeout for serial link in milliseconds");

static const struct regmap_config max96712_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
};

/* Log all register writes */
#define MAX96712_WRITE_REG(map, reg, val) \
({ \
	int __ret; \
	struct regmap *__map = (map); \
	unsigned int __reg = (reg); \
	unsigned int __val = (val); \
	dev_info(regmap_get_device(__map), "REG_WRITE: [0x%04X] = 0x%02X", __reg, __val); \
	__ret = regmap_write(__map, __reg, __val); \
	if (__ret) \
		dev_err(regmap_get_device(__map), "REG_WRITE FAILED: [0x%04X] = 0x%02X, ret=%d", __reg, __val, __ret); \
	__ret; \
})

/* Log register update_bits (read-modify-write) */
#define MAX96712_UPDATE_BITS(map, reg, mask, val) \
({ \
	int __ret; \
	struct regmap *__map = (map); \
	unsigned int __reg = (reg); \
	unsigned int __mask = (mask); \
	unsigned int __val = (val); \
	unsigned int __old_val = 0; \
	unsigned int __new_val = 0; \
	regmap_read(__map, __reg, &__old_val); \
	__new_val = (__old_val & ~__mask) | (__val & __mask); \
	dev_info(regmap_get_device(__map), "REG_UPDATE_BITS: [0x%04X] mask=0x%02X, old=0x%02X, new=0x%02X (val=0x%02X)", \
		__reg, __mask, __old_val, __new_val, __val); \
	__ret = regmap_update_bits(__map, __reg, __mask, __val); \
	if (__ret) \
		dev_err(regmap_get_device(__map), "REG_UPDATE_BITS FAILED: [0x%04X] mask=0x%02X val=0x%02X, ret=%d", \
			__reg, __mask, __val, __ret); \
	__ret; \
})

// Declarations
static int max96712_set_phy_mode(struct max9x_common *common, unsigned phy_mode);
static int max96712_set_phy_enabled(struct max9x_common *common, unsigned csi_id, bool enable);
static int max96712_set_phy_lane_map(struct max9x_common *common, unsigned csi_id, unsigned phy_lane_map);
static int max96712_set_phy_dpll_enabled(struct max9x_common *common, unsigned csi_id, bool enable);
static int max96712_set_phy_dpll_freq(struct max9x_common *common, unsigned csi_id, unsigned freq_mhz);
static int max96712_set_mipi_lane_cnt(struct max9x_common *common, unsigned csi_id, int num_lanes);
static int max96712_set_initial_deskew(struct max9x_common *common, unsigned csi_id, bool enable);
static int max96712_configure_csi_dphy(struct max9x_common *common);
static int max96712_phy_tuning(struct max9x_common *common);
static int max96712_enable(struct max9x_common *common);
static int max96712_max_elements(struct max9x_common *common, enum max9x_element_type element);
static int max96712_get_serial_link_lock(struct max9x_common *common, unsigned link_id, bool *locked);
static int max96712_set_serial_link_state(struct max9x_common *common, unsigned link_id, bool enable);
static int max96712_serial_link_reset(struct max9x_common *common, unsigned link_id);
static int max96712_set_serial_link_rate(struct max9x_common *common, unsigned link_id);
static int max96712_set_video_pipe_src(struct max9x_common *common, unsigned pipe_id, unsigned link_id, unsigned src_pipe);
static int max96712_set_video_pipe_maps_enabled(struct max9x_common *common, unsigned pipe_id, int num_maps);
static int max96712_set_video_pipe_map(struct max9x_common *common, unsigned pipe_id, unsigned map_id, struct max9x_serdes_mipi_map *mipi_map);
static int max96712_set_csi_link_enabled(struct max9x_common *common, unsigned csi_id, bool enable);
static int max96712_csi_double_pixel(struct max9x_common *common, unsigned csi_id, unsigned bpp);
static int max96712_set_video_pipe_enabled(struct max9x_common *common, unsigned pipe_id, bool enable);
static int max96712_set_serial_link_routing(struct max9x_common *common, unsigned link_id);
static int max96712_disable_serial_link(struct max9x_common *common, unsigned link_id);
static int max96712_enable_serial_link(struct max9x_common *common, unsigned link_id);
static int max96712_set_remote_control_channel_enabled(struct max9x_common *common, unsigned link_id, bool enabled);
static int max96712_select_serial_link(struct max9x_common *common, unsigned link);
static int max96712_deselect_serial_link(struct max9x_common *common, unsigned link);

/* Currently unused */
static int max96712_enable_frame_sync(struct max9x_common *common);


// Functions
static int max96712_set_phy_mode(struct max9x_common *common, unsigned phy_mode)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI: phy_mode=%d", phy_mode);

	return MAX96712_UPDATE_BITS(map, MAX96712_MIPI_PHY0,
		MAX96712_MIPI_PHY0_MODE_FIELD,
		MAX9X_FIELD_PREP(MAX96712_MIPI_PHY0_MODE_FIELD, phy_mode));
}

static int max96712_set_phy_enabled(struct max9x_common *common, unsigned csi_id, bool enable)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI link %d: %s", csi_id, (enable ? "enable" : "disable"));

	return MAX96712_UPDATE_BITS(map, MAX96712_MIPI_PHY_ENABLE,
		MAX96712_MIPI_PHY_ENABLE_FIELD(csi_id),
		MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_ENABLE_FIELD(csi_id), enable ? 1U : 0U));
}

static int max96712_set_phy_lane_map(struct max9x_common *common, unsigned csi_id, unsigned phy_lane_map)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI link %d: phy_lane_map=0x%04x", csi_id, phy_lane_map);

	return MAX96712_UPDATE_BITS(map, MAX96712_MIPI_PHY_LANE_MAP(csi_id),
		MAX96712_MIPI_PHY_LANE_MAP_FIELD(csi_id, 0)
		| MAX96712_MIPI_PHY_LANE_MAP_FIELD(csi_id, 1),
		phy_lane_map);
}

static int max96712_set_phy_dpll_enabled(struct max9x_common *common, unsigned csi_id, bool enable)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI link %d: DPLL %s", csi_id, (enable ? "on" : "off"));

	return MAX96712_UPDATE_BITS(map, MAX96712_DPLL_RESET(csi_id),
		MAX96712_DPLL_RESET_SOFT_RST_FIELD,
		MAX9X_FIELD_PREP(MAX96712_DPLL_RESET_SOFT_RST_FIELD, enable ? 1U : 0U));
}

static int max96712_set_phy_dpll_freq(struct max9x_common *common, unsigned csi_id, unsigned freq_mhz)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI link %d: freq %u MHz, %u mult", csi_id, freq_mhz, freq_mhz / MAX96712_DPLL_FREQ_MHZ_MULTIPLE);

	return MAX96712_UPDATE_BITS(map, MAX96712_DPLL_FREQ(csi_id),
		MAX96712_DPLL_FREQ_FIELD,
		MAX9X_FIELD_PREP(MAX96712_DPLL_FREQ_FIELD, freq_mhz / MAX96712_DPLL_FREQ_MHZ_MULTIPLE));
}


static int max96712_set_mipi_lane_cnt(struct max9x_common *common, unsigned csi_id, int num_lanes)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI link %d: %d lanes", csi_id, num_lanes);

	return MAX96712_UPDATE_BITS(map, MAX96712_MIPI_TX_LANE_CNT(csi_id),
		MAX96712_MIPI_TX_LANE_CNT_FIELD,
		MAX9X_FIELD_PREP(MAX96712_MIPI_TX_LANE_CNT_FIELD,
			(common->csi_link[csi_id].config.num_lanes - 1)));
}

static int max96712_set_initial_deskew(struct max9x_common *common, unsigned csi_id, bool enable)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "CSI link %d: Initial deskew %s", csi_id, enable ? "enabled" : "disabled");

	return MAX96712_WRITE_REG(map, MAX96712_MIPI_TX_DESKEW_INIT(csi_id),
			MAX9X_FIELD_PREP(MAX96712_MIPI_TX_DESKEW_INIT_AUTO_EN, enable));
}

static int max96712_configure_csi_dphy(struct max9x_common *common)
{
	struct device *dev = common->dev;
	unsigned phy_mode;
	unsigned phy_lane_map[MAX96712_NUM_CSI_LINKS];
	unsigned csi_id;
	int ret;

	for (csi_id = 0; csi_id < MAX96712_NUM_CSI_LINKS; csi_id++) {
		dev_dbg(dev, "CSI link %d: enabled=%d, num_lanes=%d, freq_mhz=%d init_deskew=%d",
			csi_id,
			common->csi_link[csi_id].enabled,
			common->csi_link[csi_id].config.num_lanes,
			common->csi_link[csi_id].config.freq_mhz,
			common->csi_link[csi_id].config.auto_init_deskew_enabled);
	}

	//TODO: Allow DT to override lane mapping?

	// Determine correct phy_mode and associate lane mapping
	if (common->csi_link[0].config.num_lanes <= 2
			&& common->csi_link[1].config.num_lanes <= 2
			&& common->csi_link[2].config.num_lanes <= 2
			&& common->csi_link[3].config.num_lanes <= 2) {

		phy_mode = MAX96712_MIPI_PHY_4X2;

		phy_lane_map[0] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 1), 1);
		phy_lane_map[1] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 1), 1);
		phy_lane_map[2] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 1), 1);
		phy_lane_map[3] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 1), 1);

	} else if (common->csi_link[0].config.num_lanes == 0
			&& common->csi_link[1].config.num_lanes >= 3
			&& common->csi_link[2].config.num_lanes >= 3
			&& common->csi_link[3].config.num_lanes == 0) {

		phy_mode = MAX96712_MIPI_PHY_2X4;

		phy_lane_map[0] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 1), 1);
		phy_lane_map[1] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 0), 2)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 1), 3);
		phy_lane_map[2] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 1), 1);
		phy_lane_map[3] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 0), 2)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 1), 3);

	} else if (common->csi_link[0].config.num_lanes == 0
			&& common->csi_link[1].config.num_lanes >= 3
			&& common->csi_link[2].config.num_lanes <= 2
			&& common->csi_link[3].config.num_lanes <= 2) {

		phy_mode = MAX96712_MIPI_PHY_1X4A_22;

		phy_lane_map[0] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 1), 1);
		phy_lane_map[1] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 0), 2)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 1), 3);
		phy_lane_map[2] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 1), 1);
		phy_lane_map[3] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 1), 1);

	} else if (common->csi_link[0].config.num_lanes <= 2
			&& common->csi_link[1].config.num_lanes <= 2
			&& common->csi_link[2].config.num_lanes >= 3
			&& common->csi_link[3].config.num_lanes == 0) {

		phy_mode = MAX96712_MIPI_PHY_1X4B_22;

		phy_lane_map[0] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(0, 1), 1);
		phy_lane_map[1] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(1, 1), 1);
		phy_lane_map[2] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 0), 0)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(2, 1), 1);
		phy_lane_map[3] = MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 0), 2)
				| MAX9X_FIELD_PREP(MAX96712_MIPI_PHY_LANE_MAP_FIELD(3, 1), 3);

	} else {
		dev_err(dev, "Invalid CSI configuration! Supported modes: 4x2, 2x4, 1x4+2x2, 2x2+1x4");
		return -EINVAL;
	}

	ret = max96712_set_phy_mode(common, phy_mode);
	if (ret)
		return ret;

	for (csi_id = 0; csi_id < MAX96712_NUM_CSI_LINKS; csi_id++) {
		struct max9x_serdes_csi_config *config = &common->csi_link[csi_id].config;

		ret = max96712_set_phy_enabled(common, csi_id, false);
		if (ret)
			return ret;

		ret = max96712_set_phy_dpll_enabled(common, csi_id, false);
		if (ret)
			return ret;

		ret = max96712_set_phy_lane_map(common, csi_id, phy_lane_map[csi_id]);
		if (ret)
			return ret;

		ret = max96712_set_mipi_lane_cnt(common, csi_id, common->csi_link[csi_id].config.num_lanes);
		if (ret)
			return ret;

		ret = max96712_set_initial_deskew(common, csi_id, common->csi_link[csi_id].config.auto_init_deskew_enabled);
		if (ret)
			return ret;

		if (WARN_ONCE(config->freq_mhz > 0 && config->freq_mhz < MAX96712_DPLL_FREQ_MHZ_MULTIPLE, "CSI frequency must be greater than %d MHz", MAX96712_DPLL_FREQ_MHZ_MULTIPLE))
			return -EINVAL;

		ret = max96712_set_phy_dpll_freq(common, csi_id, config->freq_mhz);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96712_phy_tuning(struct max9x_common *common)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	int ret;
	unsigned rev;

	ret = regmap_read(map, MAX96712_DEV_REV, &rev);
	if (ret)
		return ret;

	switch (FIELD_GET(MAX96712_DEV_REV_FIELD, rev)) {
		case MAX96712_REV_A:
		case MAX96712_REV_B:
		case MAX96712_REV_C:
			ret = regmap_multi_reg_write(map, max96712_phy_tuning_revABC, ARRAY_SIZE(max96712_phy_tuning_revABC));
			break;
		case MAX96712_REV_D:
			// No tuning necessary
			break;
		case MAX96712_REV_E:
			ret = regmap_multi_reg_write(map, max96712_phy_tuning_revE, ARRAY_SIZE(max96712_phy_tuning_revE));
			break;
		default:
			dev_warn(dev, "Unknown chip revision");
			break;
	}

	return ret;
}

static int max96712_set_all_reset(struct max9x_common *common, bool enable)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "Reset %s", (enable ? "enable" : "disable"));

	return MAX96712_UPDATE_BITS(map, MAX96712_RESET_ALL,
		MAX96712_RESET_ALL_FIELD,
		MAX9X_FIELD_PREP(MAX96712_RESET_ALL_FIELD, enable ? 1U : 0U));
}

static int max96712_enable(struct max9x_common *common)
{
	struct device *dev = common->dev;
	int link_id;
	int ret;

	dev_dbg(dev, "Enable");

	for (link_id = 0; link_id < common->num_serial_links; link_id++) {
		ret = max96712_disable_serial_link(common, link_id);
		if (ret)
			return ret;
	}

	ret = max96712_phy_tuning(common);
	if (ret)
		return ret;

	ret = max96712_configure_csi_dphy(common);
	if (ret)
		return ret;

	return 0;
}

static int max96712_soft_reset(struct max9x_common *common)
{
	struct device *dev = common->dev;
	int ret;

	dev_dbg(dev, "Soft reset");

	ret = max96712_set_all_reset(common, 1);
	if (ret)
		return ret;

	msleep(1);

	ret = max96712_set_all_reset(common, 0);
	if (ret)
		return ret;

	return 0;
}

static int max96712_max_elements(struct max9x_common *common, enum max9x_element_type element)
{
	switch (element) {
		case MAX9X_SERIAL_LINK:
			return MAX96712_NUM_SERIAL_LINKS;
		case MAX9X_VIDEO_PIPE:
			return MAX96712_NUM_VIDEO_PIPES;
		case MAX9X_MIPI_MAP:
			return MAX96712_NUM_MIPI_MAPS;
		case MAX9X_CSI_LINK:
			return MAX96712_NUM_CSI_LINKS;
		default:
			break;
	}

	return 0;
}

static struct max9x_common_ops max96712_common_ops = {
	.enable = max96712_enable,
	.soft_reset = max96712_soft_reset,
	.max_elements = max96712_max_elements,
};

static int max96712_get_serial_link_lock(struct max9x_common *common, unsigned link_id, bool *locked)
{
	struct regmap *map = common->map;
	unsigned val;
	int ret;

	ret = regmap_read(map, MAX96712_PHY_LOCKED(link_id), &val);
	if (ret)
		return ret;

	if (FIELD_GET(MAX96712_PHY_LOCKED_FIELD, val) != 0)
		*locked = true;
	else
		*locked = false;

	return 0;
}

static int max96712_set_serial_link_state(struct max9x_common *common, unsigned link_id, bool enable)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	enum max9x_serdes_link_type link_type = common->serial_link[link_id].config.link_type;

	dev_dbg(dev, "Serial-link %d: %s", link_id, (enable ? "up" : "down"));

	return MAX96712_UPDATE_BITS(map, MAX96712_LINK_CTRL,
		MAX96712_LINK_CTRL_EN_FIELD(link_id)
		| MAX96712_LINK_CTRL_GMSL_FIELD(link_id),
		MAX9X_FIELD_PREP(MAX96712_LINK_CTRL_EN_FIELD(link_id), enable ? 1U : 0U)
		| MAX9X_FIELD_PREP(MAX96712_LINK_CTRL_GMSL_FIELD(link_id), link_type == MAX9X_LINK_TYPE_GMSL2 ? 1U : 0U));
}

static int max96712_serial_link_reset(struct max9x_common *common, unsigned link_id)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "Serial-link %d reset", link_id);

	return MAX96712_UPDATE_BITS(map, MAX96712_RESET_CTRL,
		MAX96712_RESET_CTRL_FIELD(link_id),
		MAX9X_FIELD_PREP(MAX96712_RESET_CTRL_FIELD(link_id), 1U));
}

static int max96712_set_serial_link_rate(struct max9x_common *common, unsigned link_id)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	struct max9x_serdes_serial_config *config = &common->serial_link[link_id].config;
	unsigned tx_rate, rx_rate;

	tx_rate = max9x_serdes_mhz_to_rate(max96712_tx_rates, ARRAY_SIZE(max96712_tx_rates), config->tx_freq_mhz);
	if (tx_rate < 0)
		return tx_rate;

	rx_rate = max9x_serdes_mhz_to_rate(max96712_rx_rates, ARRAY_SIZE(max96712_rx_rates), config->rx_freq_mhz);
	if (rx_rate < 0)
		return rx_rate;

	dev_dbg(dev, "Serial-link %d: TX=%d MHz RX=%d MHz", link_id, config->tx_freq_mhz, config->rx_freq_mhz);

	return MAX96712_UPDATE_BITS(map, MAX96712_PHY_RATE_CTRL(link_id),
		MAX96712_PHY_RATE_CTRL_TX_FIELD(link_id)
		| MAX96712_PHY_RATE_CTRL_RX_FIELD(link_id),
		MAX9X_FIELD_PREP(MAX96712_PHY_RATE_CTRL_TX_FIELD(link_id), tx_rate)
		| MAX9X_FIELD_PREP(MAX96712_PHY_RATE_CTRL_RX_FIELD(link_id), rx_rate));
}

static int max96712_set_video_pipe_src(struct max9x_common *common, unsigned pipe_id, unsigned link_id, unsigned src_pipe)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "Video-pipe %d: src_link=%u, src_pipe=%u", pipe_id, link_id, src_pipe);

	return MAX96712_UPDATE_BITS(map, MAX96712_VIDEO_PIPE_SEL(pipe_id),
		MAX96712_VIDEO_PIPE_SEL_LINK_FIELD(pipe_id)
		| MAX96712_VIDEO_PIPE_SEL_INPUT_FIELD(pipe_id),
		MAX9X_FIELD_PREP(MAX96712_VIDEO_PIPE_SEL_LINK_FIELD(pipe_id), link_id)
		| MAX9X_FIELD_PREP(MAX96712_VIDEO_PIPE_SEL_INPUT_FIELD(pipe_id), src_pipe));
}

static int max96712_set_video_pipe_maps_enabled(struct max9x_common *common, unsigned pipe_id, int num_maps)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	unsigned val = 0;
	int ret;

	if (num_maps > 0)
		val = GENMASK(num_maps - 1, 0);

	dev_dbg(dev, "Video-pipe %d: num_maps=%u", pipe_id, num_maps);

	ret = MAX96712_WRITE_REG(map, MAX96712_MAP_EN_L(pipe_id),
		MAX9X_FIELD_PREP(MAX96712_MAP_EN_FIELD, val));
	if (ret)
		return ret;

	ret = MAX96712_WRITE_REG(map, MAX96712_MAP_EN_H(pipe_id),
		MAX9X_FIELD_PREP(MAX96712_MAP_EN_FIELD, val >> 8));
	if (ret)
		return ret;

	return 0;
}

static int max96712_set_video_pipe_map(struct max9x_common *common, unsigned pipe_id, unsigned map_id, struct max9x_serdes_mipi_map *mipi_map)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	int ret;

	dev_dbg(dev, "Video-pipe %d, map %d: VC%d:DT%02x->VC%d:DT%02x, dst_csi=%d ",
		pipe_id, map_id,
		mipi_map->src_vc,
		mipi_map->src_dt,
		mipi_map->dst_vc,
		mipi_map->dst_dt,
		mipi_map->dst_csi);

	ret = MAX96712_WRITE_REG(map, MAX96712_MAP_SRC_L(pipe_id, map_id),
		MAX9X_FIELD_PREP(MAX96712_MAP_SRC_L_VC_FIELD, mipi_map->src_vc)
		| MAX9X_FIELD_PREP(MAX96712_MAP_SRC_L_DT_FIELD, mipi_map->src_dt));
	if (ret)
		return ret;

	ret = MAX96712_WRITE_REG(map, MAX96712_MAP_DST_L(pipe_id, map_id),
		MAX9X_FIELD_PREP(MAX96712_MAP_DST_L_VC_FIELD, mipi_map->dst_vc)
		| MAX9X_FIELD_PREP(MAX96712_MAP_DST_L_DT_FIELD, mipi_map->dst_dt));
	if (ret)
		return ret;

	ret = MAX96712_WRITE_REG(map, MAX96712_MAP_SRCDST_H(pipe_id, map_id),
		MAX9X_FIELD_PREP(MAX96712_MAP_SRCDST_H_SRC_VC_FIELD, mipi_map->src_vc)
		| MAX9X_FIELD_PREP(MAX96712_MAP_SRCDST_H_DST_VC_FIELD, mipi_map->dst_vc));
	if (ret)
		return ret;

	ret = MAX96712_UPDATE_BITS(map, MAX96712_MAP_DPHY_DEST(pipe_id, map_id),
		MAX96712_MAP_DPHY_DEST_FIELD(map_id),
		MAX9X_FIELD_PREP(MAX96712_MAP_DPHY_DEST_FIELD(map_id), mipi_map->dst_csi));
	if (ret)
		return ret;

	return 0;
}

static int max96712_set_csi_link_enabled(struct max9x_common *common, unsigned csi_id, bool enable)
{
	struct device *dev = common->dev;
	struct max9x_serdes_csi_link *csi_link;
	int ret;

	if (csi_id > common->num_csi_links)
		return -EINVAL;

	csi_link = &common->csi_link[csi_id];

	if (WARN_ONCE(enable && csi_link->enabled == false, "Tried to enable a disabled CSI port???"))
		return -EINVAL;

	if (WARN_ONCE(enable && csi_link->config.num_lanes == 0, "Tried to enable CSI port with no lanes???"))
		return -EINVAL;

	mutex_lock(&csi_link->csi_mutex);

	dev_dbg(dev, "CSI link %d: %s (%d users)", csi_id, (enable ? "enable" : "disable"), csi_link->usecount);

	if (enable && csi_link->usecount == 0) {
		// Enable && first user

		ret = max96712_set_phy_dpll_enabled(common, csi_id, true);
		if (ret)
			return ret;

		ret = max96712_set_phy_enabled(common, csi_id, true);
		if (ret)
			return ret;

	} else if (!enable && csi_link->usecount == 1) {
		// Disable && no more users

		ret = max96712_set_phy_enabled(common, csi_id, false);
		if (ret)
			return ret;

		ret = max96712_set_phy_dpll_enabled(common, csi_id, false);
		if (ret)
			return ret;

	}

	// Keep track of number of enabled maps using this CSI link
	if (enable)
		csi_link->usecount++;
	else if (csi_link->usecount > 0)
		csi_link->usecount--;

	mutex_unlock(&csi_link->csi_mutex);

	return 0;
}

static int max96712_csi_double_pixel(struct max9x_common *common, unsigned csi_id, unsigned bpp)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	unsigned value;

	dev_dbg(dev, "CSI ALT Mem mapping for %u bpp on csi %i", bpp, csi_id);

	switch (bpp) {
		case 0:
			value = 0;
			break;
		case 8:
			value =	FIELD_PREP(MAX96712_MIPI_TX_ALT_MEM_8BPP, 1U);
			dev_err(dev, "8 BPP currently unsupported for pixel doubling");
			return -EINVAL;
			break;
		case 10:
			value =	FIELD_PREP(MAX96712_MIPI_TX_ALT_MEM_10BPP, 1U);
			break;
		case 12:
			value =	FIELD_PREP(MAX96712_MIPI_TX_ALT_MEM_12BPP, 1U);
			break;
		default:
			dev_err(dev, "Unsupported BPP for pixel doubling: %u", bpp);
			return -EINVAL;
	}

	// Enable alt mem mapping
	return MAX96712_UPDATE_BITS(
		map, MAX96712_MIPI_TX_ALT_MEM(csi_id),
		MAX96712_MIPI_TX_ALT_MEM_FIELD, value);
}

static int max96712_set_video_pipe_enabled(struct max9x_common *common, unsigned pipe_id, bool enable)
{
	struct device *dev = common->dev;
	struct regmap *map = common->map;

	dev_dbg(dev, "Video-pipe %d: %s", pipe_id, (enable ? "enable" : "disable"));

	return MAX96712_UPDATE_BITS(map, MAX96712_VIDEO_PIPE_EN(pipe_id),
		MAX96712_VIDEO_PIPE_EN_FIELD(pipe_id),
		MAX9X_FIELD_PREP(MAX96712_VIDEO_PIPE_EN_FIELD(pipe_id), enable ? 1U : 0U));
}

static int max96712_set_serial_link_routing(struct max9x_common *common, unsigned link_id)
{
	unsigned pipe_id;
	unsigned map_id;
	int ret;

	for (pipe_id = 0; pipe_id < common->num_video_pipes; pipe_id++) {
		struct max9x_serdes_pipe_config *config;

		if (common->video_pipe[pipe_id].enabled == false)
			continue;

		config = &common->video_pipe[pipe_id].config;
		if (config->src_link != link_id)
			continue;

		ret = max96712_set_video_pipe_src(common, pipe_id, config->src_link, config->src_pipe);
		if (ret)
			return ret;

		ret = max96712_set_video_pipe_maps_enabled(common, pipe_id, config->num_maps);
		if (ret)
			return ret;

		for (map_id = 0; map_id < config->num_maps; map_id++) {
			ret = max96712_set_video_pipe_map(common, pipe_id, map_id, &config->map[map_id]);
			if (ret)
				return ret;

			if (!config->map[map_id].is_csi_enabled) {
				ret = max96712_set_csi_link_enabled(common,
								    config->map[map_id].dst_csi,
								    true);
				if (ret)
					return ret;
				config->map[map_id].is_csi_enabled = true;
			}

			ret = max96712_csi_double_pixel(common, config->map[map_id].dst_csi, config->dbl_pixel_bpp);
			if (ret)
				return ret;
		}

		ret = max96712_set_video_pipe_enabled(common, pipe_id, true);
		if (ret)
			return ret;
	}

	return 0;
}

static int max96712_disable_serial_link(struct max9x_common *common, unsigned link_id)
{
	unsigned pipe_id;
	unsigned map_id;
	int ret;

	for (pipe_id = 0; pipe_id < common->num_video_pipes; pipe_id++) {
		struct max9x_serdes_pipe_config *config;

		if (common->video_pipe[pipe_id].enabled == false)
			continue;

		config = &common->video_pipe[pipe_id].config;
		if (config->src_link != link_id)
			continue;

		ret = max96712_set_video_pipe_enabled(common, pipe_id, false);
		if (ret)
			return ret;

		ret = max96712_set_video_pipe_maps_enabled(common, pipe_id, 0);
		if (ret)
			return ret;

		for (map_id = 0; map_id < config->num_maps; map_id++) {
			if (!config->map[map_id].is_csi_enabled)
				continue;
			ret = max96712_set_csi_link_enabled(common, config->map[map_id].dst_csi, false);
			if (ret)
				return ret;
			config->map[map_id].is_csi_enabled = false;
		}
	}

	ret = max96712_set_serial_link_state(common, link_id, false);
	if (ret)
		return ret;

	return 0;
}

static int max96712_enable_serial_link(struct max9x_common *common, unsigned link_id)
{
	struct device *dev = common->dev;
	int ret;
	bool locked;
	unsigned long timeout;

	if (WARN_ON_ONCE(link_id >= common->num_serial_links))
		return -EINVAL;

	if (WARN_ONCE(common->serial_link[link_id].config.link_type != MAX9X_LINK_TYPE_GMSL2, "Only GMSL2 is supported!"))
		return -EINVAL;

	// GMSL2
	ret = max96712_set_remote_control_channel_enabled(common, link_id, false);
	if (ret)
		return ret;

	ret = max96712_set_serial_link_state(common, link_id, true);
	if (ret)
		return ret;

	ret = max96712_set_serial_link_rate(common, link_id);
	if (ret)
		return ret;

	ret = max96712_serial_link_reset(common, link_id);
	if (ret)
		return ret;

	ret = max96712_set_serial_link_routing(common, link_id);
	if (ret)
		return ret;

	timeout = jiffies + msecs_to_jiffies(max96712_serial_link_timeout_ms);
	do {
		usleep_range(10 * 1000, 25 * 1000); // 10 to 25 ms
		ret = max96712_get_serial_link_lock(common, link_id, &locked);
		if (ret)
			return ret;
	} while (!locked && time_before(jiffies, timeout));

	if (!locked) {
		dev_err(dev, "Serial-link %d: Failed to lock!", link_id);
		return -ETIMEDOUT;
	}

	return 0;
}

static int max96712_set_remote_control_channel_enabled(struct max9x_common *common, unsigned link_id, bool enabled)
{
	struct regmap *map = common->map;
	//TODO: Allow DT to choose which port gets enabled: bit 0 disables port 0, bit 1 disables port 1
	//See also 0x0E REG14 DIS_REM_CC_P2[3:0]

	// Note that the register value 1 *disables* port-to-remote command & control
	if (enabled) {
		return regmap_write(map, MAX96712_REM_CC,
			~(MAX9X_FIELD_PREP(MAX96712_REM_CC_DIS_PORT_FIELD(link_id, 0), enabled ? 1 : 0)));
	} else {
		return regmap_write(map, MAX96712_REM_CC, ~(0));
	}
}

static int max96712_select_serial_link(struct max9x_common *common, unsigned link)
{
	return max96712_set_remote_control_channel_enabled(common, link, true);
}

static int max96712_deselect_serial_link(struct max9x_common *common, unsigned link)
{
	return max96712_set_remote_control_channel_enabled(common, link, false);
}

static struct max9x_serial_link_ops max96712_serial_link_ops = {
	.enable = max96712_enable_serial_link,
	.disable = max96712_disable_serial_link,
	.select = max96712_select_serial_link,
	.deselect = max96712_deselect_serial_link,
};

static int max96712_enable_frame_sync(struct max9x_common *common) {
	struct device_node *node = common->dev->of_node;
	struct device *dev = common->dev;
	struct regmap *map = common->map;
	int ret;
	int num_frame_sync_gpios;
	int i;
	int gpio_num;
	unsigned int tx_reg_value;

	// Can have up to the number of serial links number of frame sync
	// gpios.
	u32 frame_sync_gpios[MAX96712_NUM_SERIAL_LINKS];
	ret = of_property_read_variable_u32_array(node, "frame-sync-ports",
					frame_sync_gpios, 0,
				       MAX96712_NUM_SERIAL_LINKS);
	// Not necessarily problematic, no frame sync signals could be read
	if(ret == -ENODATA || ret == -EINVAL) {
		dev_dbg(dev, "No frame sync gpios found");
		return 0;
	}
	// Other errors are problematic
	else if(ret < 0) {
		dev_err(dev, "Problem reading in frame sync gpios with error: %d",
			ret);
	}
	num_frame_sync_gpios = ret;

	// NOTE: Currently only GPIO2 is supported, the for loop is currently
	// here to support future infastructure. This is part of the
	// preliminary release to show off basic frame sync functionality.
	// Additional configuration is required in order for more then GPIO2
	// to be supported for frame sync.
	for(i = 0; i < num_frame_sync_gpios; i++) {
		gpio_num = frame_sync_gpios[i];
		dev_dbg(dev, "Enable GPIO%d for frame sync", gpio_num);

		// Enable GPIO for transmission
		// ex) register 0x300
		// RES_CFG 	-> 1
		// EMPTY
		// TX_COMP_EN 	-> 0
		// GPIO_OUT 	-> 0
		// GPIO_IN 	-> 0
		// GPIO_RX_EN 	-> 0
		// GPIO_TX_EN 	-> 1
		// GPIO_OUT_DIS -> 1
		TRY(ret, MAX96712_WRITE_REG(map, MAX96712_GET_GPIO_REG(gpio_num),
				      0x83));

		// Each GPIO has 3 registers which need to be set for
		// transmission. Those registers have no mathmatical function
		// that relates them so we need this switch statement to
		// determine which 3 registers to write to. Adding support
		// for more GPIOs means updating the switch statement below.
		tx_reg_value = 0x20 | gpio_num;
		switch(gpio_num) {
		case 0:
			TRY(ret, MAX96712_WRITE_REG(map, 0x337, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x36D, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x3A4, tx_reg_value));
			break;
		case 2:
			TRY(ret, MAX96712_WRITE_REG(map, 0x33D, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x374, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x3AA, tx_reg_value));
			break;
		case 4:
			TRY(ret, MAX96712_WRITE_REG(map, 0x334, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x37A, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x3B4, tx_reg_value));
			break;
		case 14:
			TRY(ret, MAX96712_WRITE_REG(map, 0x364, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x39A, tx_reg_value));
			TRY(ret, MAX96712_WRITE_REG(map, 0x3D1, tx_reg_value));
			break;
		default:
			dev_dbg(dev, "GPIO%d not supported", gpio_num);
			return -EINVAL;
		}
		// Enable GPIO2 for transmition
		TRY(ret, MAX96712_WRITE_REG(map, MAX96712_REG_GPIO2_A, 0x8B));
	}
	return 0;
}

static int max96712_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max9x_common *common = max9x_client_to_common(client);

	return max9x_common_resume(common);
}

static int max96712_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max9x_common *common = max9x_client_to_common(client);

	return max9x_common_suspend(common);
}

static int max96712_freeze(struct device *dev)
{
	return max96712_suspend(dev);
}

static int max96712_thaw(struct device *dev)
{
	return max96712_resume(dev);
}

static int max96712_restore(struct device *dev)
{
	return max96712_resume(dev);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
static int max96712_probe(struct i2c_client *client, const struct i2c_device_id *id)
#else
static int max96712_probe(struct i2c_client *client)
#endif
{
	struct device *dev = &client->dev;
	struct max9x_common *des = NULL;
	int ret = 0;

	dev_dbg(dev, "Probing");

	des = devm_kzalloc(dev, sizeof(*des), GFP_KERNEL);
	if (!des) {
		dev_err(dev, "Failed to allocate memory.");
		return -ENOMEM;
	}

	des->type = MAX9X_DESERIALIZER;

	ret = max9x_common_init_i2c_client(des,	client, &max96712_regmap_config,
					   &max96712_common_ops,
					   &max96712_serial_link_ops,
					   NULL, /* csi_link_os */
					   NULL /* lf_ops */);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int max96712_remove(struct i2c_client *client)
#else
static void max96712_remove(struct i2c_client *client)
#endif
{
	struct device *dev = &client->dev;
	struct max9x_common *des = NULL;

	dev_dbg(dev, "%s Removing", client->name);

	des = max9x_client_to_common(client);
	max9x_destroy(des);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static const struct dev_pm_ops max96712_pm_ops = {
	.suspend = max96712_suspend,
	.resume = max96712_resume,
	.freeze = max96712_freeze,
	.restore = max96712_restore,
	.thaw = max96712_thaw,
};

static struct i2c_device_id max96712_idtable[] = {
	{"max96712", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, max96712_idtable);

static struct of_device_id max96712_of_match[] = {
	{ .compatible = "max96712"},
	{},
};
MODULE_DEVICE_TABLE(of, max96712_of_match);

static struct i2c_driver max96712_driver = {
	.driver = {
		.name = "max96712",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max96712_of_match),
		.pm = &max96712_pm_ops,
	},
	.probe = max96712_probe,
	.remove = max96712_remove,
	.id_table = max96712_idtable,
};

module_i2c_driver(max96712_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Josh Watts <jwatts@d3embedded.com>");
MODULE_DESCRIPTION("Maxim MAX96712 Quad GMSL2/GMSL1 to CSI-2 Deserializer driver");
