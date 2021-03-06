/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_sysfs_get_range_ext.c
 *
 * implementation for creating sysfs file get_range_ext
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_sysfs.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs_get_range_ext.h>
#include "wma_api.h"
#include "wmi_unified.h"

static ssize_t
__hdd_sysfs_get_range_ext_show(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	int value;
	int ret;

	if (hdd_validate_adapter(adapter)) {
		hdd_err_rl("adapter validate fail");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	hdd_debug("GET WMI_VDEV_PARAM_HE_RANGE_EXT");
	value = wma_cli_get_command(adapter->vdev_id,
				    WMI_VDEV_PARAM_HE_RANGE_EXT, VDEV_CMD);

	return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t
hdd_sysfs_get_range_ext_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_get_range_ext_show(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(get_range_ext, 0440,
		   hdd_sysfs_get_range_ext_show, NULL);

int hdd_sysfs_get_range_ext_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_get_range_ext);
	if (error)
		hdd_err("could not create get_range_ext sysfs file");

	return error;
}

void hdd_sysfs_get_range_ext_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_get_range_ext);
}
