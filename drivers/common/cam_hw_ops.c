/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "CAM-AHB %s:%d " fmt, __func__, __LINE__
#define TRUE   1
#include <linux/module.h>
#include <linux/interconnect.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include "cam_hw_ops.h"
#include "cam_soc_bus.h"

#ifdef CONFIG_CAM_AHB_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

//TODO: Remove this after solving issue with .h file
enum rpm_regulator_voltage_corner {
	RPM_REGULATOR_CORNER_NONE = 1,
	RPM_REGULATOR_CORNER_RETENTION,
	RPM_REGULATOR_CORNER_SVS_KRAIT,
	RPM_REGULATOR_CORNER_SVS_SOC,
	RPM_REGULATOR_CORNER_NORMAL,
	RPM_REGULATOR_CORNER_TURBO,
	RPM_REGULATOR_CORNER_SUPER_TURBO,
};

struct cam_ahb_client {
	enum cam_ahb_clk_vote vote;
};

struct cam_bus_vector {
	const char *name;
};

struct cam_ahb_client_data {
	struct cam_soc_bus_client_common_data common_data;
	void *soc_bus_client;
	u32 ahb_clk_state;
	struct cam_bus_vector *vectors;
	u32 *votes;
	u32 cnt;
	u32 probe_done;
	struct cam_ahb_client clients[CAM_AHB_CLIENT_MAX];
	struct mutex lock;
};

static struct cam_ahb_client_data data;

static int get_vector_index(char *name)
{
	int i = 0, rc = -1;

	for (i = 0; i < data.cnt; i++) {
		if (strcmp(name, data.vectors[i].name) == 0)
			return i;
	}

	return rc;
}

int cam_ahb_clk_init(struct platform_device *pdev)
{
	int i = 0, cnt = 0, rc = 0, index = 0;
	struct device_node *of_node;
	struct of_phandle_args src_args = {0}, dst_args = {0};

	if (!pdev) {
		pr_err("invalid pdev argument\n");
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;
	data.cnt = of_property_count_strings(of_node, "bus-vectors");
	if (data.cnt == 0) {
		pr_err("no vectors strings found in device tree, count=%d",
			data.cnt);
		return 0;
	}

	cnt = of_property_count_u32_elems(of_node, "qcom,bus-votes");
	if (cnt == 0) {
		pr_err("no vector values found in device tree, count=%d", cnt);
		return 0;
	}

	if (data.cnt != cnt) {
		pr_err("vector mismatch num of strings=%u, num of values %d\n",
			data.cnt, cnt);
		return -EINVAL;
	}

	CDBG("number of bus vectors: %d\n", data.cnt);

	data.vectors = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_bus_vector) * cnt,
		GFP_KERNEL);
	if (!data.vectors)
		return -ENOMEM;

	for (i = 0; i < data.cnt; i++) {
		rc = of_property_read_string_index(of_node, "bus-vectors",
				i, &(data.vectors[i].name));
		CDBG("dbg: names[%d] = %s\n", i, data.vectors[i].name);
		if (rc < 0) {
			pr_err("failed\n");
			rc = -EINVAL;
			goto err1;
		}
	}

	data.votes = devm_kzalloc(&pdev->dev, sizeof(u32) * cnt,
		GFP_KERNEL);
	if (!data.votes) {
		rc = -ENOMEM;
		goto err1;
	}

	rc = of_property_read_string(of_node, "interconnect-names",
			&data.common_data.name);
	if (rc) {
		pr_err("failed to read interconnect-names rc=%d",
				rc);
		rc = -EINVAL;
		goto err1;
	}

	rc = of_parse_phandle_with_args(of_node, "interconnects",
			"#interconnect-cells", 0, &src_args);
	if (rc) {
		pr_err("failed to read ahb bus src info rc=%d",
				rc);
		rc = -EINVAL;
		goto err1;
	}

	of_node_put(src_args.np);
	if (src_args.args_count != 1) {
		pr_err("Invalid number of ahb src args: %d",
				src_args.args_count);
		rc = -EINVAL;
		goto err1;
	}

	data.common_data.src_id = src_args.args[0];

	rc = of_parse_phandle_with_args(of_node, "interconnects",
			"#interconnect-cells", 1, &dst_args);
	if (rc) {
		pr_err("failed to read ahb bus dst info rc=%d",
			rc);
		rc = -EINVAL;
		goto err1;
	}

	of_node_put(dst_args.np);
	if (dst_args.args_count != 1) {
		pr_err("Invalid number of ahb dst args: %d",
			dst_args.args_count);
		rc = -EINVAL;
		goto err1;
	}

	data.common_data.dst_id = dst_args.args[0];
	data.common_data.num_usecases = data.cnt;

	if (data.common_data.num_usecases >
		CAM_SOC_BUS_MAX_NUM_USECASES) {
		pr_err("Invalid number of usecases: %d",
			data.common_data.num_usecases);
		rc = -EINVAL;
		goto err1;
	}

	rc = of_property_read_u32_array(of_node, "qcom,bus-votes",
		data.votes, cnt);

	for (i = 0; i < data.cnt; i++) {
		data.common_data.bw_pair[i].ab = 0;
		data.common_data.bw_pair[i].ib = data.votes[i];
		CDBG("dbg: votes[%d] = %u\n", i, data.votes[i]);
	}

	rc = cam_soc_bus_client_register(pdev,
		&data.soc_bus_client, &data.common_data);
	if (rc) {
		pr_err("ahb vote registering failed\n");
		rc = -EINVAL;
		goto err2;
	}

	index = get_vector_index("suspend");
	if (index < 0) {
		pr_err("svs vector not supported\n");
		rc = -EINVAL;
		goto err3;
	}

	/* request for svs in init */
	rc = cam_soc_bus_client_update_request(data.soc_bus_client,
			index);
	if (rc) {
		pr_err("ahb vote-update request failed\n");
		rc = -EINVAL;
		goto err3;
	}

	data.ahb_clk_state = CAM_AHB_SUSPEND_VOTE;
	data.probe_done = TRUE;
	mutex_init(&data.lock);

	CDBG("dbg, done registering ahb votes\n");
	CDBG("dbg, clk state :%u, probe :%d\n",
		data.ahb_clk_state, data.probe_done);
	return rc;

err3:
	cam_soc_bus_client_unregister(&data.soc_bus_client);
err2:
	devm_kfree(&pdev->dev, data.votes);
	data.votes = NULL;
err1:
	devm_kfree(&pdev->dev, data.vectors);
	data.vectors = NULL;
	return rc;
}
EXPORT_SYMBOL(cam_ahb_clk_init);

static int cam_consolidate_ahb_vote(enum cam_ahb_clk_client id,
	enum cam_ahb_clk_vote vote)
{
	int i = 0;
	u32 max = 0;

	CDBG("dbg: id :%u, vote : 0x%x\n", id, vote);
	mutex_lock(&data.lock);
	data.clients[id].vote = vote;

	if (vote == data.ahb_clk_state) {
		CDBG("dbg: already at desired vote\n");
		mutex_unlock(&data.lock);
		return 0;
	}

	for (i = 0; i < CAM_AHB_CLIENT_MAX; i++) {
		if (data.clients[i].vote > max)
			max = data.clients[i].vote;
	}

	CDBG("dbg: max vote : %u\n", max);
	if (max != data.ahb_clk_state) {
		cam_soc_bus_client_update_request(
				data.soc_bus_client,
				max);
		data.ahb_clk_state = max;
		CDBG("dbg: state : %u, vector : %d\n",
			data.ahb_clk_state, max);
	}
	mutex_unlock(&data.lock);
	return 0;
}

static int cam_ahb_get_voltage_level(unsigned int corner)
{
	switch (corner) {
	case RPM_REGULATOR_CORNER_NONE:
		return CAM_AHB_SUSPEND_VOTE;

	case RPM_REGULATOR_CORNER_SVS_KRAIT:
	case RPM_REGULATOR_CORNER_SVS_SOC:
		return CAM_AHB_SVS_VOTE;

	case RPM_REGULATOR_CORNER_NORMAL:
		return CAM_AHB_NOMINAL_VOTE;

	case RPM_REGULATOR_CORNER_SUPER_TURBO:
		return CAM_AHB_TURBO_VOTE;

	case RPM_REGULATOR_CORNER_TURBO:
	case RPM_REGULATOR_CORNER_RETENTION:
	default:
		return -EINVAL;
	}
}

int cam_config_ahb_clk(struct device *dev, unsigned long freq,
	enum cam_ahb_clk_client id, enum cam_ahb_clk_vote vote)
{
	struct dev_pm_opp *opp;
	unsigned int corner;
	enum cam_ahb_clk_vote dyn_vote = vote;
	int rc = -EINVAL;

	if (id >= CAM_AHB_CLIENT_MAX) {
		pr_err("err: invalid argument\n");
		return -EINVAL;
	}

	if (data.probe_done != TRUE) {
		pr_err("ahb init is not done yet\n");
		return -EINVAL;
	}

	CDBG("dbg: id :%u, vote : 0x%x\n", id, vote);
	switch (dyn_vote) {
	case CAM_AHB_SUSPEND_VOTE:
	case CAM_AHB_SVS_VOTE:
	case CAM_AHB_NOMINAL_VOTE:
	case CAM_AHB_TURBO_VOTE:
		break;
	case CAM_AHB_DYNAMIC_VOTE:
		if (!dev) {
			pr_err("device is NULL\n");
			return -EINVAL;
		}
		opp = dev_pm_opp_find_freq_exact(dev, freq, true);
		if (IS_ERR(opp)) {
			pr_err("Error on OPP freq :%ld\n", freq);
			return -EINVAL;
		}
		corner = dev_pm_opp_get_voltage(opp);
		if (corner == 0) {
			pr_err("Bad voltage corner for OPP freq :%ld\n", freq);
			return -EINVAL;
		}
		dyn_vote = cam_ahb_get_voltage_level(corner);
		if (dyn_vote < 0) {
			pr_err("Bad vote requested\n");
			return -EINVAL;
		}
		break;
	default:
		pr_err("err: invalid vote argument\n");
		return -EINVAL;
	}

	rc = cam_consolidate_ahb_vote(id, dyn_vote);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		goto end;
	}

end:
	return rc;
}
EXPORT_SYMBOL(cam_config_ahb_clk);
