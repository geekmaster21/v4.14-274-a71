/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_sysfs_set_fw_mode_cfg.h
 *
 * implementation for creating sysfs file set_fw_mode_cfg
 */

#ifndef _WLAN_HDD_SYSFS_SET_FW_MODE_CFG_H
#define _WLAN_HDD_SYSFS_SET_FW_MODE_CFG_H

#if defined(WLAN_SYSFS) && defined(CONFIG_WLAN_SET_FW_MODE_CFG)
/**
 * hdd_sysfs_set_fw_mode_cfg_create() - API to create set_fw_mode_cfg
 * @driver_kobject: sysfs driver kobject
 *
 * file path: /sys/kernel/wifi/set_fw_mode_cfg
 *
 * usage:
 *      echo [arg_0] [arg_1] > set_fw_mode_cfg
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_set_fw_mode_cfg_create(struct kobject *driver_kobject);

/**
 * hdd_sysfs_set_fw_mode_cfg_destroy() -
 *   API to destroy set_fw_mode_cfg
 *
 * Return: none
 */
void
hdd_sysfs_set_fw_mode_cfg_destroy(struct kobject *driver_kobject);
#else
static inline int
hdd_sysfs_set_fw_mode_cfg_create(struct kobject *driver_kobject)
{
	return 0;
}

static inline void
hdd_sysfs_set_fw_mode_cfg_destroy(struct kobject *driver_kobject)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_SET_FW_MODE_CFG_H */
