// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022,2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/module.h>

#include "cam_cpas_hw.h"
#include "cam_cpas_hw_intf.h"
#include "cam_cpas_soc.h"
#include "cam_req_mgr_dev.h"
#include "cam_smmu_api.h"
#include "cam_compat.h"
#include "cam_cpastop_hw.h"

#define CAM_CPAS_LOG_BUF_LEN 512

static uint cam_min_camnoc_ib_bw;
module_param(cam_min_camnoc_ib_bw, uint, 0644);

static void cam_cpas_update_monitor_array(struct cam_hw_info *cpas_hw,
	const char *identifier_string, int32_t identifier_value);
static void cam_cpas_dump_monitor_array(
	struct cam_hw_info *cpas_hw);

static void cam_cpas_process_bw_overrides(
	struct cam_cpas_bus_client *bus_client, uint64_t *ab, uint64_t *ib,
	const struct cam_cpas_debug_settings *cpas_settings)
{
	uint64_t curr_ab = *ab;
	uint64_t curr_ib = *ib;
	size_t name_len = strlen(bus_client->common_data.name);

	if (!cpas_settings) {
		CAM_ERR(CAM_CPAS, "Invalid cpas debug settings");
		return;
	}

	if (strnstr(bus_client->common_data.name, "cam_hf_0", name_len)) {
		if (cpas_settings->mnoc_hf_0_ab_bw)
			*ab = cpas_settings->mnoc_hf_0_ab_bw;
		if (cpas_settings->mnoc_hf_0_ib_bw)
			*ib = cpas_settings->mnoc_hf_0_ib_bw;
	} else if (strnstr(bus_client->common_data.name, "cam_hf_1",
		name_len)) {
		if (cpas_settings->mnoc_hf_1_ab_bw)
			*ab = cpas_settings->mnoc_hf_1_ab_bw;
		if (cpas_settings->mnoc_hf_0_ib_bw)
			*ib = cpas_settings->mnoc_hf_1_ib_bw;
	} else if (strnstr(bus_client->common_data.name, "cam_sf_0",
		name_len)) {
		if (cpas_settings->mnoc_sf_0_ab_bw)
			*ab = cpas_settings->mnoc_sf_0_ab_bw;
		if (cpas_settings->mnoc_sf_0_ib_bw)
			*ib = cpas_settings->mnoc_sf_0_ib_bw;
	} else if (strnstr(bus_client->common_data.name, "cam_sf_1",
		name_len)) {
		if (cpas_settings->mnoc_sf_1_ab_bw)
			*ab = cpas_settings->mnoc_sf_1_ab_bw;
		if (cpas_settings->mnoc_sf_1_ib_bw)
			*ib = cpas_settings->mnoc_sf_1_ib_bw;
	} else if (strnstr(bus_client->common_data.name, "cam_sf_icp",
		name_len)) {
		if (cpas_settings->mnoc_sf_icp_ab_bw)
			*ab = cpas_settings->mnoc_sf_icp_ab_bw;
		if (cpas_settings->mnoc_sf_icp_ib_bw)
			*ib = cpas_settings->mnoc_sf_icp_ib_bw;
	} else {
		CAM_ERR(CAM_CPAS, "unknown mnoc port: %s, bw override failed",
			bus_client->common_data.name);
		return;
	}

	CAM_INFO(CAM_CPAS,
		"Overriding mnoc bw for: %s with ab: %llu, ib: %llu, curr_ab: %llu, curr_ib: %llu",
		bus_client->common_data.name, *ab, *ib, curr_ab, curr_ib);
}

int cam_cpas_util_reg_read(struct cam_hw_info *cpas_hw,
	enum cam_cpas_reg_base reg_base, struct cam_cpas_reg *reg_info)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	uint32_t value;
	int reg_base_index;

	if (!reg_info->enable)
		return 0;

	reg_base_index = cpas_core->regbase_index[reg_base];
	if (reg_base_index == -1)
		return -EINVAL;

	value = cam_io_r_mb(
		soc_info->reg_map[reg_base_index].mem_base + reg_info->offset);

	CAM_INFO(CAM_CPAS, "Base[%d] Offset[0x%08x] Value[0x%08x]",
		reg_base, reg_info->offset, value);

	return 0;
}

int cam_cpas_util_reg_update(struct cam_hw_info *cpas_hw,
	enum cam_cpas_reg_base reg_base, struct cam_cpas_reg *reg_info)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	uint32_t value;
	int reg_base_index;

	if (reg_info->enable == false)
		return 0;

	reg_base_index = cpas_core->regbase_index[reg_base];
	if (reg_base_index == -1)
		return -EINVAL;

	if (reg_info->masked_value) {
		value = cam_io_r_mb(
			soc_info->reg_map[reg_base_index].mem_base +
			reg_info->offset);
		value = value & (~reg_info->mask);
		value = value | (reg_info->value << reg_info->shift);
	} else {
		value = reg_info->value;
	}

	CAM_DBG(CAM_CPAS, "Base[%d]:[0x%08x] Offset[0x%08x] Value[0x%08x]",
		reg_base, soc_info->reg_map[reg_base_index].mem_base, reg_info->offset, value);

	cam_io_w_mb(value, soc_info->reg_map[reg_base_index].mem_base +
		reg_info->offset);

	return 0;
}

static int cam_cpas_util_vote_bus_client_level(
	struct cam_cpas_bus_client *bus_client, unsigned int level)
{
	int rc = 0;

	if (!bus_client->valid) {
		CAM_ERR(CAM_CPAS, "bus client not valid");
		rc = -EINVAL;
		goto end;
	}

	if (level >= CAM_MAX_VOTE) {
		CAM_ERR(CAM_CPAS,
			"Invalid votelevel=%d,usecases=%d,Bus client=[%s]",
			level, bus_client->common_data.num_usecases,
			bus_client->common_data.name);
		return -EINVAL;
	}

	if (level == bus_client->curr_vote_level)
		goto end;

	rc = cam_soc_bus_client_update_request(bus_client->soc_bus_client,
		level);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Client: %s update request failed rc: %d",
			bus_client->common_data.name, rc);
		goto end;
	}
	bus_client->curr_vote_level = level;

end:
	return rc;
}

static int cam_cpas_util_vote_bus_client_bw(
	struct cam_cpas_bus_client *bus_client, uint64_t ab, uint64_t ib,
	bool camnoc_bw, uint64_t *applied_ab, uint64_t *applied_ib)
{
	int rc = 0;
	uint64_t min_camnoc_ib_bw = CAM_CPAS_AXI_MIN_CAMNOC_IB_BW;
	const struct camera_debug_settings *cam_debug = NULL;

	if (!bus_client->valid) {
		CAM_ERR(CAM_CPAS, "bus client: %s not valid",
			bus_client->common_data.name);
		rc = -EINVAL;
		goto end;
	}

	if (cam_min_camnoc_ib_bw > 0)
		min_camnoc_ib_bw = (uint64_t)cam_min_camnoc_ib_bw * 1000000L;

	CAM_DBG(CAM_CPAS,
		"Bus_client: %s, is_virt_port: %d cam_min_camnoc_ib_bw = %d, min_camnoc_ib_bw=%llu",
		bus_client->common_data.name, bus_client->common_data.is_virt_port,
		cam_min_camnoc_ib_bw, min_camnoc_ib_bw);

	mutex_lock(&bus_client->lock);
	if (camnoc_bw) {
		if ((ab > 0) && (ab < CAM_CPAS_AXI_MIN_CAMNOC_AB_BW))
			ab = CAM_CPAS_AXI_MIN_CAMNOC_AB_BW;

		if ((ib > 0) && (ib < min_camnoc_ib_bw))
			ib = min_camnoc_ib_bw;
	} else {
		if ((ab > 0) && (ab < CAM_CPAS_AXI_MIN_MNOC_AB_BW))
			ab = CAM_CPAS_AXI_MIN_MNOC_AB_BW;

		if ((ib > 0) && (ib < CAM_CPAS_AXI_MIN_MNOC_IB_BW))
			ib = CAM_CPAS_AXI_MIN_MNOC_IB_BW;
	}

	cam_debug = cam_debug_get_settings();

	if (cam_debug && (cam_debug->cpas_settings.mnoc_hf_0_ab_bw ||
		cam_debug->cpas_settings.mnoc_hf_0_ib_bw ||
		cam_debug->cpas_settings.mnoc_hf_1_ab_bw ||
		cam_debug->cpas_settings.mnoc_hf_1_ib_bw ||
		cam_debug->cpas_settings.mnoc_sf_0_ab_bw ||
		cam_debug->cpas_settings.mnoc_sf_0_ib_bw ||
		cam_debug->cpas_settings.mnoc_sf_1_ab_bw ||
		cam_debug->cpas_settings.mnoc_sf_1_ib_bw ||
		cam_debug->cpas_settings.mnoc_sf_icp_ab_bw ||
		cam_debug->cpas_settings.mnoc_sf_icp_ib_bw))
		cam_cpas_process_bw_overrides(bus_client, &ab, &ib,
			&cam_debug->cpas_settings);

	rc = cam_soc_bus_client_update_bw(bus_client->soc_bus_client, ab, ib);
	if (rc) {
		CAM_ERR(CAM_CPAS,
			"Update bw failed, ab[%llu] ib[%llu]",
			ab, ib);
		goto unlock_client;
	}

	if (applied_ab)
		*applied_ab = ab;
	if (applied_ib)
		*applied_ib = ib;

unlock_client:
	mutex_unlock(&bus_client->lock);
end:
	return rc;
}

static int cam_cpas_util_register_bus_client(
	struct cam_hw_soc_info *soc_info, struct device_node *dev_node,
	struct cam_cpas_bus_client *bus_client)
{
	int rc = 0;

	rc = cam_soc_bus_client_register(soc_info->pdev, dev_node,
		&bus_client->soc_bus_client, &bus_client->common_data);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Bus client: %s registertion failed ,rc: %d",
			bus_client->common_data.name, rc);
		return rc;
	}
	bus_client->curr_vote_level = 0;
	bus_client->valid = true;
	mutex_init(&bus_client->lock);

	return 0;
}

static int cam_cpas_util_unregister_bus_client(
	struct cam_cpas_bus_client *bus_client)
{
	if (!bus_client->valid) {
		CAM_ERR(CAM_CPAS, "bus client not valid");
		return -EINVAL;
	}

	cam_soc_bus_client_unregister(&bus_client->soc_bus_client);
	bus_client->curr_vote_level = 0;
	bus_client->valid = false;
	mutex_destroy(&bus_client->lock);

	return 0;
}

static int cam_cpas_util_axi_cleanup(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info)
{
	int i = 0;

	if (cpas_core->num_axi_ports > CAM_CPAS_MAX_AXI_PORTS) {
		CAM_ERR(CAM_CPAS, "Invalid num_axi_ports: %d",
			cpas_core->num_axi_ports);
		return -EINVAL;
	}

	if (cpas_core->num_camnoc_axi_ports > CAM_CPAS_MAX_AXI_PORTS) {
		CAM_ERR(CAM_CPAS, "Invalid num_camnoc_axi_ports: %d",
			cpas_core->num_camnoc_axi_ports);
		return -EINVAL;
	}

	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		cam_cpas_util_unregister_bus_client(
			&cpas_core->axi_port[i].bus_client);
		of_node_put(cpas_core->axi_port[i].axi_port_node);
		cpas_core->axi_port[i].axi_port_node = NULL;
	}

	for (i = 0; i < cpas_core->num_camnoc_axi_ports; i++) {
		cam_cpas_util_unregister_bus_client(
			&cpas_core->camnoc_axi_port[i].bus_client);
		of_node_put(cpas_core->camnoc_axi_port[i].axi_port_node);
		cpas_core->camnoc_axi_port[i].axi_port_node = NULL;
	}

	return 0;
}

static int cam_cpas_util_axi_setup(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info)
{
	int i = 0, rc = 0;
	struct device_node *axi_port_mnoc_node = NULL;
	struct device_node *axi_port_camnoc_node = NULL;

	if (cpas_core->num_axi_ports > CAM_CPAS_MAX_AXI_PORTS) {
		CAM_ERR(CAM_CPAS, "Invalid num_axi_ports: %d",
			cpas_core->num_axi_ports);
		return -EINVAL;
	}

	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		axi_port_mnoc_node = cpas_core->axi_port[i].axi_port_node;
		rc = cam_cpas_util_register_bus_client(soc_info,
			axi_port_mnoc_node, &cpas_core->axi_port[i].bus_client);
		if (rc)
			goto bus_register_fail;
	}
	for (i = 0; i < cpas_core->num_camnoc_axi_ports; i++) {
		axi_port_camnoc_node =
			cpas_core->camnoc_axi_port[i].axi_port_node;
		rc = cam_cpas_util_register_bus_client(soc_info,
			axi_port_camnoc_node,
			&cpas_core->camnoc_axi_port[i].bus_client);
		if (rc)
			goto bus_register_fail;
	}

	return 0;
bus_register_fail:
	of_node_put(cpas_core->axi_port[i].axi_port_node);
	return rc;
}

static int cam_cpas_util_vote_default_ahb_axi(struct cam_hw_info *cpas_hw,
	int enable)
{
	int rc, i = 0;
	struct cam_cpas *cpas_core = (struct cam_cpas *)cpas_hw->core_info;
	uint64_t ab_bw, ib_bw;
	uint64_t applied_ab_bw = 0, applied_ib_bw = 0;

	rc = cam_cpas_util_vote_bus_client_level(&cpas_core->ahb_bus_client,
		(enable == true) ? CAM_SVS_VOTE : CAM_SUSPEND_VOTE);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Failed in AHB vote, enable=%d, rc=%d",
			enable, rc);
		return rc;
	}

	if (enable) {
		ab_bw = CAM_CPAS_DEFAULT_AXI_BW;
		ib_bw = CAM_CPAS_DEFAULT_AXI_BW;
	} else {
		ab_bw = 0;
		ib_bw = 0;
	}

	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		if (cpas_core->axi_port[i].bus_client.common_data.is_virt_port)
			continue;

		rc = cam_cpas_util_vote_bus_client_bw(
			&cpas_core->axi_port[i].bus_client,
			ab_bw, ib_bw, false, &applied_ab_bw, &applied_ib_bw);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed in mnoc vote, enable=%d, rc=%d",
				enable, rc);
			goto remove_ahb_vote;
		}
		cpas_core->axi_port[i].applied_ab_bw = applied_ab_bw;
		cpas_core->axi_port[i].applied_ib_bw = applied_ib_bw;
	}

	return 0;
remove_ahb_vote:
	cam_cpas_util_vote_bus_client_level(&cpas_core->ahb_bus_client,
		CAM_SUSPEND_VOTE);
	return rc;
}

static int cam_cpas_hw_reg_write(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, enum cam_cpas_reg_base reg_base,
	uint32_t offset, bool mb, uint32_t value)
{
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	int reg_base_index = cpas_core->regbase_index[reg_base];
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (reg_base_index < 0 || reg_base_index >= soc_info->num_reg_map) {
		CAM_ERR(CAM_CPAS,
			"Invalid reg_base=%d, reg_base_index=%d, num_map=%d",
			reg_base, reg_base_index, soc_info->num_reg_map);
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	if (mb)
		cam_io_w_mb(value,
			soc_info->reg_map[reg_base_index].mem_base + offset);
	else
		cam_io_w(value,
			soc_info->reg_map[reg_base_index].mem_base + offset);

unlock_client:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	return rc;
}

static int cam_cpas_hw_reg_read(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, enum cam_cpas_reg_base reg_base,
	uint32_t offset, bool mb, uint32_t *value)
{
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	int reg_base_index = cpas_core->regbase_index[reg_base];
	uint32_t reg_value;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!value)
		return -EINVAL;

	if (reg_base_index < 0 || reg_base_index >= soc_info->num_reg_map) {
		CAM_ERR(CAM_CPAS,
			"Invalid reg_base=%d, reg_base_index=%d, num_map=%d",
			reg_base, reg_base_index, soc_info->num_reg_map);
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		return -EPERM;
	}

	if (mb)
		reg_value = cam_io_r_mb(
			soc_info->reg_map[reg_base_index].mem_base + offset);
	else
		reg_value = cam_io_r(
			soc_info->reg_map[reg_base_index].mem_base + offset);

	*value = reg_value;

	return rc;
}

static int cam_cpas_hw_dump_camnoc_buff_fill_info(
	struct cam_hw_info *cpas_hw,
	uint32_t client_handle)
{
	int rc = 0, i;
	uint32_t val = 0;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_camnoc_info *camnoc_info =
		(struct cam_camnoc_info *) cpas_core->camnoc_info;
	char log_buf[CAM_CPAS_LOG_BUF_LEN] = {0};
	size_t len = 0;

	if (!camnoc_info) {
		CAM_ERR(CAM_CPAS, "Invalid camnoc info for hw_version: 0x%x",
			cpas_hw->soc_info.hw_version);
		return -EINVAL;
	}

	for (i = 0; i < camnoc_info->specific_size; i++) {
		if ((!camnoc_info->specific[i].enable) ||
			(!camnoc_info->specific[i].maxwr_low.enable))
			continue;

		rc = cam_cpas_hw_reg_read(cpas_hw, client_handle,
			CAM_CPAS_REG_CAMNOC,
			camnoc_info->specific[i].maxwr_low.offset, true, &val);
		if (rc)
			break;

		len += scnprintf((log_buf + len), (CAM_CPAS_LOG_BUF_LEN - len),
			" %s:[%d %d]", camnoc_info->specific[i].port_name,
			(val & 0x7FF), (val & 0x7F0000) >> 16);
	}

	CAM_INFO(CAM_CPAS, "CAMNOC Fill level [Queued Pending] %s", log_buf);

	return rc;

}

static void cam_cpas_print_smart_qos_priority(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_cpas_tree_node *niu_node;
	uint8_t i;
	int32_t reg_indx = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	char log_buf[CAM_CPAS_LOG_BUF_LEN] = {0};
	size_t len = 0;
	uint32_t val = 0;

	for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++) {
		niu_node = soc_private->smart_qos_info->rt_wr_niu_node[i];

		val = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base +
			niu_node->pri_lut_low_offset);

		len += scnprintf((log_buf + len), (CAM_CPAS_LOG_BUF_LEN - len),
			" [%s:0x%x]", niu_node->node_name, val);
	}

	CAM_INFO(CAM_CPAS, "SmartQoS [Node Pri_lut_low] %s", log_buf);
}

static bool cam_cpas_is_new_rt_bw_lower(
	const struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	int i;
	struct cam_cpas_axi_port *temp_axi_port = NULL;
	uint64_t applied_total = 0, new_total = 0;

	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		temp_axi_port = &cpas_core->axi_port[i];

		if (temp_axi_port->is_rt) {
			CAM_DBG(CAM_PERF, "Port %s applied %llu new %llu",
				temp_axi_port->axi_port_name, temp_axi_port->applied_ab_bw,
				temp_axi_port->ab_bw);

			applied_total += temp_axi_port->applied_ab_bw;
			new_total += temp_axi_port->ab_bw;
		}
	}

	return (new_total < applied_total) ? true : false;
}

static void cam_cpas_reset_niu_priorities(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	uint8_t i;

	for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++)
		soc_private->smart_qos_info->rt_wr_niu_node[i]->applied_priority = 0x0;
}

static bool cam_cpas_calculate_smart_qos(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_cpas_tree_node *niu_node;
	uint8_t i;
	bool needs_update = false;
	uint64_t bw_per_kb, max_bw_per_kb = 0, remainder, ramp_val;
	int8_t pos;
	uint32_t priority;
	uint8_t val;

	for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++) {
		niu_node = soc_private->smart_qos_info->rt_wr_niu_node[i];

		bw_per_kb = niu_node->camnoc_bw;
		remainder = do_div(bw_per_kb, niu_node->niu_size);

		if (max_bw_per_kb < bw_per_kb)
			max_bw_per_kb = bw_per_kb;

		CAM_DBG(CAM_PERF,
			"NIU[%d][%s] : camnoc_bw %llu, niu_size %u, bw_per_kb %lld, remainder %lld max_bw_per_kb %lld",
			i, niu_node->node_name, niu_node->camnoc_bw, niu_node->niu_size,
			bw_per_kb, remainder, max_bw_per_kb);
	}

	if (!max_bw_per_kb) {
		CAM_DBG(CAM_PERF, "No valid bw on NIU nodes");
		return false;
	}

	for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++) {
		niu_node = soc_private->smart_qos_info->rt_wr_niu_node[i];

		bw_per_kb = niu_node->camnoc_bw;
		remainder = do_div(bw_per_kb, niu_node->niu_size); // --> dropping remainder

		priority = 0;

		for (pos = 7; pos >= 0; pos--) {
			if (pos == 0) {
				val = soc_private->smart_qos_info->rt_wr_priority_min;
			} else if (pos == 7) {
				val = soc_private->smart_qos_info->rt_wr_priority_max;
			} else {
				ramp_val = pos * bw_per_kb;
				remainder = do_div(ramp_val, max_bw_per_kb);

				CAM_DBG(CAM_PERF,
					"pos=%d, bw_per_kb=%lld, pos*bw_per_kb=%lld, ramp_val=%lld, remainder=%lld, max_bw_per_kb=%lld",
					pos, bw_per_kb, pos * bw_per_kb, ramp_val, remainder,
					max_bw_per_kb);

				// round the value
				if ((remainder * 2) >= max_bw_per_kb)
					ramp_val += 1;

				val = (uint8_t)(ramp_val);
				val += soc_private->smart_qos_info->rt_wr_priority_min;
				val = min(val, soc_private->smart_qos_info->rt_wr_priority_max);
			}

			priority = priority << 4;
			priority |= val;

			CAM_DBG(CAM_PERF, "pos=%d, val=0x%x, priority=0x%x", pos, val, priority);
		}

		niu_node->curr_priority = priority;

		if (niu_node->curr_priority != niu_node->applied_priority)
			needs_update = true;

		CAM_DBG(CAM_PERF,
			"Node[%d][%s] : camnoc_bw=%lld, niu_size=%d, bw_per_kb %lld, Priority applied 0x%x new 0x%x needs_update %d",
			i, niu_node->node_name, niu_node->camnoc_bw, niu_node->niu_size, bw_per_kb,
			niu_node->applied_priority, niu_node->curr_priority,
			needs_update);
	}

	if (cpas_core->smart_qos_dump && needs_update) {
		for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++) {
			niu_node = soc_private->smart_qos_info->rt_wr_niu_node[i];
			CAM_INFO(CAM_PERF,
				"Node[%d][%s] : camnoc_bw=%lld, niu_size=%d, offset 0x%x, Priority new 0x%x applied 0x%x",
				i, niu_node->node_name, niu_node->camnoc_bw, niu_node->niu_size,
				niu_node->pri_lut_low_offset,
				niu_node->curr_priority, niu_node->applied_priority);
		}
	}

	return needs_update;
}

static int cam_cpas_apply_smart_qos(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_cpas_tree_node *niu_node;
	uint8_t i;
	int32_t reg_indx = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	int32_t cam_qos_cnt = 0, ret = 0;
	struct qcom_scm_camera_qos scm_buf[QCOM_SCM_CAMERA_MAX_QOS_CNT] = {0};

	if (cpas_core->smart_qos_dump) {
		CAM_INFO(CAM_PERF, "Printing SmartQos values before update");
		cam_cpas_print_smart_qos_priority(cpas_hw);
	}

	for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++) {
		niu_node = soc_private->smart_qos_info->rt_wr_niu_node[i];

		if (niu_node->curr_priority != niu_node->applied_priority) {
			if (!soc_private->enable_secure_qos_update)
			{
				cam_io_w_mb(niu_node->curr_priority,
					soc_info->reg_map[reg_indx].mem_base +
					niu_node->pri_lut_low_offset);
				niu_node->applied_priority = niu_node->curr_priority;
			}
			else
			{
				scm_buf[cam_qos_cnt].offset = niu_node->pri_lut_low_offset;
				scm_buf[cam_qos_cnt].val = niu_node->curr_priority;
				cam_qos_cnt++;
			}
		}

		if (soc_private->enable_secure_qos_update && cam_qos_cnt) {
			CAM_DBG(CAM_PERF, "Updating secure camera smartOos count: %d", cam_qos_cnt);
			ret = cam_update_camnoc_qos_settings(CAM_QOS_UPDATE_TYPE_SMART,
				cam_qos_cnt, scm_buf);
			if (ret) {
				CAM_ERR(CAM_PERF, "Secure camera smartOos update failed:%d", ret);
				return ret;
			}
			CAM_DBG(CAM_PERF, "Updated secure camera smartOos");
			cam_qos_cnt = 0;
		}
	}

	if (cpas_core->smart_qos_dump) {
		CAM_INFO(CAM_PERF, "Printing SmartQos values after update");
		cam_cpas_print_smart_qos_priority(cpas_hw);
	}

	return 0;
}

static int cam_cpas_util_set_camnoc_axi_clk_rate(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_tree_node *tree_node = NULL;
	int rc = 0, i = 0;
	const struct camera_debug_settings *cam_debug = NULL;


	CAM_DBG(CAM_CPAS, "control_camnoc_axi_clk=%d",
		soc_private->control_camnoc_axi_clk);

	if (soc_private->control_camnoc_axi_clk) {
		struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
		uint64_t required_camnoc_bw = 0, intermediate_result = 0;
		int64_t clk_rate = 0;

		for (i = 0; i < CAM_CPAS_MAX_TREE_NODES; i++) {
			tree_node = soc_private->tree_node[i];
			if (!tree_node ||
				!tree_node->camnoc_max_needed)
				continue;

			if (required_camnoc_bw < (tree_node->camnoc_bw *
				tree_node->bus_width_factor)) {
				required_camnoc_bw = tree_node->camnoc_bw *
					tree_node->bus_width_factor;
			}
		}

		intermediate_result = required_camnoc_bw *
			soc_private->camnoc_axi_clk_bw_margin;
		do_div(intermediate_result, 100);
		required_camnoc_bw += intermediate_result;

		if (cpas_core->streamon_clients && (required_camnoc_bw == 0)) {
			CAM_DBG(CAM_CPAS,
				"Set min vote if streamon_clients is non-zero : streamon_clients=%d",
				cpas_core->streamon_clients);
			required_camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
		}

		if ((required_camnoc_bw > 0) &&
			(required_camnoc_bw <
			soc_private->camnoc_axi_min_ib_bw))
			required_camnoc_bw = soc_private->camnoc_axi_min_ib_bw;

		cam_debug = cam_debug_get_settings();
		if (cam_debug && cam_debug->cpas_settings.camnoc_bw) {
			if (cam_debug->cpas_settings.camnoc_bw <
				soc_private->camnoc_bus_width)
				required_camnoc_bw =
					soc_private->camnoc_bus_width;
			else
				required_camnoc_bw =
					cam_debug->cpas_settings.camnoc_bw;
			CAM_INFO(CAM_CPAS, "Overriding camnoc bw: %llu",
				required_camnoc_bw);
		}

		intermediate_result = required_camnoc_bw;
		do_div(intermediate_result, soc_private->camnoc_bus_width);
		clk_rate = intermediate_result;

		CAM_DBG(CAM_CPAS,
			"Setting camnoc axi clk rate[BW Clk] : [%llu %lld]",
			required_camnoc_bw, clk_rate);

		/*
		 * CPAS hw is not powered on for the first client.
		 * Also, clk_rate will be overwritten with default
		 * value while power on. So, skipping this for first
		 * client.
		 */
		if (cpas_core->streamon_clients) {
			rc = cam_soc_util_set_src_clk_rate(soc_info, clk_rate);
			if (rc)
				CAM_ERR(CAM_CPAS,
				"Failed in setting camnoc axi clk %llu %lld %d",
				required_camnoc_bw, clk_rate, rc);

			cpas_core->applied_camnoc_axi_rate = clk_rate;
		}
	}

	return rc;
}

static int cam_cpas_util_translate_client_paths(
	struct cam_axi_vote *axi_vote)
{
	int i;
	uint32_t *path_data_type = NULL;

	if (!axi_vote)
		return -EINVAL;

	for (i = 0; i < axi_vote->num_paths; i++) {
		path_data_type = &axi_vote->axi_path[i].path_data_type;
		/* Update path_data_type from UAPI value to internal value */
		if (*path_data_type >= CAM_CPAS_PATH_DATA_CONSO_OFFSET)
			*path_data_type = CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT +
				(*path_data_type %
				CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT);
		else
			*path_data_type %= CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT;

		if (*path_data_type >= CAM_CPAS_PATH_DATA_MAX) {
			CAM_ERR(CAM_CPAS, "index Invalid: %d", path_data_type);
			return -EINVAL;
		}
	}

	return 0;
}

static int cam_cpas_axi_consolidate_path_votes(
	struct cam_cpas_client *cpas_client,
	struct cam_axi_vote *axi_vote)
{
	int rc = 0, i, k, l;
	struct cam_axi_vote *con_axi_vote = &cpas_client->axi_vote;
	bool path_found = false, cons_entry_found;
	struct cam_cpas_tree_node *curr_tree_node = NULL;
	struct cam_cpas_tree_node *sum_tree_node = NULL;
	uint32_t transac_type;
	uint32_t path_data_type;
	struct cam_axi_per_path_bw_vote *axi_path;

	con_axi_vote->num_paths = 0;

	for (i = 0; i < axi_vote->num_paths; i++) {
		path_found = false;
		path_data_type = axi_vote->axi_path[i].path_data_type;
		transac_type = axi_vote->axi_path[i].transac_type;

		if ((path_data_type >= CAM_CPAS_PATH_DATA_MAX) ||
			(transac_type >= CAM_CPAS_TRANSACTION_MAX)) {
			CAM_ERR(CAM_CPAS, "Invalid path or transac type: %d %d",
				path_data_type, transac_type);
			return -EINVAL;
		}

		axi_path = &con_axi_vote->axi_path[con_axi_vote->num_paths];

		curr_tree_node =
			cpas_client->tree_node[path_data_type][transac_type];
		if (curr_tree_node) {
			path_found = true;
			memcpy(axi_path, &axi_vote->axi_path[i],
				sizeof(struct cam_axi_per_path_bw_vote));
			con_axi_vote->num_paths++;
			continue;
		}

		for (k = 0; k < CAM_CPAS_PATH_DATA_MAX; k++) {
			sum_tree_node = cpas_client->tree_node[k][transac_type];

			if (!sum_tree_node)
				continue;

			if (sum_tree_node->constituent_paths[path_data_type]) {
				path_found = true;
				/*
				 * Check if corresponding consolidated path
				 * entry is already added into consolidated list
				 */
				cons_entry_found = false;
				for (l = 0; l < con_axi_vote->num_paths; l++) {
					if ((con_axi_vote->axi_path[l]
					.path_data_type == k) &&
					(con_axi_vote->axi_path[l]
					.transac_type == transac_type)) {
						cons_entry_found = true;
						con_axi_vote->axi_path[l]
						.camnoc_bw +=
						axi_vote->axi_path[i]
						.camnoc_bw;

						con_axi_vote->axi_path[l]
						.mnoc_ab_bw +=
						axi_vote->axi_path[i]
						.mnoc_ab_bw;

						con_axi_vote->axi_path[l]
						.mnoc_ib_bw +=
						axi_vote->axi_path[i]
						.mnoc_ib_bw;
						break;
					}
				}

				/* If not found, add a new entry */
				if (!cons_entry_found) {
					axi_path->path_data_type = k;
					axi_path->transac_type = transac_type;
					axi_path->camnoc_bw =
					axi_vote->axi_path[i].camnoc_bw;
					axi_path->mnoc_ab_bw =
					axi_vote->axi_path[i].mnoc_ab_bw;
					axi_path->mnoc_ib_bw =
					axi_vote->axi_path[i].mnoc_ib_bw;
					con_axi_vote->num_paths++;
				}
				break;
			}
		}

		if (!path_found) {
			CAM_ERR(CAM_CPAS,
				"Client [%s][%d] i=%d num_paths=%d Consolidated path not found for path=%d, transac=%d",
				cpas_client->data.identifier, cpas_client->data.cell_index, i,
				axi_vote->num_paths, path_data_type, transac_type);
			return -EINVAL;
		}
	}

	return rc;
}

static int cam_cpas_update_axi_vote_bw(
	struct cam_hw_info *cpas_hw,
	struct cam_cpas_tree_node *cpas_tree_node,
	bool   *mnoc_axi_port_updated,
	bool   *camnoc_axi_port_updated)
{
	int i, axi_port_idx = -1;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;

	/* Below code is executed to fill BW votes for MMNOC AXI port and DDR RT port
	 * based on if DDR RT BW voting is enabled and DDR RT AXI port defined.
	 */
	for (i = 0; i < CAM_CPAS_MAX_VIRT_PORTS; i++) {
		if ((i > CAM_CPAS_PORT_AXI_PORT) && !soc_private->rt_bw_voting_needed)
			continue;

		axi_port_idx = cpas_tree_node->axi_port_idx_arr[i];
		if (axi_port_idx < 0)
			continue;

		if (axi_port_idx >= CAM_CPAS_MAX_AXI_PORTS) {
			CAM_ERR(CAM_CPAS, "Invalid axi_port_idx: %d drv_idx: %d", axi_port_idx, i);
			return -EINVAL;
		}

		cpas_core->axi_port[axi_port_idx].ab_bw =
			cpas_tree_node->mnoc_ab_bw;
		cpas_core->axi_port[axi_port_idx].ib_bw =
			cpas_tree_node->mnoc_ib_bw;

		/* For MMNOC AXI port, use ib value same as ab if RB BW voting is enabled */
		if (soc_private->rt_bw_voting_needed)
			cpas_core->axi_port[axi_port_idx].ib_bw =
				cpas_tree_node->mnoc_ab_bw;

		/* For DDR RT AXI port, always send ab value only */
		if (i > CAM_CPAS_PORT_AXI_PORT) {
			cpas_core->axi_port[axi_port_idx].ab_bw =
				cpas_tree_node->mnoc_ib_bw;
			cpas_core->axi_port[axi_port_idx].ib_bw = 0;
		}

		mnoc_axi_port_updated[axi_port_idx] = true;
	}

	if (soc_private->control_camnoc_axi_clk)
		return 0;

	cpas_core->camnoc_axi_port[cpas_tree_node->axi_port_idx_arr[CAM_CPAS_PORT_AXI_PORT]]
		.camnoc_bw = cpas_tree_node->camnoc_bw;
	camnoc_axi_port_updated[cpas_tree_node->camnoc_axi_port_idx] = true;
	return 0;
}

static int cam_cpas_camnoc_set_vote_axi_clk_rate(
	struct cam_hw_info *cpas_hw,
	bool   *camnoc_axi_port_updated)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	int i;
	int rc = 0;
	struct cam_cpas_axi_port *camnoc_axi_port = NULL;
	uint64_t camnoc_bw;
	uint64_t applied_ab = 0, applied_ib = 0;

	if (soc_private->control_camnoc_axi_clk) {
		rc = cam_cpas_util_set_camnoc_axi_clk_rate(cpas_hw);
		if (rc)
			CAM_ERR(CAM_CPAS,
				"Failed in setting axi clk rate rc=%d", rc);
		return rc;
	}

	/* Below code is executed if we just vote and do not set the clk rate
	 * for camnoc
	 */

	if (cpas_core->num_camnoc_axi_ports > CAM_CPAS_MAX_AXI_PORTS) {
		CAM_ERR(CAM_CPAS, "Invalid num_camnoc_axi_ports: %d",
			cpas_core->num_camnoc_axi_ports);
		return -EINVAL;
	}

	for (i = 0; i < cpas_core->num_camnoc_axi_ports; i++) {
		if (camnoc_axi_port_updated[i])
			camnoc_axi_port = &cpas_core->camnoc_axi_port[i];
		else
			continue;

		CAM_DBG(CAM_PERF, "Port[%s] : camnoc_bw=%lld",
			camnoc_axi_port->axi_port_name,
			camnoc_axi_port->camnoc_bw);

		if (camnoc_axi_port->camnoc_bw)
			camnoc_bw = camnoc_axi_port->camnoc_bw;
		else if (camnoc_axi_port->additional_bw)
			camnoc_bw = camnoc_axi_port->additional_bw;
		else if (cpas_core->streamon_clients)
			camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
		else
			camnoc_bw = 0;

		rc = cam_cpas_util_vote_bus_client_bw(
			&camnoc_axi_port->bus_client,
			0, camnoc_bw, true, &applied_ab, &applied_ib);

		CAM_DBG(CAM_CPAS,
			"camnoc vote camnoc_bw[%llu] rc=%d %s",
			camnoc_bw, rc, camnoc_axi_port->axi_port_name);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed in camnoc vote camnoc_bw[%llu] rc=%d",
				camnoc_bw, rc);
			break;
		}
		camnoc_axi_port->applied_ab_bw = applied_ab;
		camnoc_axi_port->applied_ib_bw = applied_ib;
	}
	return rc;
}

static int cam_cpas_util_apply_client_axi_vote(
	struct cam_hw_info *cpas_hw,
	struct cam_cpas_client *cpas_client,
	struct cam_axi_vote *axi_vote)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_axi_vote *con_axi_vote = NULL;
	struct cam_cpas_axi_port *mnoc_axi_port = NULL;
	struct cam_cpas_tree_node *curr_tree_node = NULL;
	struct cam_cpas_tree_node *par_tree_node = NULL;
	uint32_t transac_type;
	uint32_t path_data_type;
	bool mnoc_axi_port_updated[CAM_CPAS_MAX_AXI_PORTS] = {false};
	bool camnoc_axi_port_updated[CAM_CPAS_MAX_AXI_PORTS] = {false};
	uint64_t mnoc_ab_bw = 0, mnoc_ib_bw = 0,
		curr_camnoc_old = 0, curr_mnoc_ab_old = 0, curr_mnoc_ib_old = 0,
		par_camnoc_old = 0, par_mnoc_ab_old = 0, par_mnoc_ib_old = 0;
	int rc = 0, i = 0;
	uint64_t applied_ab = 0, applied_ib = 0;
	bool apply_smart_qos = false;
	bool rt_bw_updated = false;

	mutex_lock(&cpas_core->tree_lock);
	if (!cpas_client->tree_node_valid) {
		/*
		 * This is by assuming apply_client_axi_vote is called
		 * for these clients from only cpas_start, cpas_stop.
		 * not called from hw_update_axi_vote
		 */
		for (i = 0; i < cpas_core->num_axi_ports; i++) {
			if (cpas_core->axi_port[i].bus_client.common_data.is_virt_port)
				continue;

			if (axi_vote->axi_path[0].mnoc_ab_bw) {
				/* start case */
				cpas_core->axi_port[i].additional_bw +=
					CAM_CPAS_DEFAULT_AXI_BW;
			} else {
				/* stop case */
				cpas_core->axi_port[i].additional_bw -=
					CAM_CPAS_DEFAULT_AXI_BW;
			}
			mnoc_axi_port_updated[i] = true;
		}

		for (i = 0; i < cpas_core->num_camnoc_axi_ports; i++) {
			if (axi_vote->axi_path[0].camnoc_bw) {
				/* start case */
				cpas_core->camnoc_axi_port[i].additional_bw +=
					CAM_CPAS_DEFAULT_AXI_BW;
			} else {
				/* stop case */
				cpas_core->camnoc_axi_port[i].additional_bw -=
					CAM_CPAS_DEFAULT_AXI_BW;
			}
			camnoc_axi_port_updated[i] = true;
		}

		goto vote_start_clients;
	}

	rc = cam_cpas_axi_consolidate_path_votes(cpas_client, axi_vote);
	if (rc) {
		CAM_ERR(CAM_PERF, "Failed in bw consolidation, Client [%s][%d]",
			cpas_client->data.identifier,
			cpas_client->data.cell_index);
		goto unlock_tree;
	}

	con_axi_vote = &cpas_client->axi_vote;

	cam_cpas_dump_axi_vote_info(cpas_client, "Consolidated Vote",
		con_axi_vote);

	/* Traverse through node tree and update bw vote values */
	for (i = 0; i < con_axi_vote->num_paths; i++) {
		path_data_type =
		con_axi_vote->axi_path[i].path_data_type;
		transac_type =
		con_axi_vote->axi_path[i].transac_type;
		curr_tree_node = cpas_client->tree_node[path_data_type]
			[transac_type];

		if (con_axi_vote->axi_path[i].mnoc_ab_bw == 0)
			con_axi_vote->axi_path[i].mnoc_ab_bw =
				con_axi_vote->axi_path[i].camnoc_bw;

		if (con_axi_vote->axi_path[i].camnoc_bw == 0)
			con_axi_vote->axi_path[i].camnoc_bw =
				con_axi_vote->axi_path[i].mnoc_ab_bw;

		if ((curr_tree_node->camnoc_bw ==
			con_axi_vote->axi_path[i].camnoc_bw) &&
			(curr_tree_node->mnoc_ab_bw ==
			con_axi_vote->axi_path[i].mnoc_ab_bw) &&
			(curr_tree_node->mnoc_ib_bw ==
			con_axi_vote->axi_path[i].mnoc_ib_bw))
			continue;

		curr_camnoc_old = curr_tree_node->camnoc_bw;
		curr_mnoc_ab_old = curr_tree_node->mnoc_ab_bw;
		curr_mnoc_ib_old = curr_tree_node->mnoc_ib_bw;
		curr_tree_node->camnoc_bw =
			con_axi_vote->axi_path[i].camnoc_bw;
		curr_tree_node->mnoc_ab_bw =
			con_axi_vote->axi_path[i].mnoc_ab_bw;
		curr_tree_node->mnoc_ib_bw =
			con_axi_vote->axi_path[i].mnoc_ib_bw;

		while (curr_tree_node->parent_node) {
			par_tree_node = curr_tree_node->parent_node;
			par_camnoc_old = par_tree_node->camnoc_bw;
			par_mnoc_ab_old = par_tree_node->mnoc_ab_bw;
			par_mnoc_ib_old = par_tree_node->mnoc_ib_bw;
			par_tree_node->mnoc_ab_bw -= curr_mnoc_ab_old;
			par_tree_node->mnoc_ab_bw += curr_tree_node->mnoc_ab_bw;
			par_tree_node->mnoc_ib_bw -= curr_mnoc_ib_old;
			par_tree_node->mnoc_ib_bw += curr_tree_node->mnoc_ib_bw;

			if (par_tree_node->merge_type ==
				CAM_CPAS_TRAFFIC_MERGE_SUM) {
				par_tree_node->camnoc_bw -=
					curr_camnoc_old;
				par_tree_node->camnoc_bw +=
					curr_tree_node->camnoc_bw;
			} else if (par_tree_node->merge_type ==
				CAM_CPAS_TRAFFIC_MERGE_SUM_INTERLEAVE) {
				par_tree_node->camnoc_bw -=
					(curr_camnoc_old / 2);
				par_tree_node->camnoc_bw +=
					(curr_tree_node->camnoc_bw / 2);
			} else {
				CAM_ERR(CAM_CPAS, "Invalid Merge type");
				rc = -EINVAL;
				goto unlock_tree;
			}

			if (!par_tree_node->parent_node) {
				rc = cam_cpas_update_axi_vote_bw(cpas_hw,
					par_tree_node,
					mnoc_axi_port_updated,
					camnoc_axi_port_updated);
				if (rc) {
					CAM_ERR(CAM_CPAS,
						"Update Vote failed");
					goto unlock_tree;
				}
			}

			curr_tree_node = par_tree_node;
			curr_camnoc_old = par_camnoc_old;
			curr_mnoc_ab_old = par_mnoc_ab_old;
			curr_mnoc_ib_old = par_mnoc_ib_old;
		}
	}

	if (!par_tree_node) {
		CAM_DBG(CAM_CPAS, "No change in BW for all paths");
		rc = 0;
		goto unlock_tree;
	}

	if (soc_private->enable_smart_qos) {
		CAM_DBG(CAM_PERF, "Start QoS update for client[%s][%d]",
			cpas_client->data.identifier, cpas_client->data.cell_index);
		for (i = 0; i < cpas_core->num_axi_ports; i++) {
			if (mnoc_axi_port_updated[i] && cpas_core->axi_port[i].is_rt) {
				rt_bw_updated = true;
				break;
			}
		}

		if (rt_bw_updated) {
			apply_smart_qos = cam_cpas_calculate_smart_qos(cpas_hw);

			if (apply_smart_qos && cam_cpas_is_new_rt_bw_lower(cpas_hw)) {
				/*
				 * If new BW is low, apply QoS first and then vote,
				 * otherwise vote first and then apply QoS
				 */
				CAM_DBG(CAM_PERF, "Apply Smart QoS first");
				rc = cam_cpas_apply_smart_qos(cpas_hw);
				if (rc) {
					CAM_ERR(CAM_CPAS,
						"Failed in Smart QoS rc=%d", rc);
					goto unlock_tree;
				}

				apply_smart_qos = false;
			}
		}
	}

vote_start_clients:
	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		if (mnoc_axi_port_updated[i])
			mnoc_axi_port = &cpas_core->axi_port[i];
		else
			continue;

		CAM_DBG(CAM_PERF,
			"Port[%s] : ab=%lld ib=%lld additional=%lld, streamon_clients=%d",
			mnoc_axi_port->axi_port_name, mnoc_axi_port->ab_bw,
			mnoc_axi_port->ib_bw, mnoc_axi_port->additional_bw,
			cpas_core->streamon_clients);

		if (mnoc_axi_port->ab_bw)
			mnoc_ab_bw = mnoc_axi_port->ab_bw;
		else if (mnoc_axi_port->additional_bw)
			mnoc_ab_bw = mnoc_axi_port->additional_bw;
		else if (cpas_core->streamon_clients &&
				!mnoc_axi_port->bus_client.common_data.is_virt_port)
			mnoc_ab_bw = CAM_CPAS_DEFAULT_AXI_BW;
		else
			mnoc_ab_bw = 0;

		if (cpas_core->axi_port[i].ib_bw_voting_needed)
			mnoc_ib_bw = mnoc_axi_port->ib_bw;
		else
			mnoc_ib_bw = 0;

		rc = cam_cpas_util_vote_bus_client_bw(
			&mnoc_axi_port->bus_client,
			mnoc_ab_bw, mnoc_ib_bw, false, &applied_ab,
			&applied_ib);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed in mnoc vote ab[%llu] ib[%llu] rc=%d",
				mnoc_ab_bw, mnoc_ib_bw, rc);
			goto unlock_tree;
		}
		mnoc_axi_port->applied_ab_bw = applied_ab;
		mnoc_axi_port->applied_ib_bw = applied_ib;
	}

	rc = cam_cpas_camnoc_set_vote_axi_clk_rate(
		cpas_hw, camnoc_axi_port_updated);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Failed in setting axi clk rate rc=%d", rc);
		goto unlock_tree;
	}

	if (soc_private->enable_smart_qos && apply_smart_qos) {
		CAM_DBG(CAM_PERF, "Apply Smart QoS after bw votes");

		rc = cam_cpas_apply_smart_qos(cpas_hw);
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in Smart QoS rc=%d", rc);
			goto unlock_tree;
		}
	}

unlock_tree:
	mutex_unlock(&cpas_core->tree_lock);
	return rc;
}

static int cam_cpas_util_apply_default_axi_vote(
	struct cam_hw_info *cpas_hw, bool enable)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_axi_port *axi_port = NULL;
	uint64_t mnoc_ab_bw = 0, mnoc_ib_bw = 0;
	int rc = 0, i = 0;

	mutex_lock(&cpas_core->tree_lock);
	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		if ((!cpas_core->axi_port[i].bus_client.common_data.is_virt_port) &&
			(!cpas_core->axi_port[i].ab_bw ||
			!cpas_core->axi_port[i].ib_bw))
			axi_port = &cpas_core->axi_port[i];
		else
			continue;

		if (enable)
			mnoc_ib_bw = CAM_CPAS_DEFAULT_AXI_BW;
		else
			mnoc_ib_bw = 0;

		CAM_DBG(CAM_CPAS, "Port=[%s] :ab[%llu] ib[%llu]",
			axi_port->axi_port_name, mnoc_ab_bw, mnoc_ib_bw);

		rc = cam_cpas_util_vote_bus_client_bw(&axi_port->bus_client,
			mnoc_ab_bw, mnoc_ib_bw, false, &axi_port->applied_ab_bw,
			&axi_port->applied_ib_bw);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed in mnoc vote ab[%llu] ib[%llu] rc=%d",
				mnoc_ab_bw, mnoc_ib_bw, rc);
			goto unlock_tree;
		}
	}

unlock_tree:
	mutex_unlock(&cpas_core->tree_lock);
	return rc;
}

static int cam_cpas_hw_update_axi_vote(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, struct cam_axi_vote *client_axi_vote)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	struct cam_axi_vote *axi_vote = NULL;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!client_axi_vote) {
		CAM_ERR(CAM_CPAS, "Invalid arg, client_handle=%d",
			client_handle);
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);

	axi_vote = kmemdup(client_axi_vote, sizeof(struct cam_axi_vote),
		GFP_KERNEL);
	if (!axi_vote) {
		CAM_ERR(CAM_CPAS, "Out of memory");
		mutex_unlock(&cpas_core->client_mutex[client_indx]);
		mutex_unlock(&cpas_hw->hw_mutex);
		return -ENOMEM;
	}

	cam_cpas_dump_axi_vote_info(cpas_core->cpas_client[client_indx],
		"Incoming Vote", axi_vote);

	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	rc = cam_cpas_util_translate_client_paths(axi_vote);
	if (rc) {
		CAM_ERR(CAM_CPAS,
			"Unable to translate per path votes rc: %d", rc);
		goto unlock_client;
	}

	cam_cpas_dump_axi_vote_info(cpas_core->cpas_client[client_indx],
		"Translated Vote", axi_vote);

	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_core->cpas_client[client_indx], axi_vote);

	/* Log an entry whenever there is an AXI update - after updating */
	cam_cpas_update_monitor_array(cpas_hw, "CPAS AXI post-update",
		client_indx);
unlock_client:
	cam_free_clear((void *)axi_vote);
	axi_vote = NULL;
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_util_get_ahb_level(struct cam_hw_info *cpas_hw,
	struct device *dev, unsigned long freq, enum cam_vote_level *req_level)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct dev_pm_opp *opp;
	unsigned int corner;
	enum cam_vote_level level = CAM_SVS_VOTE;
	unsigned long corner_freq = freq;
	int i;

	if (!dev || !req_level) {
		CAM_ERR(CAM_CPAS, "Invalid params %pK, %pK", dev, req_level);
		return -EINVAL;
	}

	opp = dev_pm_opp_find_freq_ceil(dev, &corner_freq);
	if (IS_ERR(opp)) {
		CAM_DBG(CAM_CPAS, "OPP Ceil not available for freq :%ld, %pK",
			corner_freq, opp);
		*req_level = CAM_TURBO_VOTE;
		return 0;
	}

	corner = dev_pm_opp_get_voltage(opp);

	for (i = 0; i < soc_private->num_vdd_ahb_mapping; i++)
		if (corner == soc_private->vdd_ahb[i].vdd_corner)
			level = soc_private->vdd_ahb[i].ahb_level;

	CAM_DBG(CAM_CPAS,
		"From OPP table : freq=[%ld][%ld], corner=%d, level=%d",
		freq, corner_freq, corner, level);

	*req_level = level;

	return 0;
}

static int cam_cpas_util_apply_client_ahb_vote(struct cam_hw_info *cpas_hw,
	struct cam_cpas_client *cpas_client, struct cam_ahb_vote *ahb_vote,
	enum cam_vote_level *applied_level)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_bus_client *ahb_bus_client = &cpas_core->ahb_bus_client;
	enum cam_vote_level required_level;
	enum cam_vote_level highest_level;
	int i, rc = 0;

	if (!ahb_bus_client->valid) {
		CAM_ERR(CAM_CPAS, "AHB Bus client not valid");
		return -EINVAL;
	}

	if (ahb_vote->type == CAM_VOTE_DYNAMIC) {
		rc = cam_cpas_util_get_ahb_level(cpas_hw, cpas_client->data.dev,
			ahb_vote->vote.freq, &required_level);
		if (rc)
			return rc;
	} else {
		required_level = ahb_vote->vote.level;
	}

	if (cpas_client->ahb_level == required_level)
		return 0;

	mutex_lock(&ahb_bus_client->lock);
	cpas_client->ahb_level = required_level;

	CAM_DBG(CAM_CPAS, "Client[%s] required level[%d], curr_level[%d]",
		ahb_bus_client->common_data.name, required_level,
		ahb_bus_client->curr_vote_level);

	if (required_level == ahb_bus_client->curr_vote_level)
		goto unlock_bus_client;

	highest_level = required_level;
	for (i = 0; i < cpas_core->num_clients; i++) {
		if (cpas_core->cpas_client[i] && (highest_level <
			cpas_core->cpas_client[i]->ahb_level))
			highest_level = cpas_core->cpas_client[i]->ahb_level;
	}

	CAM_DBG(CAM_CPAS, "Required highest_level[%d]", highest_level);

	if (!cpas_core->ahb_bus_scaling_disable) {
		rc = cam_cpas_util_vote_bus_client_level(ahb_bus_client,
			highest_level);
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in ahb vote, level=%d, rc=%d",
				highest_level, rc);
			goto unlock_bus_client;
		}
	}

	if (cpas_core->streamon_clients) {
		rc = cam_soc_util_set_clk_rate_level(&cpas_hw->soc_info,
			highest_level, true);
		if (rc) {
			CAM_ERR(CAM_CPAS,
				"Failed in scaling clock rate level %d for AHB",
				highest_level);
			goto unlock_bus_client;
		}
	}

	if (applied_level)
		*applied_level = highest_level;

unlock_bus_client:
	mutex_unlock(&ahb_bus_client->lock);
	return rc;
}

static int cam_cpas_hw_update_ahb_vote(struct cam_hw_info *cpas_hw,
	uint32_t client_handle, struct cam_ahb_vote *client_ahb_vote)
{
	struct cam_ahb_vote ahb_vote;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_client *cpas_client = NULL;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!client_ahb_vote) {
		CAM_ERR(CAM_CPAS, "Invalid input arg");
		return -EINVAL;
	}

	ahb_vote = *client_ahb_vote;

	if (ahb_vote.vote.level == 0) {
		CAM_DBG(CAM_CPAS, "0 ahb vote from client %d",
			client_handle);
		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = CAM_SVS_VOTE;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] has not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto unlock_client;
	}

	CAM_DBG(CAM_PERF,
		"client=[%d][%s][%d] : type[%d], level[%d], freq[%ld], applied[%d]",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, ahb_vote.type,
		ahb_vote.vote.level, ahb_vote.vote.freq,
		cpas_core->cpas_client[client_indx]->ahb_level);

	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw,
		cpas_core->cpas_client[client_indx], &ahb_vote, NULL);

unlock_client:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_util_create_vote_all_paths(
	struct cam_cpas_client *cpas_client,
	struct cam_axi_vote *axi_vote)
{
	int i, j;
	uint64_t camnoc_bw, mnoc_ab_bw, mnoc_ib_bw;
	struct cam_axi_per_path_bw_vote *axi_path;

	if (!cpas_client || !axi_vote)
		return -EINVAL;

	camnoc_bw = axi_vote->axi_path[0].camnoc_bw;
	mnoc_ab_bw = axi_vote->axi_path[0].mnoc_ab_bw;
	mnoc_ib_bw = axi_vote->axi_path[0].mnoc_ib_bw;

	axi_vote->num_paths = 0;

	for (i = 0; i < CAM_CPAS_TRANSACTION_MAX; i++) {
		for (j = 0; j < CAM_CPAS_PATH_DATA_MAX; j++) {
			if (cpas_client->tree_node[j][i]) {
				axi_path =
				&axi_vote->axi_path[axi_vote->num_paths];

				axi_path->path_data_type = j;
				axi_path->transac_type = i;
				axi_path->camnoc_bw = camnoc_bw;
				axi_path->mnoc_ab_bw = mnoc_ab_bw;
				axi_path->mnoc_ib_bw = mnoc_ib_bw;

				axi_vote->num_paths++;
			}
		}
	}

	return 0;
}

static int cam_cpas_hw_start(void *hw_priv, void *start_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	uint32_t client_indx;
	struct cam_cpas_hw_cmd_start *cmd_hw_start;
	struct cam_cpas_client *cpas_client;
	struct cam_ahb_vote *ahb_vote;
	struct cam_ahb_vote remove_ahb;
	struct cam_axi_vote axi_vote = {0};
	enum cam_vote_level applied_level = CAM_SVS_VOTE;
	int rc, i = 0, count = 0;
	struct cam_cpas_private_soc *soc_private = NULL;
	bool invalid_start = true;

	if (!hw_priv || !start_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, start_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_cmd_start) != arg_size) {
		CAM_ERR(CAM_CPAS, "HW_CAPS size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_cmd_start), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	soc_private = (struct cam_cpas_private_soc *)
		cpas_hw->soc_info.soc_private;
	cmd_hw_start = (struct cam_cpas_hw_cmd_start *)start_args;
	client_indx = CAM_CPAS_GET_CLIENT_IDX(cmd_hw_start->client_handle);
	ahb_vote = cmd_hw_start->ahb_vote;

	if (!ahb_vote || !cmd_hw_start->axi_vote)
		return -EINVAL;

	if (!ahb_vote->vote.level) {
		CAM_ERR(CAM_CPAS, "Invalid vote ahb[%d]",
			ahb_vote->vote.level);
		return -EINVAL;
	}

	memcpy(&axi_vote, cmd_hw_start->axi_vote, sizeof(struct cam_axi_vote));
	for (i = 0; i < axi_vote.num_paths; i++) {
		if ((axi_vote.axi_path[i].camnoc_bw != 0) ||
			(axi_vote.axi_path[i].mnoc_ab_bw != 0) ||
			(axi_vote.axi_path[i].mnoc_ib_bw != 0)) {
			invalid_start = false;
			break;
		}
	}

	if (invalid_start) {
		CAM_ERR(CAM_CPAS, "Zero start vote");
		return -EINVAL;
	}

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	if (!CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d] is not registered",
			client_indx);
		rc = -EPERM;
		goto error;
	}

	if (CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "client=[%d][%s][%d] is in start state",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto error;
	}

	CAM_DBG(CAM_CPAS,
		"AHB :client=[%d][%s][%d] type[%d], level[%d], applied[%d]",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index,
		ahb_vote->type, ahb_vote->vote.level, cpas_client->ahb_level);
	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw, cpas_client,
		ahb_vote, &applied_level);
	if (rc)
		goto error;

	cam_cpas_dump_axi_vote_info(cpas_client, "CPAS Start Vote",
		&axi_vote);

	/*
	 * If client has indicated start bw to be applied on all paths
	 * of client, apply that otherwise apply whatever the client supplies
	 * for specific paths
	 */
	if (axi_vote.axi_path[0].path_data_type ==
		CAM_CPAS_API_PATH_DATA_STD_START) {
		rc = cam_cpas_util_create_vote_all_paths(cpas_client,
			&axi_vote);
	} else {
		rc = cam_cpas_util_translate_client_paths(&axi_vote);
	}

	if (rc) {
		CAM_ERR(CAM_CPAS, "Unable to create or translate paths rc: %d",
			rc);
		goto remove_ahb_vote;
	}

	cam_cpas_dump_axi_vote_info(cpas_client, "CPAS Start Translated Vote",
		&axi_vote);

	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_client, &axi_vote);
	if (rc)
		goto remove_ahb_vote;

	if (cpas_core->streamon_clients == 0) {
		rc = cam_cpas_util_apply_default_axi_vote(cpas_hw, true);
		if (rc)
			goto remove_ahb_vote;

		atomic_set(&cpas_core->irq_count, 1);

		count = cam_soc_util_regulators_enabled(&cpas_hw->soc_info);
		if (count > 0)
			CAM_DBG(CAM_CPAS, "Regulators already enabled %d", count);
		rc = cam_cpas_soc_enable_resources(&cpas_hw->soc_info,
			applied_level);
		if (rc) {
			atomic_set(&cpas_core->irq_count, 0);
			CAM_ERR(CAM_CPAS, "enable_resorce failed, rc=%d", rc);
			goto remove_axi_vote;
		}

		if (cpas_core->internal_ops.qchannel_handshake) {
			rc = cpas_core->internal_ops.qchannel_handshake(cpas_hw, true, false);
			if (rc) {
				CAM_WARN(CAM_CPAS, "failed in qchannel_handshake rc=%d", rc);
				/* Do not return error, passthrough */

				rc = cpas_core->internal_ops.qchannel_handshake(cpas_hw,
					true, true);
				if (rc) {
					CAM_ERR(CAM_CPAS,
						"failed in qchannel_handshake, hw blocks may not work rc=%d",
						rc);
					/* Do not return error, passthrough */
				}
				rc = 0;
			}
		}

		if (cpas_core->internal_ops.power_on) {
			rc = cpas_core->internal_ops.power_on(cpas_hw);
			if (rc) {
				atomic_set(&cpas_core->irq_count, 0);
				cam_cpas_soc_disable_resources(
					&cpas_hw->soc_info, true, true);
				CAM_ERR(CAM_CPAS,
					"failed in power_on settings rc=%d",
					rc);
				goto remove_axi_vote;
			}
		}
		CAM_DBG(CAM_CPAS, "irq_count=%d\n",
			atomic_read(&cpas_core->irq_count));

		if (soc_private->enable_smart_qos)
			cam_cpas_reset_niu_priorities(cpas_hw);

		cam_smmu_reset_cb_page_fault_cnt();
		cpas_hw->hw_state = CAM_HW_STATE_POWER_UP;
	}

	cpas_client->started = true;
	cpas_core->streamon_clients++;

	CAM_DBG(CAM_CPAS, "client=[%d][%s][%d] streamon_clients=%d",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, cpas_core->streamon_clients);

	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;

remove_axi_vote:
	memset(&axi_vote, 0x0, sizeof(struct cam_axi_vote));
	rc = cam_cpas_util_create_vote_all_paths(cpas_client, &axi_vote);
	if (rc)
		CAM_ERR(CAM_CPAS, "Unable to create per path votes rc: %d", rc);

	cam_cpas_dump_axi_vote_info(cpas_client, "CPAS Start fail Vote",
		&axi_vote);

	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_client, &axi_vote);
	if (rc)
		CAM_ERR(CAM_CPAS, "Unable remove votes rc: %d", rc);

remove_ahb_vote:
	remove_ahb.type = CAM_VOTE_ABSOLUTE;
	remove_ahb.vote.level = CAM_SUSPEND_VOTE;
	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw, cpas_client,
		&remove_ahb, NULL);
	if (rc)
		CAM_ERR(CAM_CPAS, "Removing AHB vote failed, rc=%d", rc);

error:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int _check_irq_count(struct cam_cpas *cpas_core)
{
	return (atomic_read(&cpas_core->irq_count) > 0) ? 0 : 1;
}

static int cam_cpas_hw_stop(void *hw_priv, void *stop_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	uint32_t client_indx;
	struct cam_cpas_hw_cmd_stop *cmd_hw_stop;
	struct cam_cpas_client *cpas_client;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};
	struct cam_cpas_private_soc *soc_private = NULL;
	int rc = 0, count;
	long result;
	int retry_camnoc_idle = 0;

	if (!hw_priv || !stop_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, stop_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_cmd_stop) != arg_size) {
		CAM_ERR(CAM_CPAS, "HW_CAPS size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_cmd_stop), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	soc_private = (struct cam_cpas_private_soc *)
		cpas_hw->soc_info.soc_private;
	cmd_hw_stop = (struct cam_cpas_hw_cmd_stop *)stop_args;
	client_indx = CAM_CPAS_GET_CLIENT_IDX(cmd_hw_stop->client_handle);

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);
	cpas_client = cpas_core->cpas_client[client_indx];

	CAM_DBG(CAM_CPAS, "Client=[%d][%s][%d] streamon_clients=%d",
		client_indx, cpas_client->data.identifier,
		cpas_client->data.cell_index, cpas_core->streamon_clients);

	if (!CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "Client=[%d][%s][%d] is not started",
			client_indx, cpas_client->data.identifier,
			cpas_client->data.cell_index);
		rc = -EPERM;
		goto done;
	}

	cpas_client->started = false;
	cpas_core->streamon_clients--;

	if (cpas_core->streamon_clients == 0) {
		if (cpas_core->internal_ops.power_off) {
			rc = cpas_core->internal_ops.power_off(cpas_hw);
			if (rc) {
				CAM_ERR(CAM_CPAS,
					"failed in power_off settings rc=%d",
					rc);
				/* Do not return error, passthrough */
			}
		}

		if (cpas_core->internal_ops.qchannel_handshake) {
			rc = cpas_core->internal_ops.qchannel_handshake(cpas_hw, false, false);
			if (rc) {
				CAM_ERR(CAM_CPAS, "failed in qchannel_handshake rc=%d", rc);
				retry_camnoc_idle = 1;
				/* Do not return error, passthrough */
			}
		}

		rc = cam_cpas_soc_disable_irq(&cpas_hw->soc_info);
		if (rc) {
			CAM_ERR(CAM_CPAS, "disable_irq failed, rc=%d", rc);
			goto done;
		}

		/* Wait for any IRQs still being handled */
		atomic_dec(&cpas_core->irq_count);
		result = wait_event_timeout(cpas_core->irq_count_wq,
			_check_irq_count(cpas_core), HZ);
		if (result == 0) {
			CAM_ERR(CAM_CPAS, "Wait failed: irq_count=%d",
				atomic_read(&cpas_core->irq_count));
		}

		/* try again incase camnoc is still not idle */
		if (cpas_core->internal_ops.qchannel_handshake &&
			retry_camnoc_idle) {
			rc = cpas_core->internal_ops.qchannel_handshake(cpas_hw, false, false);
			if (rc) {
				CAM_ERR(CAM_CPAS, "failed in qchannel_handshake rc=%d", rc);
				/* Do not return error, passthrough */
			}
		}

		rc = cam_cpas_soc_disable_resources(&cpas_hw->soc_info,
			true, false);
		if (rc) {
			CAM_ERR(CAM_CPAS, "disable_resorce failed, rc=%d", rc);
			goto done;
		}
		CAM_DBG(CAM_CPAS, "Disabled all the resources: irq_count=%d",
			atomic_read(&cpas_core->irq_count));

		count = cam_soc_util_regulators_enabled(&cpas_hw->soc_info);
		if (count > 0)
			CAM_WARN(CAM_CPAS,
				"Client=[%d][%s][%d] qchannel shut down while top gdsc is still on %d",
				client_indx, cpas_client->data.identifier,
				cpas_client->data.cell_index, count);

		cpas_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SUSPEND_VOTE;
	rc = cam_cpas_util_apply_client_ahb_vote(cpas_hw, cpas_client,
		&ahb_vote, NULL);
	if (rc)
		goto done;

	rc = cam_cpas_util_create_vote_all_paths(cpas_client, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Unable to create per path votes rc: %d", rc);
		goto done;
	}

	cam_cpas_dump_axi_vote_info(cpas_client, "CPAS Stop Vote", &axi_vote);

	rc = cam_cpas_util_apply_client_axi_vote(cpas_hw,
		cpas_client, &axi_vote);
	if (rc)
		goto done;

	if (cpas_core->streamon_clients == 0)
		rc = cam_cpas_util_apply_default_axi_vote(cpas_hw, false);
done:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_hw_init(void *hw_priv, void *init_hw_args,
	uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	int rc = 0;

	if (!hw_priv || !init_hw_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, init_hw_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_caps) != arg_size) {
		CAM_ERR(CAM_CPAS, "INIT HW size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_caps), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *)cpas_hw->core_info;

	if (cpas_core->internal_ops.init_hw_version) {
		rc = cpas_core->internal_ops.init_hw_version(cpas_hw,
			(struct cam_cpas_hw_caps *)init_hw_args);
	}

	return rc;
}

static int cam_cpas_hw_register_client(struct cam_hw_info *cpas_hw,
	struct cam_cpas_register_params *register_params)
{
	int rc;
	char client_name[CAM_HW_IDENTIFIER_LENGTH + 3];
	int32_t client_indx = -1;
	struct cam_cpas *cpas_core = (struct cam_cpas *)cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;

	if ((!register_params) ||
		(strlen(register_params->identifier) < 1)) {
		CAM_ERR(CAM_CPAS, "Invalid cpas client identifier");
		return -EINVAL;
	}

	CAM_DBG(CAM_CPAS, "Register params : identifier=%s, cell_index=%d",
		register_params->identifier, register_params->cell_index);

	if (soc_private->client_id_based)
		snprintf(client_name, sizeof(client_name), "%s%d",
			register_params->identifier,
			register_params->cell_index);
	else
		snprintf(client_name, sizeof(client_name), "%s",
			register_params->identifier);

	mutex_lock(&cpas_hw->hw_mutex);

	rc = cam_common_util_get_string_index(soc_private->client_name,
		soc_private->num_clients, client_name, &client_indx);

	mutex_lock(&cpas_core->client_mutex[client_indx]);

	if (rc || !CAM_CPAS_CLIENT_VALID(client_indx) ||
		CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS,
			"Inval client %s %d : %d %d %pK %d",
			register_params->identifier,
			register_params->cell_index,
			CAM_CPAS_CLIENT_VALID(client_indx),
			CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx),
			cpas_core->cpas_client[client_indx], rc);
		mutex_unlock(&cpas_core->client_mutex[client_indx]);
		mutex_unlock(&cpas_hw->hw_mutex);
		return -EPERM;
	}

	register_params->client_handle =
		CAM_CPAS_GET_CLIENT_HANDLE(client_indx);
	memcpy(&cpas_core->cpas_client[client_indx]->data, register_params,
		sizeof(struct cam_cpas_register_params));
	cpas_core->registered_clients++;
	cpas_core->cpas_client[client_indx]->registered = true;

	CAM_DBG(CAM_CPAS, "client=[%d][%s][%d], registered_clients=%d",
		client_indx,
		cpas_core->cpas_client[client_indx]->data.identifier,
		cpas_core->cpas_client[client_indx]->data.cell_index,
		cpas_core->registered_clients);

	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);

	return 0;
}

static int cam_cpas_hw_unregister_client(struct cam_hw_info *cpas_hw,
	uint32_t client_handle)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	uint32_t client_indx = CAM_CPAS_GET_CLIENT_IDX(client_handle);
	int rc = 0;

	if (!CAM_CPAS_CLIENT_VALID(client_indx))
		return -EINVAL;

	mutex_lock(&cpas_hw->hw_mutex);
	mutex_lock(&cpas_core->client_mutex[client_indx]);

	if (!CAM_CPAS_CLIENT_REGISTERED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "Client=[%d][%s][%d] not registered",
			client_indx,
			cpas_core->cpas_client[client_indx]->data.identifier,
			cpas_core->cpas_client[client_indx]->data.cell_index);
		rc = -EPERM;
		goto done;
	}

	if (CAM_CPAS_CLIENT_STARTED(cpas_core, client_indx)) {
		CAM_ERR(CAM_CPAS, "Client=[%d][%s][%d] is not stopped",
			client_indx,
			cpas_core->cpas_client[client_indx]->data.identifier,
			cpas_core->cpas_client[client_indx]->data.cell_index);

		rc = -EPERM;
		goto done;
	}

	CAM_DBG(CAM_CPAS, "client=[%d][%s][%d], registered_clients=%d",
		client_indx,
		cpas_core->cpas_client[client_indx]->data.identifier,
		cpas_core->cpas_client[client_indx]->data.cell_index,
		cpas_core->registered_clients);

	cpas_core->cpas_client[client_indx]->registered = false;
	cpas_core->registered_clients--;
done:
	mutex_unlock(&cpas_core->client_mutex[client_indx]);
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_hw_get_hw_info(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	struct cam_cpas_hw_caps *hw_caps;
	struct cam_cpas_private_soc *soc_private;

	if (!hw_priv || !get_hw_cap_args) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK",
			hw_priv, get_hw_cap_args);
		return -EINVAL;
	}

	if (sizeof(struct cam_cpas_hw_caps) != arg_size) {
		CAM_ERR(CAM_CPAS, "HW_CAPS size mismatch %zd %d",
			sizeof(struct cam_cpas_hw_caps), arg_size);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)hw_priv;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	hw_caps = (struct cam_cpas_hw_caps *)get_hw_cap_args;
	*hw_caps = cpas_core->hw_caps;

	/*Extract Fuse Info*/
	soc_private = (struct cam_cpas_private_soc *)
		cpas_hw->soc_info.soc_private;

	hw_caps->fuse_info = soc_private->fuse_info;
	CAM_INFO(CAM_CPAS, "fuse info->num_fuses %d",
		hw_caps->fuse_info.num_fuses);

	hw_caps->rt_bw_voting_needed = soc_private->rt_bw_voting_needed;
	return 0;
}

static int cam_cpas_log_vote(struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	uint32_t i;
	struct cam_cpas_tree_node *curr_node;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;

	if ((cpas_core->streamon_clients > 0) && soc_private->enable_smart_qos)
		cam_cpas_print_smart_qos_priority(cpas_hw);

	/*
	 * First print rpmh registers as early as possible to catch nearest
	 * state of rpmh after an issue (overflow) occurs.
	 */
	if ((cpas_core->streamon_clients > 0) &&
		(cpas_core->regbase_index[CAM_CPAS_REG_RPMH] != -1)) {
		int reg_base_index =
			cpas_core->regbase_index[CAM_CPAS_REG_RPMH];
		void __iomem *rpmh_base =
			soc_info->reg_map[reg_base_index].mem_base;
		uint32_t offset_fe, offset_be;
		uint32_t fe_val, be_val;
		uint32_t *rpmh_info = &soc_private->rpmh_info[0];
		uint32_t ddr_bcm_index =
			soc_private->rpmh_info[CAM_RPMH_BCM_DDR_INDEX];
		uint32_t mnoc_bcm_index =
			soc_private->rpmh_info[CAM_RPMH_BCM_MNOC_INDEX];

		/*
		 * print 12 registers from 0x4, 0x800 offsets -
		 * this will give ddr, mmnoc and other BCM info.
		 * i=0 for DDR, i=4 for mnoc, but double check for each chipset.
		 */
		for (i = 0; i < rpmh_info[CAM_RPMH_NUMBER_OF_BCMS]; i++) {
			if ((!cpas_core->full_state_dump) &&
				(i != ddr_bcm_index) &&
				(i != mnoc_bcm_index))
				continue;

			offset_fe = rpmh_info[CAM_RPMH_BCM_FE_OFFSET] +
				(i * 0x4);
			offset_be = rpmh_info[CAM_RPMH_BCM_BE_OFFSET] +
				(i * 0x4);

			fe_val = cam_io_r_mb(rpmh_base + offset_fe);
			be_val = cam_io_r_mb(rpmh_base + offset_be);

			CAM_INFO(CAM_CPAS,
				"i=%d, FE[offset=0x%x, value=0x%x] BE[offset=0x%x, value=0x%x]",
				i, offset_fe, fe_val, offset_be, be_val);
		}
	}

	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		CAM_INFO(CAM_CPAS,
			"[%s] ab_bw[%lld] ib_bw[%lld] additional_bw[%lld] applied_ab[%lld] applied_ib[%lld]",
			cpas_core->axi_port[i].axi_port_name,
			cpas_core->axi_port[i].ab_bw,
			cpas_core->axi_port[i].ib_bw,
			cpas_core->axi_port[i].additional_bw,
			cpas_core->axi_port[i].applied_ab_bw,
			cpas_core->axi_port[i].applied_ib_bw);
	}

	if (soc_private->control_camnoc_axi_clk) {
		CAM_INFO(CAM_CPAS, "applied camnoc axi clk[%lld]",
			cpas_core->applied_camnoc_axi_rate);
	} else {
		for (i = 0; i < cpas_core->num_camnoc_axi_ports; i++) {
			CAM_INFO(CAM_CPAS,
				"[%s] ab_bw[%lld] ib_bw[%lld] additional_bw[%lld] applied_ab[%lld] applied_ib[%lld]",
				cpas_core->camnoc_axi_port[i].axi_port_name,
				cpas_core->camnoc_axi_port[i].ab_bw,
				cpas_core->camnoc_axi_port[i].ib_bw,
				cpas_core->camnoc_axi_port[i].additional_bw,
				cpas_core->camnoc_axi_port[i].applied_ab_bw,
				cpas_core->camnoc_axi_port[i].applied_ib_bw);
		}
	}

	CAM_INFO(CAM_CPAS, "ahb client curr vote level[%d]",
		cpas_core->ahb_bus_client.curr_vote_level);

	if (!cpas_core->full_state_dump) {
		CAM_DBG(CAM_CPAS, "CPAS full state dump not enabled");
		return 0;
	}

	/* This will traverse through all nodes in the tree and print stats*/
	for (i = 0; i < CAM_CPAS_MAX_TREE_NODES; i++) {
		if (!soc_private->tree_node[i])
			continue;
		curr_node = soc_private->tree_node[i];

		CAM_INFO(CAM_CPAS,
			"[%s] Cell[%d] level[%d] PortIdx[%d][%d] camnoc_bw[%d %d %lld %lld] mnoc_bw[%lld %lld]",
			curr_node->node_name, curr_node->cell_idx,
			curr_node->level_idx,
			curr_node->axi_port_idx_arr[CAM_CPAS_PORT_AXI_PORT],
			curr_node->camnoc_axi_port_idx,
			curr_node->camnoc_max_needed,
			curr_node->bus_width_factor,
			curr_node->camnoc_bw,
			curr_node->camnoc_bw * curr_node->bus_width_factor,
			curr_node->mnoc_ab_bw, curr_node->mnoc_ib_bw);
	}

	cam_cpas_dump_monitor_array(cpas_hw);

	if (cpas_core->internal_ops.print_poweron_settings)
		cpas_core->internal_ops.print_poweron_settings(cpas_hw);
	else
		CAM_DBG(CAM_CPAS, "No ops for print_poweron_settings");

	return 0;
}

static void cam_cpas_update_monitor_array(struct cam_hw_info *cpas_hw,
	const char *identifier_string, int32_t identifier_value)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_camnoc_info *camnoc_info =
		(struct cam_camnoc_info *) cpas_core->camnoc_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	struct cam_cpas_monitor *entry;
	int iterator;
	int i, j = 0;
	int reg_camnoc = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	uint32_t val = 0;

	if (!camnoc_info) {
		CAM_ERR(CAM_CPAS, "Invalid camnoc info for hw_version: 0x%x",
			cpas_hw->soc_info.hw_version);
		return;
	}

	CAM_CPAS_INC_MONITOR_HEAD(&cpas_core->monitor_head, &iterator);

	entry = &cpas_core->monitor_entries[iterator];

	CAM_GET_TIMESTAMP(entry->timestamp);
	strlcpy(entry->identifier_string, identifier_string,
		sizeof(entry->identifier_string));

	entry->identifier_value = identifier_value;

	for (i = 0; i < cpas_core->num_axi_ports; i++) {
		entry->axi_info[i].axi_port_name =
			cpas_core->axi_port[i].axi_port_name;
		entry->axi_info[i].ab_bw = cpas_core->axi_port[i].ab_bw;
		entry->axi_info[i].ib_bw = cpas_core->axi_port[i].ib_bw;
		entry->axi_info[i].camnoc_bw = cpas_core->axi_port[i].camnoc_bw;
		entry->axi_info[i].applied_ab_bw =
			cpas_core->axi_port[i].applied_ab_bw;
		entry->axi_info[i].applied_ib_bw =
			cpas_core->axi_port[i].applied_ib_bw;
	}

	entry->applied_camnoc_clk = cpas_core->applied_camnoc_axi_rate;
	entry->applied_ahb_level = cpas_core->ahb_bus_client.curr_vote_level;

	if ((cpas_core->streamon_clients > 0) &&
		(cpas_core->regbase_index[CAM_CPAS_REG_RPMH] != -1) &&
		soc_private->rpmh_info[CAM_RPMH_NUMBER_OF_BCMS]) {
		int reg_base_index =
			cpas_core->regbase_index[CAM_CPAS_REG_RPMH];
		void __iomem *rpmh_base =
			soc_info->reg_map[reg_base_index].mem_base;
		uint32_t fe_ddr_offset =
			soc_private->rpmh_info[CAM_RPMH_BCM_FE_OFFSET] +
			(0x4 * soc_private->rpmh_info[CAM_RPMH_BCM_DDR_INDEX]);
		uint32_t fe_mnoc_offset =
			soc_private->rpmh_info[CAM_RPMH_BCM_FE_OFFSET] +
			(0x4 * soc_private->rpmh_info[CAM_RPMH_BCM_MNOC_INDEX]);
		uint32_t be_ddr_offset =
			soc_private->rpmh_info[CAM_RPMH_BCM_BE_OFFSET] +
			(0x4 * soc_private->rpmh_info[CAM_RPMH_BCM_DDR_INDEX]);
		uint32_t be_mnoc_offset =
			soc_private->rpmh_info[CAM_RPMH_BCM_BE_OFFSET] +
			(0x4 * soc_private->rpmh_info[CAM_RPMH_BCM_MNOC_INDEX]);
		uint32_t be_shub_offset =
			soc_private->rpmh_info[CAM_RPMH_BCM_BE_OFFSET] +
			(0x4 * 1); /* i=1 for SHUB, hardcode for now */

		/*
		 * 0x4, 0x800 - DDR
		 * 0x800, 0x810 - mmnoc
		 */
		entry->fe_ddr = cam_io_r_mb(rpmh_base + fe_ddr_offset);
		entry->fe_mnoc = cam_io_r_mb(rpmh_base + fe_mnoc_offset);
		entry->be_ddr = cam_io_r_mb(rpmh_base + be_ddr_offset);
		entry->be_mnoc = cam_io_r_mb(rpmh_base + be_mnoc_offset);
		entry->be_shub = cam_io_r_mb(rpmh_base + be_shub_offset);

		CAM_DBG(CAM_CPAS,
			"fe_ddr=0x%x, fe_mnoc=0x%x, be_ddr=0x%x, be_mnoc=0x%x",
			entry->fe_ddr, entry->fe_mnoc, entry->be_ddr,
			entry->be_mnoc);
	}

	for (i = 0; i < camnoc_info->specific_size; i++) {
		if ((!camnoc_info->specific[i].enable) ||
			(!camnoc_info->specific[i].maxwr_low.enable))
			continue;

		if (j >= CAM_CAMNOC_FILL_LVL_REG_INFO_MAX) {
			CAM_WARN(CAM_CPAS,
				"CPAS monitor reg info buffer full, max : %d",
				j);
			break;
		}

		entry->camnoc_port_name[j] = camnoc_info->specific[i].port_name;
		val = cam_io_r_mb(soc_info->reg_map[reg_camnoc].mem_base +
			camnoc_info->specific[i].maxwr_low.offset);
		entry->camnoc_fill_level[j] = val;
		j++;
	}

	entry->num_camnoc_lvl_regs = j;

	if (soc_private->enable_smart_qos) {
		for (i = 0; i < soc_private->smart_qos_info->num_rt_wr_nius; i++) {
			struct cam_cpas_tree_node *niu_node =
				soc_private->smart_qos_info->rt_wr_niu_node[i];

			entry->rt_wr_niu_pri_lut[i] =
				cam_io_r_mb(soc_info->reg_map[reg_camnoc].mem_base +
				niu_node->pri_lut_low_offset);
		}
	}
}

static void cam_cpas_dump_monitor_array(
	struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *) cpas_hw->soc_info.soc_private;
	int i = 0, j = 0;
	int64_t state_head = 0;
	uint32_t index, num_entries, oldest_entry;
	uint64_t ms, hrs, min, sec;
	struct cam_cpas_monitor *entry;
	struct timespec64 curr_timestamp;
	char log_buf[CAM_CPAS_LOG_BUF_LEN];
	size_t len;

	if (!cpas_core->full_state_dump)
		return;

	state_head = atomic64_read(&cpas_core->monitor_head);

	if (state_head == -1) {
		CAM_WARN(CAM_CPAS, "No valid entries in cpas monitor array");
		return;
	} else if (state_head < CAM_CPAS_MONITOR_MAX_ENTRIES) {
		num_entries = state_head;
		oldest_entry = 0;
	} else {
		num_entries = CAM_CPAS_MONITOR_MAX_ENTRIES;
		div_u64_rem(state_head + 1,
			CAM_CPAS_MONITOR_MAX_ENTRIES, &oldest_entry);
	}

	CAM_GET_TIMESTAMP(curr_timestamp);
	CAM_CONVERT_TIMESTAMP_FORMAT(curr_timestamp, hrs, min, sec, ms);

	CAM_INFO(CAM_CPAS,
		"**** %llu:%llu:%llu.%llu : ======== Dumping monitor information ===========",
		hrs, min, sec, ms);

	index = oldest_entry;

	for (i = 0; i < num_entries; i++) {
		entry = &cpas_core->monitor_entries[index];
		CAM_CONVERT_TIMESTAMP_FORMAT(entry->timestamp, hrs, min, sec, ms);
		memset(log_buf, 0, sizeof(log_buf));
		len = 0;

		CAM_INFO(CAM_CPAS,
			"**** %llu:%llu:%llu.%llu : Index[%d] Identifier[%s][%d] camnoc=%lld, ahb=%d",
			hrs, min, sec, ms,
			index,
			entry->identifier_string, entry->identifier_value,
			entry->applied_camnoc_clk, entry->applied_ahb_level);

		for (j = 0; j < cpas_core->num_axi_ports; j++) {
			CAM_INFO(CAM_CPAS,
				"MNOC BW [%s] : ab=%lld, ib=%lld, camnoc=%lld",
				entry->axi_info[j].axi_port_name,
				entry->axi_info[j].applied_ab_bw,
				entry->axi_info[j].applied_ib_bw,
				entry->axi_info[j].camnoc_bw);
		}

		if (cpas_core->regbase_index[CAM_CPAS_REG_RPMH] != -1) {
			CAM_INFO(CAM_CPAS,
				"fe_ddr=0x%x, fe_mnoc=0x%x, be_ddr=0x%x, be_mnoc=0x%x, be_shub=0x%x",
				entry->fe_ddr, entry->fe_mnoc,
				entry->be_ddr, entry->be_mnoc, entry->be_shub);
		}

		for (j = 0; j < entry->num_camnoc_lvl_regs; j++) {
			len += scnprintf((log_buf + len),
				(CAM_CPAS_LOG_BUF_LEN - len), " %s:[%d %d]",
				entry->camnoc_port_name[j],
				(entry->camnoc_fill_level[j] & 0x7FF),
				(entry->camnoc_fill_level[j] & 0x7F0000) >> 16);
		}
		CAM_INFO(CAM_CPAS, "CAMNOC REG[Queued Pending] %s", log_buf);

		if (soc_private->enable_smart_qos) {
			len = 0;
			for (j = 0; j < soc_private->smart_qos_info->num_rt_wr_nius; j++) {
				struct cam_cpas_tree_node *niu_node =
					soc_private->smart_qos_info->rt_wr_niu_node[j];

				len += scnprintf((log_buf + len),
					(CAM_CPAS_LOG_BUF_LEN - len), " [%s: 0x%x]",
					niu_node->node_name,
					entry->rt_wr_niu_pri_lut[j]);
			}
			CAM_INFO(CAM_CPAS, "SmartQoS [Node: Pri_lut_low] %s", log_buf);
		}

		index = (index + 1) % CAM_CPAS_MONITOR_MAX_ENTRIES;
	}
}

static int cam_cpas_log_event(struct cam_hw_info *cpas_hw,
	const char *identifier_string, int32_t identifier_value)
{
	cam_cpas_update_monitor_array(cpas_hw, identifier_string,
		identifier_value);

	return 0;
}

static int cam_cpas_select_qos(struct cam_hw_info *cpas_hw,
	uint32_t selection_mask)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	int rc = 0;

	mutex_lock(&cpas_hw->hw_mutex);

	if (cpas_hw->hw_state == CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_CPAS,
			"Hw already in power up state, can't change QoS settings");
		rc = -EINVAL;
		goto done;
	}

	if (cpas_core->internal_ops.setup_qos_settings) {
		rc = cpas_core->internal_ops.setup_qos_settings(cpas_hw,
			selection_mask);
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in changing QoS %d", rc);
	} else {
		CAM_WARN(CAM_CPAS, "No ops for qos_settings");
	}

done:
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_activate_cache(
	struct cam_hw_info *cpas_hw,
	struct cam_sys_cache_info *cache_info)
{
	int rc = 0;

	mutex_lock(&cpas_hw->hw_mutex);
	cache_info->ref_cnt++;
	if (cache_info->ref_cnt > 1) {
		mutex_unlock(&cpas_hw->hw_mutex);
		CAM_DBG(CAM_CPAS, "Cache: %s has already been activated cnt: %d",
			cache_info->name, cache_info->ref_cnt);
		return rc;
	}

	rc = llcc_slice_activate(cache_info->slic_desc);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Failed to activate cache:%s",
			cache_info->name);
		goto end;
	}

	mutex_unlock(&cpas_hw->hw_mutex);
	CAM_DBG(CAM_CPAS, "Activated cache:%s", cache_info->name);
	return rc;

end:
	cache_info->ref_cnt--;
	mutex_unlock(&cpas_hw->hw_mutex);
	return rc;
}

static int cam_cpas_deactivate_cache(
	struct cam_hw_info *cpas_hw,
	struct cam_sys_cache_info *cache_info)
{
	int rc = 0;

	mutex_lock(&cpas_hw->hw_mutex);
	if (!cache_info->ref_cnt) {
		mutex_unlock(&cpas_hw->hw_mutex);
		CAM_ERR(CAM_CPAS, "Unbalanced deactivate");
		return -EFAULT;
	}

	cache_info->ref_cnt--;
	if (cache_info->ref_cnt) {
		mutex_unlock(&cpas_hw->hw_mutex);
		CAM_DBG(CAM_CPAS, "activate cnt for: %s non-zero: %d",
			cache_info->name, cache_info->ref_cnt);
		return rc;
	}

	rc = llcc_slice_deactivate(cache_info->slic_desc);
	if (rc)
		CAM_ERR(CAM_CPAS, "Failed to deactivate cache:%s",
			cache_info->name);

	mutex_unlock(&cpas_hw->hw_mutex);
	CAM_DBG(CAM_CPAS, "De-activated cache:%s", cache_info->name);
	return rc;
}

static inline int cam_cpas_validate_cache_type(
	uint32_t num_caches, enum cam_sys_cache_config_types type)
{
	if ((!num_caches) || (type < 0) || (type >= num_caches))
		return -EINVAL;
	else
		return 0;
}

static int cam_cpas_get_slice_id(
	struct cam_hw_info *cpas_hw,
	enum cam_sys_cache_config_types type)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *)cpas_hw->soc_info.soc_private;
	uint32_t num_caches = soc_private->num_caches;
	int scid = -1, i;

	if (cam_cpas_validate_cache_type(num_caches, type))
		goto end;

	for (i = 0; i < num_caches; i++) {
		if (type == soc_private->llcc_info[i].type) {
			scid = soc_private->llcc_info[i].scid;
			CAM_DBG(CAM_CPAS, "Cache:%s type:%d scid:%d",
				soc_private->llcc_info[i].name, type, scid);
			break;
		}
	}

end:
	return scid;
}

static int cam_cpas_activate_cache_slice(
	struct cam_hw_info *cpas_hw,
	enum cam_sys_cache_config_types type)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *)cpas_hw->soc_info.soc_private;
	uint32_t num_caches = soc_private->num_caches;
	int rc = 0, i;

	CAM_DBG(CAM_CPAS, "Activate type: %d", type);
	if (cam_cpas_validate_cache_type(num_caches, type))
		goto end;

	for (i = 0; i < num_caches; i++) {
		if (type == soc_private->llcc_info[i].type)
			rc = cam_cpas_activate_cache(cpas_hw,
				&soc_private->llcc_info[i]);
	}

end:
	return rc;
}

static int cam_cpas_deactivate_cache_slice(
	struct cam_hw_info *cpas_hw,
	enum cam_sys_cache_config_types type)
{
	struct cam_cpas_private_soc *soc_private =
		(struct cam_cpas_private_soc *)cpas_hw->soc_info.soc_private;
	uint32_t num_caches = soc_private->num_caches;
	int rc = 0, i;

	CAM_DBG(CAM_CPAS, "De-activate type: %d", type);
	if (cam_cpas_validate_cache_type(num_caches, type))
		goto end;

	for (i = 0; i < num_caches; i++) {
		if (type == soc_private->llcc_info[i].type)
			rc = cam_cpas_deactivate_cache(cpas_hw,
				&soc_private->llcc_info[i]);
	}

end:
	return rc;
}

static int cam_cpas_hw_process_cmd(void *hw_priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!hw_priv || !cmd_args ||
		(cmd_type >= CAM_CPAS_HW_CMD_INVALID)) {
		CAM_ERR(CAM_CPAS, "Invalid arguments %pK %pK %d",
			hw_priv, cmd_args, cmd_type);
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_CPAS_HW_CMD_REGISTER_CLIENT: {
		struct cam_cpas_register_params *register_params;

		if (sizeof(struct cam_cpas_register_params) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		register_params = (struct cam_cpas_register_params *)cmd_args;
		rc = cam_cpas_hw_register_client(hw_priv, register_params);
		break;
	}
	case CAM_CPAS_HW_CMD_UNREGISTER_CLIENT: {
		uint32_t *client_handle;

		if (sizeof(uint32_t) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		client_handle = (uint32_t *)cmd_args;
		rc = cam_cpas_hw_unregister_client(hw_priv, *client_handle);
		break;
	}
	case CAM_CPAS_HW_CMD_REG_WRITE: {
		struct cam_cpas_hw_cmd_reg_read_write *reg_write;

		if (sizeof(struct cam_cpas_hw_cmd_reg_read_write) !=
			arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		reg_write =
			(struct cam_cpas_hw_cmd_reg_read_write *)cmd_args;
		rc = cam_cpas_hw_reg_write(hw_priv, reg_write->client_handle,
			reg_write->reg_base, reg_write->offset, reg_write->mb,
			reg_write->value);
		break;
	}
	case CAM_CPAS_HW_CMD_REG_READ: {
		struct cam_cpas_hw_cmd_reg_read_write *reg_read;

		if (sizeof(struct cam_cpas_hw_cmd_reg_read_write) !=
			arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		reg_read =
			(struct cam_cpas_hw_cmd_reg_read_write *)cmd_args;
		rc = cam_cpas_hw_reg_read(hw_priv,
			reg_read->client_handle, reg_read->reg_base,
			reg_read->offset, reg_read->mb, &reg_read->value);

		break;
	}
	case CAM_CPAS_HW_CMD_AHB_VOTE: {
		struct cam_cpas_hw_cmd_ahb_vote *cmd_ahb_vote;

		if (sizeof(struct cam_cpas_hw_cmd_ahb_vote) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		cmd_ahb_vote = (struct cam_cpas_hw_cmd_ahb_vote *)cmd_args;
		rc = cam_cpas_hw_update_ahb_vote(hw_priv,
			cmd_ahb_vote->client_handle, cmd_ahb_vote->ahb_vote);
		break;
	}
	case CAM_CPAS_HW_CMD_AXI_VOTE: {
		struct cam_cpas_hw_cmd_axi_vote *cmd_axi_vote;

		if (sizeof(struct cam_cpas_hw_cmd_axi_vote) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		cmd_axi_vote = (struct cam_cpas_hw_cmd_axi_vote *)cmd_args;
		rc = cam_cpas_hw_update_axi_vote(hw_priv,
			cmd_axi_vote->client_handle, cmd_axi_vote->axi_vote);
		break;
	}
	case CAM_CPAS_HW_CMD_LOG_VOTE: {
		rc = cam_cpas_log_vote(hw_priv);
		break;
	}

	case CAM_CPAS_HW_CMD_LOG_EVENT: {
		struct cam_cpas_hw_cmd_notify_event *event;

		if (sizeof(struct cam_cpas_hw_cmd_notify_event) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		event = (struct cam_cpas_hw_cmd_notify_event *)cmd_args;

		rc = cam_cpas_log_event(hw_priv, event->identifier_string,
			event->identifier_value);
		break;
	}

	case CAM_CPAS_HW_CMD_SELECT_QOS: {
		uint32_t *selection_mask;

		if (sizeof(uint32_t) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		selection_mask = (uint32_t *)cmd_args;
		rc = cam_cpas_select_qos(hw_priv, *selection_mask);
		break;
	}
	case CAM_CPAS_HW_CMD_GET_SCID: {
		enum cam_sys_cache_config_types type;

		if (sizeof(enum cam_sys_cache_config_types) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}
		type = *((enum cam_sys_cache_config_types *) cmd_args);
		rc = cam_cpas_get_slice_id(hw_priv, type);
	}
		break;
	case CAM_CPAS_HW_CMD_ACTIVATE_LLC: {
		enum cam_sys_cache_config_types type;

		if (sizeof(enum cam_sys_cache_config_types) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}
		type = *((enum cam_sys_cache_config_types *) cmd_args);
		rc = cam_cpas_activate_cache_slice(hw_priv, type);
	}
		break;
	case CAM_CPAS_HW_CMD_DEACTIVATE_LLC: {
		enum cam_sys_cache_config_types type;

		if (sizeof(enum cam_sys_cache_config_types) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}
		type = *((enum cam_sys_cache_config_types *) cmd_args);
		rc = cam_cpas_deactivate_cache_slice(hw_priv, type);
	}
		break;
	case CAM_CPAS_HW_CMD_DUMP_BUFF_FILL_INFO: {
		uint32_t *client_handle;

		if (sizeof(uint32_t) != arg_size) {
			CAM_ERR(CAM_CPAS, "cmd_type %d, size mismatch %d",
				cmd_type, arg_size);
			break;
		}

		client_handle = (uint32_t *)cmd_args;
		rc = cam_cpas_hw_dump_camnoc_buff_fill_info(hw_priv,
			*client_handle);
		break;
	}
	default:
		CAM_ERR(CAM_CPAS, "CPAS HW command not valid =%d", cmd_type);
		break;
	}

	return rc;
}

static int cam_cpas_util_client_setup(struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	int i;

	for (i = 0; i < CAM_CPAS_MAX_CLIENTS; i++) {
		mutex_init(&cpas_core->client_mutex[i]);
	}

	return 0;
}

int cam_cpas_util_client_cleanup(struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	int i;

	for (i = 0; i < CAM_CPAS_MAX_CLIENTS; i++) {
		if (cpas_core->cpas_client[i] &&
			cpas_core->cpas_client[i]->registered) {
			cam_cpas_hw_unregister_client(cpas_hw, i);
		}
		kfree(cpas_core->cpas_client[i]);
		cpas_core->cpas_client[i] = NULL;
		mutex_destroy(&cpas_core->client_mutex[i]);
	}

	return 0;
}

static int cam_cpas_util_get_internal_ops(struct platform_device *pdev,
	struct cam_hw_intf *hw_intf, struct cam_cpas_internal_ops *internal_ops)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc;
	const char *compat_str = NULL;

	rc = of_property_read_string_index(of_node, "arch-compat", 0,
		(const char **)&compat_str);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed to get arch-compat rc=%d", rc);
		return -EINVAL;
	}

	if (strnstr(compat_str, "camss_top", strlen(compat_str))) {
		hw_intf->hw_type = CAM_HW_CAMSSTOP;
		rc = cam_camsstop_get_internal_ops(internal_ops);
	} else if (strnstr(compat_str, "cpas_top", strlen(compat_str))) {
		hw_intf->hw_type = CAM_HW_CPASTOP;
		rc = cam_cpastop_get_internal_ops(internal_ops);
	} else {
		CAM_ERR(CAM_CPAS, "arch-compat %s not supported", compat_str);
		rc = -EINVAL;
	}

	return rc;
}

static int cam_cpas_util_create_debugfs(struct cam_cpas *cpas_core)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;

	dbgfileptr = debugfs_create_dir("camera_cpas", NULL);
	if (!dbgfileptr) {
		CAM_ERR(CAM_CPAS,"DebugFS could not create directory!");
		rc = -ENOENT;
		goto end;
	}
	/* Store parent inode for cleanup in caller */
	cpas_core->dentry = dbgfileptr;

	debugfs_create_bool("ahb_bus_scaling_disable", 0644,
		cpas_core->dentry, &cpas_core->ahb_bus_scaling_disable);

	debugfs_create_bool("full_state_dump", 0644,
		cpas_core->dentry, &cpas_core->full_state_dump);

	debugfs_create_bool("smart_qos_dump", 0644,
		cpas_core->dentry, &cpas_core->smart_qos_dump);
end:
	return rc;
}

int cam_cpas_hw_probe(struct platform_device *pdev,
	struct cam_hw_intf **hw_intf)
{
	int rc = 0;
	int i;
	struct cam_hw_info *cpas_hw = NULL;
	struct cam_hw_intf *cpas_hw_intf = NULL;
	struct cam_cpas *cpas_core = NULL;
	struct cam_cpas_private_soc *soc_private;
	struct cam_cpas_internal_ops *internal_ops;

	cpas_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!cpas_hw_intf)
		return -ENOMEM;

	cpas_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!cpas_hw) {
		kfree(cpas_hw_intf);
		return -ENOMEM;
	}

	cpas_core = kzalloc(sizeof(struct cam_cpas), GFP_KERNEL);
	if (!cpas_core) {
		kfree(cpas_hw);
		kfree(cpas_hw_intf);
		return -ENOMEM;
	}

	for (i = 0; i < CAM_CPAS_REG_MAX; i++)
		cpas_core->regbase_index[i] = -1;

	cpas_hw_intf->hw_priv = cpas_hw;
	cpas_hw->core_info = cpas_core;

	cpas_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cpas_hw->soc_info.pdev = pdev;
	cpas_hw->soc_info.dev = &pdev->dev;
	cpas_hw->soc_info.dev_name = pdev->name;
	cpas_hw->open_count = 0;
	cpas_core->ahb_bus_scaling_disable = false;
	cpas_core->full_state_dump = false;
	cpas_core->smart_qos_dump = false;

	atomic64_set(&cpas_core->monitor_head, -1);

	mutex_init(&cpas_hw->hw_mutex);
	spin_lock_init(&cpas_hw->hw_lock);
	init_completion(&cpas_hw->hw_complete);

	cpas_hw_intf->hw_ops.get_hw_caps = cam_cpas_hw_get_hw_info;
	cpas_hw_intf->hw_ops.init = cam_cpas_hw_init;
	cpas_hw_intf->hw_ops.deinit = NULL;
	cpas_hw_intf->hw_ops.reset = NULL;
	cpas_hw_intf->hw_ops.reserve = NULL;
	cpas_hw_intf->hw_ops.release = NULL;
	cpas_hw_intf->hw_ops.start = cam_cpas_hw_start;
	cpas_hw_intf->hw_ops.stop = cam_cpas_hw_stop;
	cpas_hw_intf->hw_ops.read = NULL;
	cpas_hw_intf->hw_ops.write = NULL;
	cpas_hw_intf->hw_ops.process_cmd = cam_cpas_hw_process_cmd;

	cpas_core->work_queue = alloc_workqueue(CAM_CPAS_WORKQUEUE_NAME,
		WQ_UNBOUND | WQ_MEM_RECLAIM, CAM_CPAS_INFLIGHT_WORKS);
	if (!cpas_core->work_queue) {
		rc = -ENOMEM;
		goto release_mem;
	}

	internal_ops = &cpas_core->internal_ops;
	rc = cam_cpas_util_get_internal_ops(pdev, cpas_hw_intf, internal_ops);
	if (rc)
		goto release_workq;

	rc = cam_cpas_soc_init_resources(&cpas_hw->soc_info,
		internal_ops->handle_irq, cpas_hw);
	if (rc)
		goto release_workq;

	soc_private = (struct cam_cpas_private_soc *)
		cpas_hw->soc_info.soc_private;
	cpas_core->num_clients = soc_private->num_clients;
	atomic_set(&cpas_core->irq_count, 0);
	init_waitqueue_head(&cpas_core->irq_count_wq);

	if (internal_ops->setup_regbase) {
		rc = internal_ops->setup_regbase(&cpas_hw->soc_info,
			cpas_core->regbase_index, CAM_CPAS_REG_MAX);
		if (rc)
			goto deinit_platform_res;
	}

	rc = cam_cpas_util_client_setup(cpas_hw);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in client setup, rc=%d", rc);
		goto deinit_platform_res;
	}

	rc = cam_cpas_util_register_bus_client(&cpas_hw->soc_info,
		cpas_hw->soc_info.pdev->dev.of_node,
		&cpas_core->ahb_bus_client);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in ahb setup, rc=%d", rc);
		goto client_cleanup;
	}

	rc = cam_cpas_util_axi_setup(cpas_core, &cpas_hw->soc_info);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in axi setup, rc=%d", rc);
		goto ahb_cleanup;
	}

	/* Need to vote first before enabling clocks */
	rc = cam_cpas_util_vote_default_ahb_axi(cpas_hw, true);
	if (rc)
		goto axi_cleanup;

	rc = cam_cpas_soc_enable_resources(&cpas_hw->soc_info, CAM_SVS_VOTE);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in soc_enable_resources, rc=%d", rc);
		goto remove_default_vote;
	}

	if (internal_ops->get_hw_info) {
		rc = internal_ops->get_hw_info(cpas_hw, &cpas_core->hw_caps);
		if (rc) {
			CAM_ERR(CAM_CPAS, "failed in get_hw_info, rc=%d", rc);
			goto disable_soc_res;
		}
	} else {
		CAM_ERR(CAM_CPAS, "Invalid get_hw_info");
		goto disable_soc_res;
	}

	rc = cam_cpas_hw_init(cpas_hw_intf->hw_priv,
		&cpas_core->hw_caps, sizeof(struct cam_cpas_hw_caps));
	if (rc)
		goto disable_soc_res;

	rc = cam_cpas_soc_disable_resources(&cpas_hw->soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed in soc_disable_resources, rc=%d", rc);
		goto remove_default_vote;
	}

	rc = cam_cpas_util_vote_default_ahb_axi(cpas_hw, false);
	if (rc)
		goto axi_cleanup;

	rc = cam_cpas_util_create_debugfs(cpas_core);

	*hw_intf = cpas_hw_intf;
	return 0;

disable_soc_res:
	cam_cpas_soc_disable_resources(&cpas_hw->soc_info, true, true);
remove_default_vote:
	cam_cpas_util_vote_default_ahb_axi(cpas_hw, false);
axi_cleanup:
	cam_cpas_util_axi_cleanup(cpas_core, &cpas_hw->soc_info);
ahb_cleanup:
	cam_cpas_util_unregister_bus_client(&cpas_core->ahb_bus_client);
client_cleanup:
	cam_cpas_util_client_cleanup(cpas_hw);
	cam_cpas_node_tree_cleanup(cpas_core, cpas_hw->soc_info.soc_private);
deinit_platform_res:
	cam_cpas_soc_deinit_resources(&cpas_hw->soc_info);
release_workq:
	flush_workqueue(cpas_core->work_queue);
	destroy_workqueue(cpas_core->work_queue);
release_mem:
	mutex_destroy(&cpas_hw->hw_mutex);
	kfree(cpas_core);
	kfree(cpas_hw);
	kfree(cpas_hw_intf);
	CAM_ERR(CAM_CPAS, "failed in hw probe");
	return rc;
}

int cam_cpas_hw_remove(struct cam_hw_intf *cpas_hw_intf)
{
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;

	if (!cpas_hw_intf) {
		CAM_ERR(CAM_CPAS, "cpas interface not initialized");
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info *)cpas_hw_intf->hw_priv;
	cpas_core = (struct cam_cpas *)cpas_hw->core_info;

	if (cpas_hw->hw_state == CAM_HW_STATE_POWER_UP) {
		CAM_ERR(CAM_CPAS, "cpas hw is in power up state");
		return -EINVAL;
	}

	cam_cpas_util_axi_cleanup(cpas_core, &cpas_hw->soc_info);
	cam_cpas_node_tree_cleanup(cpas_core, cpas_hw->soc_info.soc_private);
	cam_cpas_util_unregister_bus_client(&cpas_core->ahb_bus_client);
	cam_cpas_util_client_cleanup(cpas_hw);
	cam_cpas_soc_deinit_resources(&cpas_hw->soc_info);
	debugfs_remove_recursive(cpas_core->dentry);
	cpas_core->dentry = NULL;
	flush_workqueue(cpas_core->work_queue);
	destroy_workqueue(cpas_core->work_queue);
	mutex_destroy(&cpas_hw->hw_mutex);
	kfree(cpas_core);
	kfree(cpas_hw);
	kfree(cpas_hw_intf);

	return 0;
}
