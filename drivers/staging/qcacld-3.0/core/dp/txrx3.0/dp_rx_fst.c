/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "dp_types.h"
#include "qdf_mem.h"
#include "qdf_nbuf.h"
#include "cfg_dp.h"
#include "wlan_cfg.h"
#include "dp_types.h"
#include "hal_rx_flow.h"
#include "dp_htt.h"
#include "dp_internal.h"

#ifdef WLAN_SUPPORT_RX_FISA

void dp_rx_dump_fisa_table(struct dp_soc *soc)
{
	hal_rx_dump_fse_table(soc->rx_fst->hal_rx_fst);
}

/**
 * dp_rx_fst_attach() - Initialize Rx FST and setup necessary parameters
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: Handle to flow search table entry
 */
QDF_STATUS dp_rx_fst_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *fst;
	uint8_t *hash_key;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;

	/* Check if it is enabled in the INI */
	if (!wlan_cfg_is_rx_fisa_enabled(cfg)) {
		dp_err("RX FISA feature is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

#ifdef NOT_YET /* Not required for now */
	/* Check if FW supports */
	if (!wlan_psoc_nif_fw_ext_cap_get((void *)pdev->ctrl_pdev,
					  WLAN_SOC_CEXT_RX_FSE_SUPPORT)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "rx fse disabled in FW\n");
		wlan_cfg_set_rx_flow_tag_enabled(cfg, false);
		return QDF_STATUS_E_NOSUPPORT;
	}
#endif
	if (soc->rx_fst) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST already allocated\n");
		return QDF_STATUS_SUCCESS;
	}

	fst = qdf_mem_malloc(sizeof(struct dp_rx_fst));
	if (!fst) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST allocation failed\n");
		return QDF_STATUS_E_NOMEM;
	}

	fst->max_skid_length = wlan_cfg_rx_fst_get_max_search(cfg);
	fst->max_entries = wlan_cfg_get_rx_flow_search_table_size(cfg);
	hash_key = wlan_cfg_rx_fst_get_hash_key(cfg);

	fst->hash_mask = fst->max_entries - 1;
	fst->num_entries = 0;
	dp_err("FST setup params FT size %d, hash_mask 0x%x, skid_length %d",
	       fst->max_entries, fst->hash_mask, fst->max_skid_length);

	fst->base = (uint8_t *)qdf_mem_malloc(DP_RX_GET_SW_FT_ENTRY_SIZE *
					       fst->max_entries);

	if (!fst->base) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Rx fst->base allocation failed, #entries:%d\n",
			  fst->max_entries);

		goto out2;
	}

	fst->hal_rx_fst = hal_rx_fst_attach(soc->osdev,
					    &fst->hal_rx_fst_base_paddr,
					    fst->max_entries,
					    fst->max_skid_length, hash_key);

	if (qdf_unlikely(!fst->hal_rx_fst)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Rx Hal fst allocation failed, #entries:%d\n",
			  fst->max_entries);
		goto out1;
	}

	qdf_spinlock_create(&fst->dp_rx_fst_lock);

	fst->soc_hdl = soc;
	soc->rx_fst = fst;
	soc->fisa_enable = true;

	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
		  "Rx FST attach successful, #entries:%d\n",
		  fst->max_entries);

	return QDF_STATUS_SUCCESS;
out1:
	qdf_mem_free(fst->base);
out2:
	qdf_mem_free(fst);
	return QDF_STATUS_E_NOMEM;
}

/**
 * dp_rx_flow_send_fst_fw_setup() - Program FST parameters in FW/HW post-attach
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: Success when fst parameters are programmed in FW, error otherwise
 */
QDF_STATUS dp_rx_flow_send_fst_fw_setup(struct dp_soc *soc,
					struct dp_pdev *pdev)
{
	struct dp_htt_rx_flow_fst_setup fisa_hw_fst_setup_cmd = {0};
	struct dp_rx_fst *fst = soc->rx_fst;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	QDF_STATUS status;

	/* mac_id = 0 is used to configure both macs with same FT */
	fisa_hw_fst_setup_cmd.pdev_id = 0;
	fisa_hw_fst_setup_cmd.max_entries = fst->max_entries;
	fisa_hw_fst_setup_cmd.max_search = fst->max_skid_length;
	fisa_hw_fst_setup_cmd.base_addr_lo = fst->hal_rx_fst_base_paddr &
							0xffffffff;
	fisa_hw_fst_setup_cmd.base_addr_hi = (fst->hal_rx_fst_base_paddr >> 32);
	fisa_hw_fst_setup_cmd.ip_da_sa_prefix =	HTT_RX_IPV4_COMPATIBLE_IPV6;
	fisa_hw_fst_setup_cmd.hash_key_len = HAL_FST_HASH_KEY_SIZE_BYTES;
	fisa_hw_fst_setup_cmd.hash_key = wlan_cfg_rx_fst_get_hash_key(cfg);

	status  = dp_htt_rx_flow_fst_setup(pdev, &fisa_hw_fst_setup_cmd);

	return status;
}

/**
 * dp_rx_fst_detach() - De-initialize Rx FST
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: None
 */
void dp_rx_fst_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *dp_fst;

	dp_fst = soc->rx_fst;
	if (qdf_likely(dp_fst)) {
		hal_rx_fst_detach(dp_fst->hal_rx_fst, soc->osdev);
		qdf_mem_free(dp_fst->base);
		qdf_spinlock_destroy(&dp_fst->dp_rx_fst_lock);
		qdf_mem_free(dp_fst);
	}
	soc->rx_fst = NULL;
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
		  "Rx FST detached\n");
}
#else /* WLAN_SUPPORT_RX_FISA */

#endif /* !WLAN_SUPPORT_RX_FISA */

