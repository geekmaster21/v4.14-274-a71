/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
/*
 * DOC: contains scan bss scoring logic
 */

#include <wlan_scan_utils_api.h>
#include "wlan_scan_main.h"
#include "wlan_scan_cache_db_i.h"
#ifdef WLAN_POLICY_MGR_ENABLE
#include "wlan_policy_mgr_api.h"
#endif
#include "wlan_reg_services_api.h"

#define SCM_20MHZ_BW_INDEX                  0
#define SCM_40MHZ_BW_INDEX                  1
#define SCM_80MHZ_BW_INDEX                  2
#define SCM_160MHZ_BW_INDEX                 3
#define SCM_MAX_BW_INDEX                    4

#define SCM_NSS_1x1_INDEX                   0
#define SCM_NSS_2x2_INDEX                   1
#define SCM_NSS_3x3_INDEX                   2
#define SCM_NSS_4x4_INDEX                   3
#define SCM_MAX_NSS_INDEX                   4

#define SCM_BAND_2G_INDEX                   0
#define SCM_BAND_5G_INDEX                   1
#define SCM_BAND_6G_INDEX                   2

/* 2 and 3 are reserved */
#define SCM_MAX_BAND_INDEX                  4

#define SCM_SCORE_INDEX_0                   0
#define SCM_SCORE_INDEX_3                   3
#define SCM_SCORE_INDEX_7                   7
#define SCM_SCORE_OFFSET_INDEX_7_4          4
#define SCM_SCORE_INDEX_11                  11
#define SCM_SCORE_OFFSET_INDEX_11_8         8
#define SCM_SCORE_MAX_INDEX                 15
#define SCM_SCORE_OFFSET_INDEX_15_12        12

#define SCM_MAX_OCE_WAN_DL_CAP 16

#define SCM_MAX_CHANNEL_WEIGHT 100
#define SCM_MAX_CHANNEL_UTILIZATION 100
#define SCM_MAX_ESTIMATED_AIR_TIME_FRACTION 255
#define MAX_AP_LOAD 255

#define SCM_MAX_WEIGHT_OF_PCL_CHANNELS 255
#define SCM_PCL_GROUPS_WEIGHT_DIFFERENCE 20

/* Congestion threshold (channel load %) to consider band and OCE WAN score */
#define CONGESTION_THRSHOLD_FOR_BAND_OCE_SCORE 75

bool scm_is_better_bss(struct scan_default_params *params,
	struct scan_cache_entry *bss1,
	struct scan_cache_entry *bss2)
{
	if (bss1->bss_score > bss2->bss_score)
		return true;
	else if (bss1->bss_score == bss2->bss_score)
		if (bss1->rssi_raw > bss2->rssi_raw)
			return true;

	return false;
}

/**
 * scm_get_rssi_pcnt_for_slot () - calculate rssi % score based on the slot
 * index between the high rssi and low rssi threshold
 * @high_rssi_threshold: High rssi of the window
 * @low_rssi_threshold: low rssi of the window
 * @high_rssi_pcnt: % score for the high rssi
 * @low_rssi_pcnt: %score for the low rssi
 * @bucket_size: bucket size of the window
 * @bss_rssi: Input rssi for which value need to be calculated
 *
 * Return : rssi pct to use for the given rssi
 */
static inline
int8_t scm_get_rssi_pcnt_for_slot(int32_t high_rssi_threshold,
	int32_t low_rssi_threshold, uint32_t high_rssi_pcnt,
	uint32_t low_rssi_pcnt, uint32_t bucket_size, int8_t bss_rssi)
{
	int8_t slot_index, slot_size, rssi_diff, num_slot, rssi_pcnt;

	num_slot = ((high_rssi_threshold -
		     low_rssi_threshold) / bucket_size) + 1;
	slot_size = ((high_rssi_pcnt - low_rssi_pcnt) +
		     (num_slot / 2)) / (num_slot);
	rssi_diff = high_rssi_threshold - bss_rssi;
	slot_index = (rssi_diff / bucket_size) + 1;
	rssi_pcnt = high_rssi_pcnt - (slot_size * slot_index);
	if (rssi_pcnt < low_rssi_pcnt)
		rssi_pcnt = low_rssi_pcnt;

	scm_debug("Window %d -> %d pcnt range %d -> %d bucket_size %d bss_rssi %d num_slot %d slot_size %d rssi_diff %d slot_index %d rssi_pcnt %d",
		  high_rssi_threshold, low_rssi_threshold, high_rssi_pcnt,
		  low_rssi_pcnt, bucket_size, bss_rssi, num_slot, slot_size,
		  rssi_diff, slot_index, rssi_pcnt);

	return rssi_pcnt;
}

/**
 * scm_calculate_rssi_score () - Calculate RSSI score based on AP RSSI
 * @score_param: rssi score params
 * @rssi: rssi of the AP
 * @rssi_weightage: rssi_weightage out of total weightage
 *
 * Return : rssi score
 */
static int32_t scm_calculate_rssi_score(
	struct rssi_cfg_score *score_param,
	int32_t rssi, uint8_t rssi_weightage)
{
	int8_t rssi_pcnt;
	int32_t total_rssi_score;
	int32_t best_rssi_threshold;
	int32_t good_rssi_threshold;
	int32_t bad_rssi_threshold;
	uint32_t good_rssi_pcnt;
	uint32_t bad_rssi_pcnt;
	uint32_t good_bucket_size;
	uint32_t bad_bucket_size;

	best_rssi_threshold = score_param->best_rssi_threshold*(-1);
	good_rssi_threshold = score_param->good_rssi_threshold*(-1);
	bad_rssi_threshold = score_param->bad_rssi_threshold*(-1);
	good_rssi_pcnt = score_param->good_rssi_pcnt;
	bad_rssi_pcnt = score_param->bad_rssi_pcnt;
	good_bucket_size = score_param->good_rssi_bucket_size;
	bad_bucket_size = score_param->bad_rssi_bucket_size;

	total_rssi_score = (MAX_PCT_SCORE * rssi_weightage);

	/*
	 * If RSSI is better than the best rssi threshold then it return full
	 * score.
	 */
	if (rssi > best_rssi_threshold)
		return total_rssi_score;
	/*
	 * If RSSI is less or equal to bad rssi threshold then it return
	 * least score.
	 */
	if (rssi <= bad_rssi_threshold)
		return (total_rssi_score * bad_rssi_pcnt) / 100;

	/* RSSI lies between best to good rssi threshold */
	if (rssi > good_rssi_threshold)
		rssi_pcnt = scm_get_rssi_pcnt_for_slot(best_rssi_threshold,
				good_rssi_threshold, 100, good_rssi_pcnt,
				good_bucket_size, rssi);
	else
		rssi_pcnt = scm_get_rssi_pcnt_for_slot(good_rssi_threshold,
				bad_rssi_threshold, good_rssi_pcnt,
				bad_rssi_pcnt, bad_bucket_size,
				rssi);

	return (total_rssi_score * rssi_pcnt) / 100;

}

/**
 * scm_calculate_pcl_score () - Calculate PCL score based on PCL weightage
 * @pcl_chan_weight: pcl weight of BSS channel
 * @pcl_weightage: PCL _weightage out of total weightage
 *
 * Return : pcl score
 */
static int32_t scm_calculate_pcl_score(int pcl_chan_weight,
		uint8_t pcl_weightage)
{
	int32_t pcl_score = 0;
	int32_t temp_pcl_chan_weight = 0;

	if (pcl_chan_weight) {
		temp_pcl_chan_weight =
			(SCM_MAX_WEIGHT_OF_PCL_CHANNELS - pcl_chan_weight);
		temp_pcl_chan_weight = qdf_do_div(temp_pcl_chan_weight,
					  SCM_PCL_GROUPS_WEIGHT_DIFFERENCE);
		pcl_score = pcl_weightage - temp_pcl_chan_weight;
		if (pcl_score < 0)
			pcl_score = 0;
	}
	return pcl_score * MAX_PCT_SCORE;

}

/**
 * scm_rssi_is_same_bucket () - check if both rssi fall in same bucket
 * @rssi_top_thresh: high rssi threshold of the the window
 * @low_rssi_threshold: low rssi of the window
 * @rssi_ref1: rssi ref one
 * @rssi_ref2: rssi ref two
 * @bucket_size: bucket size of the window
 *
 * Return : true if both fall in same window
 */
static inline bool scm_rssi_is_same_bucket(int8_t rssi_top_thresh,
	int8_t rssi_ref1, int8_t rssi_ref2, int8_t bucket_size)
{
	int8_t rssi_diff1 = 0;
	int8_t rssi_diff2 = 0;

	rssi_diff1 = rssi_top_thresh - rssi_ref1;
	rssi_diff2 = rssi_top_thresh - rssi_ref2;

	return (rssi_diff1 / bucket_size) == (rssi_diff2 / bucket_size);
}

/**
 * scm_roam_calculate_prorated_pcnt_by_rssi () - Calculate prorated RSSI score
 * based on AP RSSI. This will be used to determine HT VHT score
 * @score_param: rssi score params
 * @rssi: bss rssi
 * @rssi_weightage: rssi_weightage out of total weightage
 *
 * If rssi is greater than good threshold return 100, if less than bad return 0,
 * if between good and bad, return prorated rssi score for the index.
 *
 * Return : rssi prorated score
 */
static int8_t scm_roam_calculate_prorated_pcnt_by_rssi(
	struct rssi_cfg_score *score_param,
	int32_t rssi, uint8_t rssi_weightage)
{
	int32_t good_rssi_threshold;
	int32_t bad_rssi_threshold;
	int8_t rssi_pref_5g_rssi_thresh;
	bool same_bucket;

	good_rssi_threshold = score_param->good_rssi_threshold * (-1);
	bad_rssi_threshold = score_param->bad_rssi_threshold * (-1);
	rssi_pref_5g_rssi_thresh = score_param->rssi_pref_5g_rssi_thresh * (-1);

	/* If RSSI is greater than good rssi return full weight */
	if (rssi > good_rssi_threshold)
		return MAX_PCT_SCORE;

	same_bucket = scm_rssi_is_same_bucket(good_rssi_threshold,
			rssi, rssi_pref_5g_rssi_thresh,
			score_param->bad_rssi_bucket_size);
	if (same_bucket || (rssi < rssi_pref_5g_rssi_thresh))
		return 0;
	/* If RSSI is less or equal to bad rssi threshold then it return 0 */
	if (rssi <= bad_rssi_threshold)
		return 0;

	/* If RSSI is between good and bad threshold */
	return scm_get_rssi_pcnt_for_slot(good_rssi_threshold,
					  bad_rssi_threshold,
					  score_param->good_rssi_pcnt,
					  score_param->bad_rssi_pcnt,
					  score_param->bad_rssi_bucket_size,
					  rssi);
}

/**
 * scm_calculate_bandwidth_score () - Calculate BW score
 * @entry: scan entry
 * @score_config: scoring config
 * @prorated_pct: prorated % to return dependent on RSSI
 *
 * Return : bw score
 */
static int32_t scm_calculate_bandwidth_score(
	struct scan_cache_entry *entry,
	struct scoring_config *score_config, uint8_t prorated_pct)
{
	uint32_t score;
	int32_t bw_weight_per_idx;
	uint8_t cbmode = 0;
	uint8_t ch_width_index;
	bool is_vht = false;

	bw_weight_per_idx = score_config->bandwidth_weight_per_index;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq)) {
		cbmode = score_config->cb_mode_24G;
		if (score_config->vht_24G_cap)
			is_vht = true;
	} else if (score_config->vht_cap) {
		is_vht = true;
		cbmode = score_config->cb_mode_5G;
	}

	if (IS_WLAN_PHYMODE_160MHZ(entry->phy_mode))
		ch_width_index = SCM_160MHZ_BW_INDEX;
	else if (IS_WLAN_PHYMODE_80MHZ(entry->phy_mode))
		ch_width_index = SCM_80MHZ_BW_INDEX;
	else if (IS_WLAN_PHYMODE_40MHZ(entry->phy_mode))
		ch_width_index = SCM_40MHZ_BW_INDEX;
	else
		ch_width_index = SCM_20MHZ_BW_INDEX;


	if (!score_config->ht_cap && ch_width_index > SCM_20MHZ_BW_INDEX)
		ch_width_index = SCM_20MHZ_BW_INDEX;

	if (!is_vht && ch_width_index > SCM_40MHZ_BW_INDEX)
		ch_width_index = SCM_40MHZ_BW_INDEX;

	if (cbmode && ch_width_index > SCM_20MHZ_BW_INDEX)
		score = WLAN_GET_SCORE_PERCENTAGE(bw_weight_per_idx,
					ch_width_index);
	else
		score = WLAN_GET_SCORE_PERCENTAGE(bw_weight_per_idx,
					SCM_20MHZ_BW_INDEX);

	return (prorated_pct * score *
		score_config->weight_cfg.chan_width_weightage) / MAX_PCT_SCORE;
}

/**
 * scm_get_score_for_index () - get score for the given index
 * @index: index for which we need the score
 * @weightage: weigtage for the param
 * @score: per slot score
 *
 * Return : score for the index
 */
static int32_t scm_get_score_for_index(uint8_t index,
	uint8_t weightage, struct per_slot_scoring *score)
{
	if (index <= SCM_SCORE_INDEX_3)
		return weightage * WLAN_GET_SCORE_PERCENTAGE(
				   score->score_pcnt3_to_0,
				   index);
	else if (index <= SCM_SCORE_INDEX_7)
		return weightage * WLAN_GET_SCORE_PERCENTAGE(
				   score->score_pcnt7_to_4,
				   index - SCM_SCORE_OFFSET_INDEX_7_4);
	else if (index <= SCM_SCORE_INDEX_11)
		return weightage * WLAN_GET_SCORE_PERCENTAGE(
				   score->score_pcnt11_to_8,
				   index - SCM_SCORE_OFFSET_INDEX_11_8);
	else
		return weightage * WLAN_GET_SCORE_PERCENTAGE(
				   score->score_pcnt15_to_12,
				   index - SCM_SCORE_OFFSET_INDEX_15_12);
}

/**
 * scm_get_congestion_pct () - Calculate congestion pct from esp/qbss load
 * @entry: bss information
 *
 * Return : congestion pct
 */
static int32_t scm_get_congestion_pct(struct scan_cache_entry *entry)
{
	uint32_t ap_load = 0;
	uint32_t est_air_time_percentage = 0;
	uint32_t congestion = 0;

	if (entry->air_time_fraction) {
		/* Convert 0-255 range to percentage */
		est_air_time_percentage = entry->air_time_fraction *
							SCM_MAX_CHANNEL_WEIGHT;
		est_air_time_percentage = qdf_do_div(est_air_time_percentage,
					   SCM_MAX_ESTIMATED_AIR_TIME_FRACTION);
		/*
		 * Calculate channel congestion from estimated air time
		 * fraction.
		 */
		congestion = SCM_MAX_CHANNEL_UTILIZATION -
					est_air_time_percentage;
	} else if (entry->qbss_chan_load) {
		ap_load = (entry->qbss_chan_load * MAX_PCT_SCORE);
		/*
		 * Calculate ap_load in % from qbss channel load from
		 * 0-255 range
		 */
		congestion = qdf_do_div(ap_load, MAX_AP_LOAD);
	}

	return congestion;
}

/**
 * scm_calculate_congestion_score () - Calculate congestion score
 * @entry: bss information
 * @score_params: bss score params
 * @congestion_pct: congestion pct
 *
 * Return : congestion score
 */
static int32_t scm_calculate_congestion_score(
	struct scan_cache_entry *entry,
	struct scoring_config *score_params,
	uint32_t *congestion_pct)
{
	uint32_t window_size;
	uint8_t index;
	int32_t good_rssi_threshold;

	*congestion_pct = scm_get_congestion_pct(entry);

	if (!score_params->esp_qbss_scoring.num_slot)
		return 0;

	if (score_params->esp_qbss_scoring.num_slot >
	    SCM_SCORE_MAX_INDEX)
		score_params->esp_qbss_scoring.num_slot =
			SCM_SCORE_MAX_INDEX;

	good_rssi_threshold =
		score_params->rssi_score.good_rssi_threshold * (-1);

	/* For bad zone rssi get score from last index */
	if (entry->rssi_raw <= good_rssi_threshold)
		return scm_get_score_for_index(
				score_params->esp_qbss_scoring.num_slot,
				score_params->weight_cfg.
				channel_congestion_weightage,
				&score_params->esp_qbss_scoring);

	if (!*congestion_pct)
		return score_params->weight_cfg.channel_congestion_weightage *
			   WLAN_GET_SCORE_PERCENTAGE(
			   score_params->esp_qbss_scoring.score_pcnt3_to_0,
			   SCM_SCORE_INDEX_0);

	window_size = MAX_PCT_SCORE / score_params->esp_qbss_scoring.num_slot;

	/* Desired values are from 1 to 15, as 0 is for not present. so do +1 */
	index = qdf_do_div(*congestion_pct, window_size) + 1;

	if (index > score_params->esp_qbss_scoring.num_slot)
		index = score_params->esp_qbss_scoring.num_slot;

	return scm_get_score_for_index(index, score_params->weight_cfg.
				       channel_congestion_weightage,
				       &score_params->esp_qbss_scoring);
}

/**
 * scm_calculate_nss_score () - Calculate congestion score
 * @psoc: psoc ptr
 * @score_config: scoring config
 * @ap_nss: ap nss
 * @prorated_pct: prorated % to return dependent on RSSI
 *
 * Return : nss score
 */
static int32_t scm_calculate_nss_score(struct wlan_objmgr_psoc *psoc,
	struct scoring_config *score_config, uint8_t ap_nss,
	uint8_t prorated_pct, uint32_t sta_nss)
{
	uint8_t nss;
	uint8_t score_pct;

	nss = ap_nss;
	if (sta_nss < nss)
		nss = sta_nss;

	if (nss == 4)
		score_pct = WLAN_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index,
				SCM_NSS_4x4_INDEX);
	else if (nss == 3)
		score_pct = WLAN_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index,
				SCM_NSS_3x3_INDEX);
	else if (nss == 2)
		score_pct = WLAN_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index,
				SCM_NSS_2x2_INDEX);
	else
		score_pct = WLAN_GET_SCORE_PERCENTAGE(
				score_config->nss_weight_per_index,
				SCM_NSS_1x1_INDEX);

	return (score_config->weight_cfg.nss_weightage * score_pct *
		prorated_pct) / MAX_PCT_SCORE;
}

/**
 * scm_calculate_oce_wan_score () - Calculate oce wan score
 * @entry: bss information
 * @score_params: bss score params
 *
 * Return : oce wan score
 */
static int32_t scm_calculate_oce_wan_score(
	struct scan_cache_entry *entry,
	struct scoring_config *score_params)
{
	uint32_t window_size;
	uint8_t index;
	struct oce_reduced_wan_metrics wan_metrics;
	uint8_t *mbo_oce_ie;

	if (!score_params->oce_wan_scoring.num_slot)
		return 0;

	if (score_params->oce_wan_scoring.num_slot >
	    SCM_SCORE_MAX_INDEX)
		score_params->oce_wan_scoring.num_slot =
			SCM_SCORE_MAX_INDEX;

	window_size = SCM_SCORE_MAX_INDEX/
			score_params->oce_wan_scoring.num_slot;
	mbo_oce_ie = util_scan_entry_mbo_oce(entry);
	if (wlan_parse_oce_reduced_wan_metrics_ie(mbo_oce_ie,
	    &wan_metrics)) {
		scm_err("downlink_av_cap %d", wan_metrics.downlink_av_cap);
		/* if capacity is 0 return 0 score */
		if (!wan_metrics.downlink_av_cap)
			return 0;
		/* Desired values are from 1 to WLAN_SCORE_MAX_INDEX */
		index = qdf_do_div(wan_metrics.downlink_av_cap,
				   window_size);
	} else {
		index = SCM_SCORE_INDEX_0;
	}

	if (index > score_params->oce_wan_scoring.num_slot)
		index = score_params->oce_wan_scoring.num_slot;

	return scm_get_score_for_index(index,
			score_params->weight_cfg.oce_wan_weightage,
			&score_params->oce_wan_scoring);
}

/**
 * scm_calculate_oce_subnet_id_weightage () - Calculate oce subnet id weightage
 * @entry: bss entry
 * @score_params: bss score params
 *
 * Return : oce subnet id score
 */
static uint32_t
scm_calculate_oce_subnet_id_weightage(struct scan_cache_entry *entry,
				      struct scoring_config *score_params,
				      bool *oce_subnet_id_present)
{
	uint32_t score = 0;
	uint8_t *mbo_oce_ie;

	mbo_oce_ie = util_scan_entry_mbo_oce(entry);
	*oce_subnet_id_present = wlan_parse_oce_subnet_id_ie(mbo_oce_ie);

	if (*oce_subnet_id_present)
		/* Consider 50% weightage of subnet id */
		score  = (score_params->weight_cfg.oce_subnet_id_weightage *
			 (MAX_PCT_SCORE / 2));

	return score;
}

/**
 * scm_calculate_oce_ap_tx_pwr_weightage () - Calculate oce ap tx pwr weightage
 * @entry: bss entry
 * @score_params: bss score params
 * @ap_tx_pwr_dbm: pointer to hold ap tx power
 *
 * Return : oce ap tx power score
 */
static uint32_t
scm_calculate_oce_ap_tx_pwr_weightage(struct scan_cache_entry *entry,
				      struct scoring_config *score_params,
				      int8_t *ap_tx_pwr_dbm)
{
	uint8_t *mbo_oce_ie, ap_tx_pwr_factor;
	struct rssi_cfg_score *rssi_score_param;
	int32_t best_rssi_threshold, good_rssi_threshold, bad_rssi_threshold;
	uint32_t good_rssi_pcnt, bad_rssi_pcnt, good_bucket_size;
	uint32_t score, normalized_ap_tx_pwr, bad_bucket_size;
	bool ap_tx_pwr_cap_present = true;

	mbo_oce_ie = util_scan_entry_mbo_oce(entry);
	if (!wlan_parse_oce_ap_tx_pwr_ie(mbo_oce_ie, ap_tx_pwr_dbm)) {
		ap_tx_pwr_cap_present = false;
		/* If no OCE AP TX pwr, consider Uplink RSSI = Downlink RSSI */
		normalized_ap_tx_pwr = entry->rssi_raw;
	} else {
		/*
		 * Normalized ap_tx_pwr =
		 * Uplink RSSI = (STA TX Power - * (AP TX power - RSSI)) in dBm.
		 * Currently assuming STA Tx Power to be 20dBm, though later it
		 * need to fetched from hal-phy API.
		 */
		normalized_ap_tx_pwr = (20 - (*ap_tx_pwr_dbm - entry->rssi_raw));
	}

	rssi_score_param = &score_params->rssi_score;

	best_rssi_threshold = rssi_score_param->best_rssi_threshold * (-1);
	good_rssi_threshold = rssi_score_param->good_rssi_threshold * (-1);
	bad_rssi_threshold = rssi_score_param->bad_rssi_threshold * (-1);
	good_rssi_pcnt = rssi_score_param->good_rssi_pcnt;
	bad_rssi_pcnt = rssi_score_param->bad_rssi_pcnt;
	good_bucket_size = rssi_score_param->good_rssi_bucket_size;
	bad_bucket_size = rssi_score_param->bad_rssi_bucket_size;

	/* Uplink RSSI is better than best rssi threshold */
	if (normalized_ap_tx_pwr > best_rssi_threshold) {
		ap_tx_pwr_factor = MAX_PCT_SCORE;
	} else if (normalized_ap_tx_pwr <= bad_rssi_threshold) {
		/* Uplink RSSI is less or equal to bad rssi threshold */
		ap_tx_pwr_factor = rssi_score_param->bad_rssi_pcnt;
	} else if (normalized_ap_tx_pwr > good_rssi_threshold) {
		/* Uplink RSSI lies between best to good rssi threshold */
		ap_tx_pwr_factor =
			scm_get_rssi_pcnt_for_slot(best_rssi_threshold,
				good_rssi_threshold, 100, good_rssi_pcnt,
				good_bucket_size, normalized_ap_tx_pwr);
	} else {
		/* Uplink RSSI lies between good to best rssi threshold */
		ap_tx_pwr_factor =
			scm_get_rssi_pcnt_for_slot(good_rssi_threshold,
				bad_rssi_threshold, good_rssi_pcnt,
				bad_rssi_pcnt, bad_bucket_size,
				normalized_ap_tx_pwr);
	}

	score  = score_params->weight_cfg.oce_ap_tx_pwr_weightage *
			ap_tx_pwr_factor;

	return score;
}

#ifdef WLAN_POLICY_MGR_ENABLE

static uint32_t scm_get_sta_nss(struct wlan_objmgr_psoc *psoc,
			qdf_freq_t bss_channel_freq,
			uint8_t vdev_nss_2g,
			uint8_t vdev_nss_5g)
{
	/*
	 * If station support nss as 2*2 but AP support NSS as 1*1,
	 * this AP will be given half weight compare to AP which are having
	 * NSS as 2*2.
	 */

	if (policy_mgr_is_chnl_in_diff_band(
	    psoc, bss_channel_freq) &&
	    policy_mgr_is_hw_dbs_capable(psoc) &&
	    !(policy_mgr_is_hw_dbs_2x2_capable(psoc)))
		return 1;

	return (WLAN_REG_IS_24GHZ_CH_FREQ(bss_channel_freq) ?
		vdev_nss_2g :
		vdev_nss_5g);
}
#else
static uint32_t scm_get_sta_nss(struct wlan_objmgr_psoc *psoc,
			qdf_freq_t bss_channel_freq,
			uint8_t vdev_nss_2g,
			uint8_t vdev_nss_5g)
{
	return (WLAN_REG_IS_24GHZ_CH_FREQ(bss_channel_freq) ?
		vdev_nss_2g :
		vdev_nss_5g);
}
#endif

/**
 * scm_get_band_score() - Get band prefernce weightage
 * freq: Operating frequency of the AP
 * @score_config: Score configuration
 *
 * Return : Band score for AP.
 */
static int
scm_get_band_score(uint32_t freq, struct scoring_config *score_config)
{
	uint8_t band_index;
	struct weight_config *weight_config;

	weight_config = &score_config->weight_cfg;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(freq))
		band_index = SCM_BAND_5G_INDEX;
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(freq))
		band_index = SCM_BAND_2G_INDEX;
	else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(freq))
		band_index = SCM_BAND_6G_INDEX;
	else
		return 0;

	return weight_config->chan_band_weightage *
	       WLAN_GET_SCORE_PERCENTAGE(score_config->band_weight_per_index,
					 band_index);
}

int scm_calculate_bss_score(struct wlan_objmgr_psoc *psoc,
		struct scan_default_params *params,
		struct scan_cache_entry *entry,
		int pcl_chan_weight)
{
	int32_t score = 0;
	int32_t rssi_score = 0;
	int32_t pcl_score = 0;
	int32_t ht_score = 0;
	int32_t vht_score = 0;
	int32_t he_score = 0;
	int32_t bandwidth_score = 0;
	int32_t beamformee_score = 0;
	int32_t band_score = 0;
	int32_t nss_score = 0;
	int32_t congestion_score = 0;
	int32_t congestion_pct = 0;
	int32_t oce_wan_score = 0;
	uint8_t oce_ap_tx_pwr_score = 0;
	uint8_t oce_subnet_id_score = 0;
	bool oce_subnet_id_present = 0;
	uint8_t prorated_pcnt;
	bool is_vht = false;
	int8_t good_rssi_threshold;
	int8_t rssi_pref_5g_rssi_thresh;
	bool same_bucket = false;
	bool ap_su_beam_former = false;
	struct wlan_ie_vhtcaps *vht_cap;
	struct scoring_config *score_config;
	struct weight_config *weight_config;
	struct wlan_scan_obj *scan_obj;
	uint32_t sta_nss;
	struct wlan_objmgr_pdev *pdev = NULL;
	int8_t ap_tx_pwr_dbm = 0;

	scan_obj = wlan_psoc_get_scan_obj(psoc);
	if (!scan_obj) {
		scm_err("scan_obj is NULL");
		return 0;
	}

	score_config = &scan_obj->scan_def.score_config;
	weight_config = &score_config->weight_cfg;

	rssi_score = scm_calculate_rssi_score(&score_config->rssi_score,
			entry->rssi_raw, weight_config->rssi_weightage);
	score += rssi_score;

	pcl_score = scm_calculate_pcl_score(pcl_chan_weight,
					    weight_config->pcl_weightage);
	score += pcl_score;

	prorated_pcnt = scm_roam_calculate_prorated_pcnt_by_rssi(
				&score_config->rssi_score, entry->rssi_raw,
				weight_config->rssi_weightage);

	/*
	 * Add HT weight if HT is supported by the AP. In case
	 * of 6 GHZ AP, HT and VHT won't be supported so that
	 * these weightage to the same by default to match
	 * with 2.4/5 GHZ APs where HT, VHT is supported
	 */
	if (score_config->ht_cap && (entry->ie_list.htcap ||
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(entry->channel.chan_freq)))
		ht_score = prorated_pcnt *
				weight_config->ht_caps_weightage;
	score += ht_score;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq)) {
		if (score_config->vht_24G_cap)
			is_vht = true;
	} else if (score_config->vht_cap) {
		is_vht = true;
	}

	/* Add VHT score to 6 GHZ AP to match with 2.4/5 GHZ APs */
	if (is_vht && (entry->ie_list.vhtcap ||
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(entry->channel.chan_freq)))
		vht_score = prorated_pcnt *
				 weight_config->vht_caps_weightage;
	score += vht_score;

	if (score_config->he_cap && entry->ie_list.hecap)
		he_score = prorated_pcnt *
				 weight_config->he_caps_weightage;
	score += he_score;

	bandwidth_score = scm_calculate_bandwidth_score(entry, score_config,
							prorated_pcnt);
	score += bandwidth_score;

	good_rssi_threshold =
		score_config->rssi_score.good_rssi_threshold * (-1);
	rssi_pref_5g_rssi_thresh =
		score_config->rssi_score.rssi_pref_5g_rssi_thresh * (-1);
	if (entry->rssi_raw < good_rssi_threshold)
		same_bucket = scm_rssi_is_same_bucket(good_rssi_threshold,
				entry->rssi_raw, rssi_pref_5g_rssi_thresh,
				score_config->rssi_score.bad_rssi_bucket_size);

	vht_cap = (struct wlan_ie_vhtcaps *) util_scan_entry_vhtcap(entry);
	if (vht_cap && vht_cap->su_beam_former)
		ap_su_beam_former = true;
	if (is_vht && ap_su_beam_former &&
	    (entry->rssi_raw > rssi_pref_5g_rssi_thresh) && !same_bucket)
		beamformee_score = MAX_PCT_SCORE *
				weight_config->beamforming_cap_weightage;
	score += beamformee_score;

	congestion_score = scm_calculate_congestion_score(entry, score_config,
							  &congestion_pct);
	score += congestion_score;
	/*
	 * Consider OCE WAN score and band preference score only if
	 * congestion_pct is greater than CONGESTION_THRSHOLD_FOR_BAND_OCE_SCORE
	 */
	if (congestion_pct < CONGESTION_THRSHOLD_FOR_BAND_OCE_SCORE) {
		/*
		 * If AP is on 5/6 GHZ channel , extra weigtage is added to BSS
		 * score. if RSSI is greater tha 5g rssi threshold or fall in
		 * same bucket else give weigtage to 2.4 GHZ AP.
		 */
		if ((entry->rssi_raw > rssi_pref_5g_rssi_thresh) &&
		    !same_bucket) {
			if (!WLAN_REG_IS_24GHZ_CH_FREQ(entry->channel.chan_freq))
				band_score = scm_get_band_score(
						entry->channel.chan_freq,
						score_config);
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
			   entry->channel.chan_freq)) {
			band_score =
				scm_get_band_score(entry->channel.chan_freq,
						   score_config);
		}

		score += band_score;

		oce_wan_score = scm_calculate_oce_wan_score(entry,
							    score_config);
		score += oce_wan_score;
	}

	oce_ap_tx_pwr_score =
		scm_calculate_oce_ap_tx_pwr_weightage(entry, score_config,
						      &ap_tx_pwr_dbm);
	score += oce_ap_tx_pwr_score;
	oce_subnet_id_score = scm_calculate_oce_subnet_id_weightage(entry,
					score_config, &oce_subnet_id_present);
	score += oce_subnet_id_score;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, entry->pdev_id, WLAN_SCAN_ID);
	if (!pdev) {
		scm_err("pdev is NULL");
		return 0;
	}

	sta_nss = scm_get_sta_nss(psoc,
				  entry->channel.chan_freq,
				  score_config->vdev_nss_24g,
				  score_config->vdev_nss_5g);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_SCAN_ID);
	/*
	 * If station support nss as 2*2 but AP support NSS as 1*1,
	 * this AP will be given half weight compare to AP which are having
	 * NSS as 2*2.
	 */
	nss_score = scm_calculate_nss_score(psoc, score_config, entry->nss,
					    prorated_pcnt, sta_nss);
	score += nss_score;

	scm_nofl_debug("Self: HT %d VHT %d HE %d VHT_24Ghz %d BF cap %d cb_mode_24g %d cb_mode_5G %d NSS %d",
		       score_config->ht_cap, score_config->vht_cap,
		       score_config->he_cap,  score_config->vht_24G_cap,
		       score_config->beamformee_cap, score_config->cb_mode_24G,
		       score_config->cb_mode_5G, sta_nss);

	scm_nofl_debug("Candidate(%pM freq %d): rssi %d HT %d VHT %d HE %d su bfer %d phy %d  air time frac %d qbss %d cong_pct %d NSS %d ap_tx_pwr_dbm %d oce_subnet_id_present %d",
		       entry->bssid.bytes, entry->channel.chan_freq,
		       entry->rssi_raw, util_scan_entry_htcap(entry) ? 1 : 0,
		       util_scan_entry_vhtcap(entry) ? 1 : 0,
		       util_scan_entry_hecap(entry) ? 1 : 0, ap_su_beam_former,
		       entry->phy_mode, entry->air_time_fraction,
		       entry->qbss_chan_load, congestion_pct, entry->nss,
		       ap_tx_pwr_dbm, oce_subnet_id_present);

	scm_nofl_debug("Scores: prorated_pcnt %d rssi %d pcl %d ht %d vht %d he %d bfee %d bw %d band %d congestion %d nss %d oce wan %d oce ap tx pwr %d subnet id score %d TOTAL %d",
		       prorated_pcnt, rssi_score, pcl_score, ht_score,
		       vht_score, he_score, beamformee_score, bandwidth_score,
		       band_score, congestion_score, nss_score, oce_wan_score,
		       oce_ap_tx_pwr_score, oce_subnet_id_score, score);

	entry->bss_score = score;
	return score;
}

bool scm_get_pcl_weight_of_channel(uint32_t chan_freq,
				   struct scan_filter *filter,
				   int *pcl_chan_weight,
				   uint8_t *weight_list)
{
	int i;
	bool found = false;

	if (!filter)
		return found;

	for (i = 0; i < filter->num_of_pcl_channels; i++) {
		if (filter->pcl_freq_list[i] == chan_freq) {
			*pcl_chan_weight = filter->pcl_weight_list[i];
			found = true;
			break;
		}
	}
	return found;
}
