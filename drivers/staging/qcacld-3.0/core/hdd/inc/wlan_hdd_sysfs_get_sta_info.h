/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_get_sta_info.h
 *
 * implementation for creating sysfs file get_sta_info
 */

#ifndef _WLAN_HDD_SYSFS_GET_STA_INFO_H
#define _WLAN_HDD_SYSFS_GET_STA_INFO_H

#if defined(WLAN_SYSFS) && defined(WLAN_SYSFS_GET_STA_INFO)
/**
 * hdd_sysfs_get_sta_info_interface_create() - API to create get_sta_info sysfs
 * interface
 * @adapter: pointer to adapter
 *
 * this file is created for SAP adapter.
 * file path: /sys/class/net/wlanxx/get_sta_info
 *	where wlanxx is adapter name
 *
 * usage:
 *      cat /sys/class/net/wlanxx/get_sta_info
 *
 * Return: none
 */
void hdd_sysfs_get_sta_info_interface_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_get_sta_info_interface_destroy() - API to destroy get_sta_info
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_get_sta_info_interface_destroy(struct hdd_adapter *adapter);

#else
static inline
void hdd_sysfs_get_sta_info_interface_create(struct hdd_adapter *adapter)
{
}

static inline
void hdd_sysfs_get_sta_info_interface_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_GET_STA_INFO_H */
