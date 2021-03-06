/*
 * Copyright (c) 2011-2014,2017-2020 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * copyright (c) 2011 Atheros Communications Inc.
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

/*
 * LMAC VAP specific offload interface functions for UMAC - for power and performance offload model
 */
#include "ol_if_athvar.h"
#include "ol_if_athpriv.h"
#include "wmi_unified_api.h"
#include "ieee80211_api.h"
#include "umac_lmac_common.h"
#include "osif_private.h"
#include "wlan_osif_priv.h"
#include "qdf_mem.h"
#include "target_if.h"
#include "qdf_module.h"
#include "cfg_ucfg_api.h"
#include <qdf_types.h>
#include "wlan_vdev_mgr_ucfg_api.h"
#include "wlan_vdev_mgr_utils_api.h"
#include <wlan_mlme_vdev_mgmt_ops.h>
#include <wlan_utility.h>
#include <init_deinit_ops.h>

#define DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS  (IEEE80211_INACT_RUN * IEEE80211_INACT_WAIT)
#define DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_IDLE_TIME_SECS          (DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS - 5)
#define DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MIN_IDLE_TIME_SECS          (DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_IDLE_TIME_SECS/2)
#if ATH_SUPPORT_WRAP
#include "ol_if_mat.h"
#endif

#if MESH_MODE_SUPPORT
#include <if_meta_hdr.h>
#endif

#include <cdp_txrx_cmn.h>
#include <cdp_txrx_ctrl.h>
#include <cdp_txrx_wds.h>
#include <dp_txrx.h>
#include <ol_txrx_api_internal.h>
#include <ol_if_ath_api.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_api.h>
//PRAVEEN: check two lines below
#include <wlan_tgt_def_config.h>
#include <init_deinit_lmac.h>

#include <wlan_son_pub.h>

#include "target_type.h"
#ifdef WLAN_SUPPORT_FILS
#include <target_if_fd.h>
#endif

#ifdef PORT_SPIRENT_HK
#include "spirent.h"
#endif
#define RC_2_RATE_IDX(_rc)        ((_rc) & 0x7)
#ifndef HT_RC_2_STREAMS
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#endif

#define ONEMBPS 1000

/*
 * WMI_ADD_CIPHER_KEY_CMDID
 */
typedef enum {
    PAIRWISE_USAGE      = 0x00,
    GROUP_USAGE         = 0x01,
    TX_USAGE            = 0x02,     /* default Tx Key - Static WEP only */
} KEY_USAGE;
#if defined(PORT_SPIRENT_HK) && defined(SPT_MULTI_CLIENTS)
int main_proxy_flag0;
int main_proxy_flag1;
int main_proxy_flag2;
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
enum {
    NSS_CIPHER_UNICAST   = 0x00,
    NSS_CIPHER_MULTICAST = 0x01,
};

#endif
#define  TX_MIC_LENGTH    8
#define  RX_MIC_LENGTH    8

#define  WMI_CIPHER_NONE     0x0  /* clear key */
#define  WMI_CIPHER_WEP      0x1
#define  WMI_CIPHER_TKIP     0x2
#define  WMI_CIPHER_AES_OCB  0x3
#define  WMI_CIPHER_AES_CCM  0x4
#define  WMI_CIPHER_WAPI     0x5
#define  WMI_CIPHER_CKIP     0x6
#define  WMI_CIPHER_AES_CMAC 0x7
#define  WMI_CIPHER_ANY      0x8
#define  WMI_CIPHER_AES_GCM  0x9
#define  WMI_CIPHER_AES_GMAC 0xa

/* Enable value for bcast probe
 * response in FILS enable WMI
 */
#ifdef WLAN_SUPPORT_FILS
#define WMI_FILS_FLAGS_BITMAP_BCAST_PRB_RSP 0x1
#endif /* WLAN_SUPPORT_FILS */

extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int ol_num_global_soc;

extern int ol_ath_set_vap_dscp_tid_map(struct ieee80211vap *vap);
extern int ol_ath_set_pdev_dscp_tid_map(struct ieee80211vap *vap, uint32_t val);
extern int ol_ath_ucfg_get_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t aid);
static int wlan_get_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t aid)
{
    return ol_ath_ucfg_get_peer_mumimo_tx_count(vaphandle, aid);
}
extern int ol_ath_ucfg_get_user_postion(wlan_if_t vaphandle, u_int32_t aid);
static int wlan_get_user_postion(wlan_if_t vaphandle, u_int32_t aid)
{
    return ol_ath_ucfg_get_user_postion(vaphandle, aid);
}
extern int ieee80211_rate_is_valid_basic(struct ieee80211vap *, u_int32_t);

extern void ieee80211_vi_dbg_print_stats(struct ieee80211vap *vap);
extern int ol_ath_ucfg_reset_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t aid);
extern int ol_ath_net80211_get_vap_stats(struct ieee80211vap *vap);
extern void ol_ath_set_vap_cts2self_prot_dtim_bcn(struct ieee80211vap *vap);
#if MESH_MODE_SUPPORT
extern void ol_txrx_set_mesh_mode(struct cdp_vdev *vdev, u_int32_t val);
#endif
extern int ol_ath_target_start(ol_ath_soc_softc_t *soc);
extern void ol_wlan_txpow_mgmt(struct ieee80211vap *vap,u_int8_t val);
extern int config_txchainmask(struct ieee80211com *ic,
                              struct ieee80211_ath_channel *chan);
extern int config_rxchainmask(struct ieee80211com *ic,
                              struct ieee80211_ath_channel *chan);
#if ATH_PERF_PWR_OFFLOAD
extern void osif_vap_setup_ol (struct ieee80211vap *vap, osif_dev *osifp);

#if defined(PORT_SPIRENT_HK) && defined(SPT_ADV_STATS)
int ol_ath_calculate_rate(ol_txrx_soc_handle soc_txrx_handle, uint8_t vdev_id, uint8_t direction);
#endif

/*
+legacy rate table for the MCAST/BCAST rate. This table is specific to peregrine
+chip, so its implemented here in the ol layer instead of the ieee layer.

+This table is created according to the discription mentioned in the
+wmi_unified.h file.

+Here the left hand side specify the rate and the right hand side specify the
+respective values which the target understands.
+*/

static const int legacy_11b_rate_ol[][2] = {
    {1000, 0x083},
    {2000, 0x082},
    {5500, 0x081},
    {11000, 0x080},
};

static const int legacy_11a_rate_ol[][2] = {
    {6000, 0x003},
    {9000, 0x007},
    {12000, 0x002},
    {18000, 0x006},
    {24000, 0x001},
    {36000, 0x005},
    {48000, 0x000},
    {54000, 0x004},
};

static const int legacy_11bg_rate_ol[][2] = {
    {1000, 0x083},
    {2000, 0x082},
    {5500, 0x081},
    {6000, 0x003},
    {9000, 0x007},
    {11000, 0x080},
    {12000, 0x002},
    {18000, 0x006},
    {24000, 0x001},
    {36000, 0x005},
    {48000, 0x000},
    {54000, 0x004},
};

static const int ht20_11n_rate_ol[][2] = {
    {6500,  0x100},
    {13000, 0x101},
    {19500, 0x102},
    {26000, 0x103},
    {39000, 0x104},
    {52000, 0x105},
    {58500, 0x106},
    {65000, 0x107},

    {13000,  0x110},
    {26000,  0x111},
    {39000,  0x112},
    {52000,  0x113},
    {78000,  0x114},
    {104000, 0x115},
    {117000, 0x116},
    {130000, 0x117},

    {19500,  0x120},
    {39000,  0x121},
    {58500,  0x122},
    {78000,  0x123},
    {117000, 0x124},
    {156000, 0x125},
    {175500, 0x126},
    {195000, 0x127},

    {26000,  0x130},
    {52000,  0x131},
    {78000,  0x132},
    {104000, 0x133},
    {156000, 0x134},
    {208000, 0x135},
    {234000, 0x136},
    {260000, 0x137},
};

static const int ht20_11ac_rate_ol[][2] = {
/* VHT MCS0-9 NSS 1 20 MHz */
    { 6500, 0x180},
    {13000, 0x181},
    {19500, 0x182},
    {26000, 0x183},
    {39000, 0x184},
    {52000, 0x185},
    {58500, 0x186},
    {65000, 0x187},
    {78000, 0x188},
    {86500, 0x189},

/* VHT MCS0-9 NSS 2 20 MHz */
    { 13000, 0x190},
    { 26000, 0x191},
    { 39000, 0x192},
    { 52000, 0x193},
    { 78000, 0x194},
    {104000, 0x195},
    {117000, 0x196},
    {130000, 0x197},
    {156000, 0x198},
    {173000, 0x199},

 /* VHT MCS0-9 NSS 3 20 MHz */
    { 19500, 0x1a0},
    { 39000, 0x1a1},
    { 58500, 0x1a2},
    { 78000, 0x1a3},
    {117000, 0x1a4},
    {156000, 0x1a5},
    {175500, 0x1a6},
    {195000, 0x1a7},
    {234000, 0x1a8},
    {260000, 0x1a9},

 /* VHT MCS0-9 NSS 4 20 MHz */
    { 26000, 0x1b0},
    { 52000, 0x1b1},
    { 78000, 0x1b2},
    {104000, 0x1b3},
    {156000, 0x1b4},
    {208000, 0x1b5},
    {234000, 0x1b6},
    {260000, 0x1b7},
    {312000, 0x1b8},
    {344000, 0x1b9},
};

static const int ht20_11ax_rate_ol[][2] = {
/* HE MCS0-11 NSS 1 20 MHz */
      {8600, 0x200},
     {17200, 0x201},
     {25800, 0x202},
     {34400, 0x203},
     {51600, 0x204},
     {68800, 0x205},
     {77400, 0x206},
     {86000, 0x207},
    {103200, 0x208},
    {114700, 0x209},
    {129000, 0x20a},
    {143400, 0x20b},

/* HE MCS0-11 NSS 2 20 MHz */
     {17200, 0x210},
     {34400, 0x211},
     {51600, 0x212},
     {68800, 0x213},
    {103200, 0x214},
    {137600, 0x215},
    {154900, 0x216},
    {172100, 0x217},
    {206500, 0x218},
    {229400, 0x219},
    {258100, 0x21a},
    {286800, 0x21b},

/* HE MCS0-11 NSS 3 20 MHz */
     {25800, 0x220},
     {51600, 0x221},
     {77400, 0x222},
    {103200, 0x223},
    {154900, 0x224},
    {206500, 0x225},
    {232300, 0x226},
    {258100, 0x227},
    {309700, 0x228},
    {344100, 0x229},
    {387100, 0x22a},
    {430100, 0x22b},

/* HE MCS0-11 NSS 4 20 MHz */
     {34400, 0x230},
     {68800, 0x231},
    {103200, 0x232},
    {137600, 0x233},
    {206500, 0x234},
    {275300, 0x235},
    {309700, 0x236},
    {344100, 0x237},
    {412900, 0x238},
    {458800, 0x239},
    {516300, 0x23a},
    {573500, 0x23b},

/* HE MCS0-11 NSS 5 20 MHz */
     {43000, 0x240},
     {86000, 0x241},
    {129000, 0x242},
    {172100, 0x243},
    {258100, 0x244},
    {344100, 0x245},
    {387100, 0x246},
    {430100, 0x247},
    {516200, 0x248},
    {573500, 0x249},
    {645200, 0x24a},
    {716900, 0x24b},

/* HE MCS0-11 NSS 6 20 MHz */
     {51600, 0x250},
    {103200, 0x251},
    {154900, 0x252},
    {206500, 0x253},
    {309700, 0x254},
    {412900, 0x255},
    {464600, 0x256},
    {516200, 0x257},
    {619400, 0x258},
    {688200, 0x259},
    {774300, 0x25a},
    {860300, 0x25b},

/* HE MCS0-11 NSS 7 20 MHz */
     {60200, 0x260},
    {120400, 0x261},
    {180700, 0x262},
    {240900, 0x263},
    {361300, 0x264},
    {481800, 0x265},
    {542000, 0x266},
    {602200, 0x267},
    {722600, 0x268},
    {802900, 0x269},
    {903300, 0x26a},
   {1003700, 0x26b},

/* HE MCS0-11 NSS 8 20 MHz */
     {68800, 0x270},
    {137600, 0x271},
    {206500, 0x272},
    {275300, 0x273},
    {412900, 0x274},
    {550600, 0x275},
    {619400, 0x276},
    {688200, 0x277},
    {825900, 0x278},
    {917600, 0x279},
   {1032400, 0x27a},
   {1147100, 0x27b},
};


static inline void ol_ath_populate_bsscolor_in_vdev_param_heop(
        struct ieee80211com *ic,
        uint32_t *heop) {
#if SUPPORT_11AX_D3
            if (ic->ic_he_bsscolor_override) {
                *heop |= (((ic->ic_he_bsscolor <<
                         IEEE80211_HEOP_BSS_COLOR_S) &
                         IEEE80211_HEOP_BSS_COLOR_MASK) << HEOP_PARAM_S);
            } else {
                *heop |= (((ic->ic_bsscolor_hdl.selected_bsscolor <<
                       IEEE80211_HEOP_BSS_COLOR_S) &
                       IEEE80211_HEOP_BSS_COLOR_MASK) << HEOP_PARAM_S);
            }

            *heop |= (ic->ic_he.heop_bsscolor_info &
                    ~(IEEE80211_HEOP_BSS_COLOR_MASK
                        << IEEE80211_HEOP_BSS_COLOR_S)) << HEOP_PARAM_S;
#else
            if (ic->ic_he_bsscolor_override) {
                *heop |= ((ic->ic_he_bsscolor <<
                        IEEE80211_HEOP_BSS_COLOR_S) &
                        IEEE80211_HEOP_BSS_COLOR_MASK);
            } else {
                *heop |=((ic->ic_bsscolor_hdl.selected_bsscolor <<
                       IEEE80211_HEOP_BSS_COLOR_S) &
                       IEEE80211_HEOP_BSS_COLOR_MASK);;
            }
#endif
}

#if QCA_AIRTIME_FAIRNESS
int
ol_ath_vdev_atf_request_send(struct ol_ath_softc_net80211 *scn, u_int32_t param_value)
{
    /* Will be implemented when it is decided how to use it*/
    /* While implementing, please Move this API to WMI layer. */
    return EOK;
}
#endif

#if ATH_SUPPORT_NAC_RSSI
int
ol_ath_config_fw_for_nac_rssi(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
                uint8_t vdev_id, enum cdp_nac_param_cmd cmd, char *bssid, char *client_macaddr,
                uint8_t chan_num)
{
    struct vdev_scan_nac_rssi_params param;
    wmi_unified_t pdev_wmi_handle;
    QDF_STATUS status;
    struct wlan_objmgr_pdev *pdev =
           wlan_objmgr_get_pdev_by_id((struct wlan_objmgr_psoc *)psoc,
                                       pdev_id, WLAN_VDEV_TARGET_IF_ID);

    if (!pdev) {
        qdf_err("pdev is NULL\n");
        return QDF_STATUS_E_FAILURE;
    }

    qdf_mem_zero(&param, sizeof(param));
    param.vdev_id = vdev_id;

    pdev_wmi_handle = lmac_get_pdev_wmi_unified_handle(pdev);
    if (!pdev_wmi_handle) {
        qdf_err("WMI handle is NULL\n");
        status = QDF_STATUS_E_FAILURE;
    } else if (CDP_NAC_PARAM_LIST == cmd) {
        struct stats_request_params list_param = {0};
        u_int8_t macaddr[QDF_MAC_ADDR_SIZE] = {0};

        list_param.vdev_id = vdev_id;
        list_param.stats_id = WMI_HOST_REQUEST_NAC_RSSI;
        status =  wmi_unified_stats_request_send(pdev_wmi_handle, macaddr, &list_param);
    } else {
        param.action = cmd;
        param.chan_num = chan_num;

        qdf_mem_copy(&param.bssid_addr, bssid,QDF_MAC_ADDR_SIZE);
        qdf_mem_copy(&param.client_addr, client_macaddr,QDF_MAC_ADDR_SIZE);
        status = wmi_unified_vdev_set_nac_rssi_send(pdev_wmi_handle, &param);
    }

    wlan_objmgr_pdev_release_ref(pdev, WLAN_VDEV_TARGET_IF_ID);

    return status;
}

int ol_ath_config_bssid_in_fw_for_nac_rssi(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
                                           uint8_t vdev_id, enum cdp_nac_param_cmd cmd,
                                           char *bssid, char *client_macaddr)
{
    void *pdev_wmi_handle;
    struct set_neighbour_rx_params param;
    struct ieee80211com *ic = NULL;
    QDF_STATUS status = QDF_STATUS_E_FAILURE;
    struct wlan_objmgr_pdev *pdev =
            wlan_objmgr_get_pdev_by_id((struct wlan_objmgr_psoc *)psoc,
                                        pdev_id, WLAN_VDEV_TARGET_IF_ID);

    if (!pdev) {
        qdf_err("pdev is NULL\n");
        return status;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
        goto end;

    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = vdev_id;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
    if (!pdev_wmi_handle) {
        qdf_err("WMI handle is NULL\n");
        status = QDF_STATUS_E_FAILURE;
        goto end;
    }

    param.idx = 1;
    param.action = cmd;
    if (ic->ic_hw_nac_monitor_support) {
        param.type = IEEE80211_NAC_MACTYPE_CLIENT;
        status = wmi_unified_vdev_set_neighbour_rx_cmd_send(pdev_wmi_handle, client_macaddr, &param);
    } else {
        param.type = IEEE80211_NAC_MACTYPE_BSSID;
        status = wmi_unified_vdev_set_neighbour_rx_cmd_send(pdev_wmi_handle, bssid, &param);
    }

end:
    wlan_objmgr_pdev_release_ref(pdev, WLAN_VDEV_TARGET_IF_ID);

    return status;
}

#endif


#define NUM_RATE_TABS 4
int ol_get_rate_code(struct ieee80211_ath_channel *chan, int val)
{
    uint32_t chan_mode;
    int i = 0, j = 0, found = 0, array_size = 0;
    int *rate_code = NULL;

    struct ol_rate_table {
        int *table;
        int size;
    } rate_table [NUM_RATE_TABS];

    if(chan == NULL)
        return EINVAL;

    OS_MEMZERO(&rate_table[0], sizeof(rate_table));

    chan_mode = ieee80211_chan2mode(chan);

    switch (chan_mode)
    {
        case IEEE80211_MODE_11B:
            {
                /* convert rate to index */
                rate_table[3].size = sizeof(legacy_11b_rate_ol)/sizeof(legacy_11b_rate_ol[0]);
                rate_table[3].table = (int *)&legacy_11b_rate_ol;
            }
            break;

        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_TURBO_G:
            {
                /* convert rate to index */
                rate_table[3].size = sizeof(legacy_11bg_rate_ol)/sizeof(legacy_11bg_rate_ol[0]);
                rate_table[3].table = (int *)&legacy_11bg_rate_ol;
            }
            break;

        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_TURBO_A:
            {
                /* convert rate to index */
                rate_table[3].size = sizeof(legacy_11a_rate_ol)/sizeof(legacy_11a_rate_ol[0]);
                rate_table[3].table = (int *)&legacy_11a_rate_ol;
            }
            break;

        case IEEE80211_MODE_11AXG_HE20      :
        case IEEE80211_MODE_11AXA_HE40PLUS  :
        case IEEE80211_MODE_11AXA_HE20      :
        case IEEE80211_MODE_11AXA_HE40MINUS :
        case IEEE80211_MODE_11AXG_HE40PLUS  :
        case IEEE80211_MODE_11AXG_HE40MINUS :
        case IEEE80211_MODE_11AXA_HE40      :
        case IEEE80211_MODE_11AXG_HE40      :
        case IEEE80211_MODE_11AXA_HE80      :
        case IEEE80211_MODE_11AXA_HE160     :
        case IEEE80211_MODE_11AXA_HE80_80   :
            {
                if (IEEE80211_IS_CHAN_6GHZ(chan)) {
                    rate_table[2].size = sizeof(ht20_11ax_rate_ol)/sizeof(ht20_11ax_rate_ol[0]);
                    rate_table[2].table = (int *)&ht20_11ax_rate_ol;
                } else {
                    rate_table[0].size = sizeof(ht20_11ax_rate_ol)/sizeof(ht20_11ax_rate_ol[0]);
                    rate_table[0].table = (int *)&ht20_11ax_rate_ol;
                }
            }

        case IEEE80211_MODE_11AC_VHT20:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AC_VHT80_80:
            {
                if (!IEEE80211_IS_CHAN_6GHZ(chan)) {
                    rate_table[1].size = sizeof(ht20_11ac_rate_ol)/sizeof(ht20_11ac_rate_ol[0]);
                    rate_table[1].table = (int *)&ht20_11ac_rate_ol;
                }
            }

        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NG_HT40:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
            {
                if (!IEEE80211_IS_CHAN_6GHZ(chan)) {
                    rate_table[2].size = sizeof(ht20_11n_rate_ol)/sizeof(ht20_11n_rate_ol[0]);
                    rate_table[2].table = (int *)&ht20_11n_rate_ol;
                }
            }

            if (IEEE80211_IS_CHAN_5GHZ(chan)) {
                rate_table[3].size = sizeof(legacy_11a_rate_ol)/sizeof(legacy_11a_rate_ol[0]);
                rate_table[3].table = (int *)&legacy_11a_rate_ol;
            } else {
                rate_table[3].size = sizeof(legacy_11bg_rate_ol)/sizeof(legacy_11bg_rate_ol[0]);
                rate_table[3].table = (int *)&legacy_11bg_rate_ol;
            }

            break;

        default:
        {
            qdf_print("%s Invalid channel mode. 0x%x\n\r",__func__, chan_mode);
            break;
        }
    }

    for (j = NUM_RATE_TABS - 1; ((j >= 0) && !found) && rate_table[j].table; j--) {
        array_size = rate_table[j].size;
        rate_code = rate_table[j].table;
        for (i = 0; i < array_size; i++) {
            /* Array Index 0 has the rate and 1 has the rate code.
               The variable rate has the rate code which must be converted to actual rate*/
            if (val == *rate_code) {
                val = *(rate_code + 1);
                found = 1;
                break;
            }
            rate_code += 2;
        }
    }

    if(!found) {
        return EINVAL;
    }
    return val;
}

static int
ol_ath_validate_tx_encap_type(struct ol_ath_softc_net80211 *scn,
        struct ieee80211vap *vap, u_int32_t val)
{
    struct ieee80211com *ic = vap->iv_ic;
    if (!ic->ic_rawmode_support)
    {
        qdf_print("Configuration capability not provided for this chipset");
        return 0;
    }

#ifndef PORT_SPIRENT_HK
    if (wlan_vap_get_opmode(vap) != IEEE80211_M_HOSTAP)
    {
        qdf_print("Configuration capability available only for AP mode");
        return 0;
    }
#endif

#if !QCA_OL_SUPPORT_RAWMODE_TXRX
    if (val == 0) {
        qdf_print("Valid values: 1 - Native Wi-Fi, 2 - Ethernet\n"
               "0 - RAW is unavailable");
        return 0;
    }
#endif

    if (val <= 2) {
        return 1;
    } else {
        qdf_print("Valid values: 0 - RAW, 1 - Native Wi-Fi, 2 - Ethernet, "
               "%d is invalid", val);
        return 0;
    }
}

static int
ol_ath_validate_rx_decap_type(struct ol_ath_softc_net80211 *scn,
        struct ieee80211vap *vap, u_int32_t val)
{
    /* Though the body of this function is the same as
     * ol_ath_validate_tx_encap_type(), it is kept separate for future
     * flexibility.
     */

    struct ieee80211com *ic = vap->iv_ic;
    if (!ic->ic_rawmode_support)
    {
        qdf_print("Configuration capability not provided for this chipset");
        return 0;
    }

    if (wlan_vap_get_opmode(vap) != IEEE80211_M_HOSTAP)
    {
        qdf_print("Configuration capability available only for AP mode");
        return 0;
    }

#if !QCA_OL_SUPPORT_RAWMODE_TXRX
    if (val == 0) {
        qdf_print("Valid values: 1 - Native Wi-Fi, 2 - Ethernet\n"
               "0 - RAW is unavailable");
        return 0;
    }
#endif

    if (val <= 2) {
        return 1;
    } else {
        qdf_print("Valid values: 0 - RAW, 1 - Native Wi-Fi, 2 - Ethernet, "
               "%d is invalid", val);
        return 0;
    }
}

static void ol_ath_vap_iter_sta_wds_disable(void *arg, wlan_if_t vap)
{

    struct ieee80211com *ic = vap->iv_ic;
    ol_txrx_soc_handle soc_txrx_handle;
    struct wlan_objmgr_psoc *psoc;
    uint8_t vdev_id = wlan_vdev_get_id(vap->vdev_obj);

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

    if ((ieee80211vap_get_opmode(vap) == IEEE80211_M_STA)) {
        cdp_config_param_type val = {0};
        cdp_txrx_set_vdev_param(soc_txrx_handle,
           vdev_id, CDP_ENABLE_WDS, val);
        cdp_txrx_set_vdev_param(soc_txrx_handle,
           vdev_id, CDP_ENABLE_MEC, val);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (ic->nss_vops) {
            ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp, OSIF_NSS_VDEV_AST_OVERRIDE_CFG);
        }
#endif
    }
}


int ol_ath_wmi_send_sifs_trigger(struct ol_ath_softc_net80211 *scn,
           uint8_t if_id, uint32_t param_value)
{
    struct sifs_trigger_param sparam;
    struct wmi_unified *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!pdev_wmi_handle)
        return -1;

    qdf_mem_set(&sparam, sizeof(sparam), 0);
    sparam.vdev_id = if_id;
    sparam.param_value = param_value;

    return wmi_unified_sifs_trigger_send(pdev_wmi_handle, &sparam);
}

int ol_ath_wmi_send_vdev_param(struct ol_ath_softc_net80211 *scn, uint8_t if_id,
        uint32_t param_id, uint32_t param_value)
{
    struct vdev_set_params vparam;
    struct wmi_unified *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&vparam, sizeof(vparam), 0);
    vparam.vdev_id = if_id;
    vparam.param_id = param_id;
    vparam.param_value = param_value;
    return wmi_unified_vdev_set_param_send(pdev_wmi_handle, &vparam);
}

#if OBSS_PD
int
ol_ath_send_cfg_obss_spatial_reuse_param(struct ol_ath_softc_net80211 *scn,
					 uint8_t if_id)
{
    struct wmi_unified *pdev_wmi_handle;
    struct wmi_host_obss_spatial_reuse_set_param obss_cmd;
    struct wlan_objmgr_psoc *psoc = scn->soc->psoc_obj;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!pdev_wmi_handle)
	    return -1;

    /* send obss_pd command */
    obss_cmd.enable = true;
    obss_cmd.obss_min =
            cfg_get(psoc, CFG_OL_SRP_SRG_OBSS_PD_MIN_OFFSET);
    obss_cmd.obss_max =
            cfg_get(psoc, CFG_OL_SRP_SRG_OBSS_PD_MAX_OFFSET);
    obss_cmd.vdev_id = if_id;

    return wmi_unified_send_obss_spatial_reuse_set_cmd(pdev_wmi_handle,
							&obss_cmd);
}

int
ol_ath_send_derived_obsee_spatial_reuse_param(struct ieee80211com *ic,
						struct ieee80211vap *vap)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct wmi_host_obss_spatial_reuse_set_param obss_cmd;
    struct ieee80211_node *ni = vap->iv_bss;
    struct ieee80211_spatial_reuse_handle *ni_srp = &ni->ni_srp;
    struct wmi_unified *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!pdev_wmi_handle)
	    return -1;

    obss_cmd.enable = true;
    obss_cmd.obss_min =
            ni_srp->obss_min;
    obss_cmd.obss_max =
	    ni_srp->obss_max;
    obss_cmd.vdev_id = avn->av_if_id;

    return wmi_unified_send_obss_spatial_reuse_set_cmd(pdev_wmi_handle,
							&obss_cmd);
}

bool ol_ath_is_spatial_reuse_enabled(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wmi_unified *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    if (!wmi_handle)
	    return false;

    if (wmi_service_enabled(wmi_handle, wmi_service_obss_spatial_reuse) &&
		   (IEEE80211_IS_CHAN_11AX(ic->ic_curchan)))
        return true;
    else
        return false;
}
#endif
static int
ol_ath_vap_sifs_trigger(struct ieee80211vap *vap, u_int32_t val)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct wlan_objmgr_psoc *psoc;
    int retval = 0;

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    if (!(ol_target_lithium(psoc))) {
        vap->iv_sifs_trigger_time = val;
        retval = ol_ath_wmi_send_sifs_trigger(scn, avn->av_if_id, val);
    } else
        return -EPERM;

    return retval;
}

#ifdef WLAN_SUPPORT_FILS
int ol_ath_wmi_send_vdev_bcast_prbrsp_param (struct ol_ath_softc_net80211 *scn,
                uint8_t if_id,
                uint32_t param_value)
{
    struct config_fils_params param;
    struct wmi_unified *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = if_id;
    param.fd_period = param_value;
    if (param_value) {
        param.send_prb_rsp_frame = WMI_FILS_FLAGS_BITMAP_BCAST_PRB_RSP;
    } else {
        param.send_prb_rsp_frame = 0;
    }
    return wmi_unified_fils_vdev_config_send_cmd(pdev_wmi_handle, &param);

}

static QDF_STATUS ol_ath_fd_tmpl_update(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    QDF_STATUS ret = 0;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    qdf_spin_lock_bh(&avn->avn_lock);
    ret = target_if_fd_tmpl_update(vdev);
	qdf_spin_unlock_bh(&avn->avn_lock);

    if(ret < 0) {
        qdf_err("FD template update failed");
    }

    return ret;
}
#endif /* WLAN_SUPPORT_FILS */

/* Assemble rate code in lithium format from legacy rate code */
static inline uint32_t asemble_ratecode_lithium(uint32_t rate_code)
{
    uint8_t preamble, nss, rix;

    rix = rate_code & RATECODE_V1_RIX_MASK;
    nss = (rate_code >> RATECODE_V1_NSS_OFFSET) &
                                RATECODE_V1_NSS_MASK;
    preamble = rate_code >> RATECODE_V1_PREAMBLE_OFFSET;

    return ASSEMBLE_RATECODE_V1(rix, nss, preamble);
}

/* Assemble rate code in legacy format */
static inline uint32_t assemble_ratecode_legacy(uint32_t rate_code)
{
    uint8_t preamble, nss, rix;

    rix = rate_code & RATECODE_LEGACY_RIX_MASK;
    nss = (rate_code >> RATECODE_LEGACY_NSS_OFFSET) &
                                RATECODE_LEGACY_NSS_MASK;
    preamble = rate_code >> RATECODE_LEGACY_PREAMBLE_OFFSET;

    return ASSEMBLE_RATECODE_LEGACY(rix, nss, preamble);
}

uint32_t ol_ath_assemble_ratecode(struct ieee80211vap *vap,
                                  struct ieee80211_ath_channel *cur_chan,
                                  uint32_t rate)
{
    struct wlan_objmgr_psoc *psoc;
    int value;
    psoc = wlan_vdev_get_psoc(vap->vdev_obj);

    if (!cur_chan)
        return 0;

    value = ol_get_rate_code(cur_chan, rate);
    if (value == EINVAL)
       return  0;

    if (ol_target_lithium(psoc))
        return asemble_ratecode_lithium(value);
    else
        return assemble_ratecode_legacy(value);
}

/**
 * ol_ath_vap_set_qdepth_thresh: Send MSDUQ depth threshold values
 * to the firmware through the WMI interface
 *
 * @ic: Pointer to the ic
 * @vap: Pointer to the vap
 * @mac_addr: Pointer to the MAC address array
 * @tid: TID number
 * @update_mask: amsduq update mask
 * @thresh_val: qdepth threshold value
 */

static int
ol_ath_vap_set_qdepth_thresh(struct ieee80211com *ic, struct ieee80211vap *vap,
                        uint8_t *mac_addr, uint32_t tid, uint32_t update_mask,
                                                          uint32_t thresh_val)
{
    struct set_qdepth_thresh_params param = {0};
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct wmi_unified *pdev_wmi_handle;


    if (scn == NULL) {
        qdf_print("%s: scn is NULL", __func__);
        return -EINVAL;
    }

    if (avn == NULL) {
        qdf_print("%s: avn is NULL", __func__);
        return -EINVAL;
    }

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    param.pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
    param.vdev_id  = avn->av_if_id;
    qdf_mem_copy(param.mac_addr, mac_addr, QDF_MAC_ADDR_SIZE);
    param.update_params[0].tid_num = tid;
    param.update_params[0].msduq_update_mask = update_mask;
    param.update_params[0].qdepth_thresh_value = thresh_val;
    /* Sending in one update */
    param.num_of_msduq_updates = 1;

    return wmi_unified_vdev_set_qdepth_thresh_cmd_send(pdev_wmi_handle,
                                                       &param);
}
/* Vap interface functions */
static int
ol_ath_vap_set_param(struct ieee80211vap *vap,
              ieee80211_param param, u_int32_t val)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    ol_txrx_soc_handle soc_txrx_handle;
    struct wlan_objmgr_psoc *psoc;
    int retval = 0;
    struct wlan_vdev_mgr_cfg mlme_cfg;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;
    uint8_t vdev_id = wlan_vdev_get_id(vap->vdev_obj);
    uint8_t pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);
#if UMAC_VOW_DEBUG
    int ii;
#endif
    uint32_t iv_nss;
    uint8_t sniffer_mode = 0;
    cdp_config_param_type value = {0};

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    vdev_id = wlan_vdev_get_id(vap->vdev_obj);

    /* Set the VAP param in the target */
    switch (param) {

        case IEEE80211_ATIM_WINDOW:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                         wmi_vdev_param_atim_window, val);
        break;

        case IEEE80211_BMISS_COUNT_RESET:
         /* this is mainly under assumsion that if this number of  */
         /* beacons are not received then HW is hung anf HW need to be resett */
         /* target will use its own method to detect and reset the chip if required. */
            retval = 0;
        break;

        case IEEE80211_BMISS_COUNT_MAX:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                         wmi_vdev_param_bmiss_count_max, val);
        break;

        case IEEE80211_FEATURE_WMM:
        retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                wmi_vdev_param_feature_wmm, val);
        break;

        case IEEE80211_FEATURE_WDS:
        retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                wmi_vdev_param_wds, val);

        if (retval == EOK) {
             /* For AP mode, keep WDS always enabled */
            if ((ieee80211vap_get_opmode(vap) != IEEE80211_M_HOSTAP)) {
                cdp_config_param_type value = {0};

                value.cdp_vdev_param_wds = val;
                cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id, CDP_ENABLE_WDS,
                        value);
                value.cdp_vdev_param_mec = val;
                cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id, CDP_ENABLE_MEC,
                        value);
/* DA_WAR is enabled by default within DP in AP mode, for Hawkeye v1.x */

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_vops) {
                    ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp, OSIF_NSS_VDEV_WDS_CFG);
                }
#endif
            }
        }
        break;

        case IEEE80211_CHWIDTH:
            retval = ol_ath_wmi_send_vdev_param(
                    scn, avn->av_if_id,
                    wmi_vdev_param_chwidth, val);
            break;

        case IEEE80211_SIFS_TRIGGER_RATE:
             if (!(ol_target_lithium(psoc))) {
                 vap->iv_sifs_trigger_rate = val;
                 ol_ath_wmi_send_vdev_param( scn, avn->av_if_id,
                             wmi_vdev_param_sifs_trigger_rate, val);
             } else
                 return -EPERM;
        break;

        case IEEE80211_FIXED_RATE:
            {
                u_int8_t preamble, nss, rix;
                /* Note: Event though val is 32 bits, only the lower 8 bits matter */
                if (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_NONE) {
                    val = WMI_HOST_FIXED_RATE_NONE;
                }
                else {
                    rix = RC_2_RATE_IDX(vap->iv_fixed_rateset);
                    if (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_MCS) {
                        preamble = WMI_HOST_RATE_PREAMBLE_HT;
                        nss = HT_RC_2_STREAMS(vap->iv_fixed_rateset) -1;
                    }
                    else {
                        nss = 0;
                        rix = RC_2_RATE_IDX(vap->iv_fixed_rateset);

                        if(scn->burst_enable)
                        {
                            retval = ol_ath_pdev_set_param(scn,
                                    wmi_pdev_param_burst_enable, 0);

                            if (retval == EOK) {
                                scn->burst_enable = 0;
                            }
                        }

                        if (vap->iv_fixed_rateset & 0x10) {
                            if(IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
                                preamble = WMI_HOST_RATE_PREAMBLE_CCK;
                                if(rix != 0x3)
                                    /* Enable Short preamble always for CCK except 1mbps*/
                                    rix |= 0x4;
                            }
                            else {
                                qdf_err("Invalid, 5G does not support CCK\n");
                                return -EINVAL;
                            }
                        }
                        else {
                            preamble = WMI_HOST_RATE_PREAMBLE_OFDM;
                        }
                    }

                    if (ol_target_lithium(psoc)) {
                        val = ASSEMBLE_RATECODE_V1(rix, nss, preamble);
                        qdf_print("%s: Legacy/HT fixed rate value: 0x%x ", __func__, val);
                    } else {
                        val = ASSEMBLE_RATECODE_LEGACY(rix, nss, preamble);
                    }

                }

                retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                              wmi_vdev_param_fixed_rate, val);
           }
        break;
        case IEEE80211_FIXED_VHT_MCS:
           {
               if (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_VHT) {
                   wlan_util_vdev_mlme_get_param(vdev_mlme,
                           WLAN_MLME_CFG_NSS, &iv_nss);
                   if (ol_target_lithium(psoc)) {
                       val = ASSEMBLE_RATECODE_V1(vap->iv_vht_fixed_mcs, iv_nss-1, WMI_HOST_RATE_PREAMBLE_VHT);
                       qdf_print("%s: VHT fixed rate value: 0x%x ", __func__, val);
                   } else {
                       val = ASSEMBLE_RATECODE_LEGACY(vap->iv_vht_fixed_mcs,
                                                    (iv_nss - 1),
                                                    WMI_HOST_RATE_PREAMBLE_VHT);
                   }
               } else {
                    /* Note: Even though val is 32 bits, only the lower 8 bits matter */
                    val = WMI_HOST_FIXED_RATE_NONE;
                }
                retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                              wmi_vdev_param_fixed_rate, val);
           }
        break;
        case IEEE80211_FIXED_HE_MCS:
           {
                if (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_HE) {
                    wlan_util_vdev_mlme_get_param(vdev_mlme,
                            WLAN_MLME_CFG_NSS, &iv_nss);
                    val = ASSEMBLE_RATECODE_V1(vap->iv_he_fixed_mcs, iv_nss-1, WMI_HOST_RATE_PREAMBLE_HE);
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE , "%s : HE Fixed Rate %d \n",
                                        __func__, val);
                }
                else {
                    /* Note: Even though val is 32 bits, only the lower 8 bits matter */
                    val = WMI_HOST_FIXED_RATE_NONE;
                }

                retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                              wmi_vdev_param_fixed_rate, val);
           }
        break;
        case IEEE80211_FEATURE_APBRIDGE:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                              wmi_vdev_param_intra_bss_fwd, val);
            if (retval == EOK) {
                value.cdp_vdev_param_ap_brdg_en = val;
                if (cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id, CDP_ENABLE_AP_BRIDGE, value)
                                            != QDF_STATUS_SUCCESS)
                    return -1;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_vops) {
                    ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp, OSIF_NSS_VDEV_AP_BRIDGE_CFG);
                }
#endif
            }
        break;

        case IEEE80211_SHORT_GI:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                              wmi_vdev_param_sgi, val);
            qdf_info("Setting SGI value: %d", val);
        break;

        case IEEE80211_SECOND_CENTER_FREQ :
            if (ic->ic_modecaps &
                    ((1 << IEEE80211_MODE_11AC_VHT80_80) |
                     (1ULL << IEEE80211_MODE_11AXA_HE80_80))) {
                if ((vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80) ||
                    (vap->iv_des_mode == IEEE80211_MODE_11AXA_HE80_80)) {
                    vap->iv_des_cfreq2 = val;

                    qdf_print("Desired cfreq2 is %d. Please set primary 20 MHz "
                              "channel for cfreq2 setting to take effect",
                              vap->iv_des_cfreq2);
                } else {
                    qdf_print("Command inapplicable for this mode");
                    return -EINVAL;
                }
            } else {
                qdf_print("Command inapplicable because 80+80 MHz capability "
                          "is not available");
                return -EINVAL;
            }

        break;

        case IEEE80211_SUPPORT_TX_STBC:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                              wmi_vdev_param_tx_stbc, val);
        break;

        case IEEE80211_SUPPORT_RX_STBC:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                          wmi_vdev_param_rx_stbc, val);
        break;

        case IEEE80211_CONFIG_HE_UL_SHORTGI:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_shortgi, val);
        break;

        case IEEE80211_CONFIG_HE_UL_LTF:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_he_ltf, val);
        break;

        case IEEE80211_CONFIG_HE_UL_NSS:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_nss, val);
        break;

        case IEEE80211_CONFIG_HE_UL_PPDU_BW:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_ppdu_bw, val);
        break;

        case IEEE80211_CONFIG_HE_UL_LDPC:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_ldpc, val);
        break;

        case IEEE80211_CONFIG_HE_UL_STBC:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_stbc, val);
        break;

        case IEEE80211_CONFIG_HE_UL_FIXED_RATE:
            val = ASSEMBLE_RATECODE_V1(vap->iv_he_ul_fixed_rate, vap->iv_he_ul_nss-1,
                    WMI_HOST_RATE_PREAMBLE_HE);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE , "%s : HE UL Fixed Rate %d \n",
                                __func__, val);
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_ul_fixed_rate, val);
        break;

        case IEEE80211_DEFAULT_KEYID:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                          wmi_vdev_param_def_keyid, val);
        break;
#if UMAC_SUPPORT_PROXY_ARP
        case IEEE80211_PROXYARP_CAP:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                          wmi_vdev_param_mcast_indicate, val);
            retval |= ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                          wmi_vdev_param_dhcp_indicate, val);
        break;
#endif /* UMAC_SUPPORT_PROXY_ARP */
	    case IEEE80211_MCAST_RATE:
        {
            struct ieee80211_ath_channel *chan = vap->iv_des_chan[vap->iv_des_mode];
            int value;

            if ((!chan) || (chan == IEEE80211_CHAN_ANYC)) {
                vap->iv_mcast_rate_config_defered = TRUE;
                qdf_print("Configuring MCAST RATE is deffered as channel is not yet set for VAP ");
                break;
            }
            if (IEEE80211_IS_CHAN_5GHZ_6GHZ(chan) && (val < 6000)){
                qdf_print("%s: MCAST RATE should be at least 6000(kbps) for 5G",__func__);
                retval = -EINVAL;
                break;
            }

            if (!ieee80211_rate_is_valid_basic(vap,val)) {
                qdf_err("rate %d is not valid.",val);
                retval = EINVAL;
                break;
            }

            value = ol_get_rate_code(chan, val);
            if(value == EINVAL) {
                retval = -EINVAL;
                break;
            }
            if (ol_target_lithium(psoc)) {
                value = asemble_ratecode_lithium(value);
            }
            else {
                value = assemble_ratecode_legacy(value);
            }

            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_mcast_data_rate, value);
            if(retval == 0 ) {
                vap->iv_mcast_rate_config_defered = FALSE;
                qdf_print("%s: Now supported MCAST RATE is %d(kbps) and rate code: 0x%x",
                        __func__, val, value);
            }
        }
        break;
        case IEEE80211_BCAST_RATE:
        {
            struct ieee80211_ath_channel *chan = vap->iv_des_chan[vap->iv_des_mode];
            int value;

            if ((!chan) || (chan == IEEE80211_CHAN_ANYC)) {
                vap->iv_bcast_rate_config_defered = TRUE;
                qdf_print("Configuring BCAST RATE is deffered as channel is not yet set for VAP ");
                break;
            }
            if (IEEE80211_IS_CHAN_5GHZ_6GHZ(chan) && (val < 6000)) {
                qdf_print("%s: BCAST RATE should be at least 6000(kbps) for 5G",__func__);
                retval = -EINVAL;
                break;
            }

            if (!ieee80211_rate_is_valid_basic(vap,val)) {
                qdf_err("rate %d is not valid.",val);
                retval = EINVAL;
                break;
            }

            value = ol_get_rate_code(chan, val);
            if(value == EINVAL) {
                retval = -EINVAL;
                break;
            }
            if (ol_target_lithium(psoc)) {
                value = asemble_ratecode_lithium(value);
            }
            else {
                value = assemble_ratecode_legacy(value);
            }

            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                     wmi_vdev_param_bcast_data_rate, value);
            if (retval == 0) {
                vap->iv_bcast_rate_config_defered = FALSE;
                qdf_print("%s: Now supported BCAST RATE is %d(kbps) and rate code: 0x%x",
                        __func__, val, value);
            }
        }
        break;
        case IEEE80211_MGMT_RATE:
        {
            struct ieee80211_ath_channel *chan = vap->iv_des_chan[vap->iv_des_mode];
            int value;

            if ((!chan) || (chan == IEEE80211_CHAN_ANYC)) {
                vap->iv_mgt_rate_config_defered = TRUE;
                qdf_print("Configuring MGMT RATE is deffered as channel is not yet set for VAP ");
                break;
            }
            if (IEEE80211_IS_CHAN_5GHZ_6GHZ(chan) && (val < 6000)){
                qdf_print("%s: MGMT RATE should be at least 6000(kbps) for 5G",__func__);
                retval = EINVAL;
                break;
            }
            if(!ieee80211_rate_is_valid_basic(vap,val)){
                qdf_print("%s: rate %d is not valid. ",__func__,val);
                retval = EINVAL;
                break;
            }
            value = ol_get_rate_code(chan, val);
            if(value == EINVAL) {
                retval = EINVAL;
                break;
            }
            if (ol_target_lithium(psoc)) {
                value = asemble_ratecode_lithium(value);
            }
            else {
                value = assemble_ratecode_legacy(value);
            }

            mlme_cfg.value = value;
            retval = vdev_mlme_set_param(vdev_mlme,
                            WLAN_MLME_CFG_TX_MGMT_RATE_CODE, mlme_cfg);
            if (qdf_status_to_os_return(retval) == 0){
                vap->iv_mgt_rate_config_defered = FALSE;
                qdf_nofl_info("vdev[%d]: Mgt Rate:%d(kbps)",
                              wlan_vdev_get_id(vap->vdev_obj), val);
            }
#ifdef WLAN_SUPPORT_FILS
            if((retval >= 0) && IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan)) {
                retval = ol_ath_fd_tmpl_update(vap->vdev_obj);
            }
#endif /* WLAN_SUPPORT_FILS */
        }
        break;
        case IEEE80211_RTSCTS_RATE:
        {
            struct ieee80211_ath_channel *chan = vap->iv_des_chan[vap->iv_des_mode];
            int rtscts_rate;

            if ((!chan) || (chan == IEEE80211_CHAN_ANYC)) {
                qdf_print("Configuring  RATE for RTS and CTS is deffered as channel is not yet set for VAP ");
                break;
            }

            if(!ieee80211_rate_is_valid_basic(vap,val)){
                qdf_print("%s: Rate %d is not valid. ",__func__,val);
                retval = EINVAL;
                break;
            }
            rtscts_rate = ol_get_rate_code(chan, val);
            if(rtscts_rate == EINVAL) {
               retval = EINVAL;
                break;
            }
            if (ol_target_lithium(psoc)) {
                rtscts_rate = asemble_ratecode_lithium(rtscts_rate);
            }
            else {
                rtscts_rate = assemble_ratecode_legacy(rtscts_rate);
            }
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                        wmi_vdev_param_rts_fixed_rate, rtscts_rate);
            if (retval == 0)
                qdf_print("%s:Now supported CTRL RATE is : %d kbps and rate code : 0x%x",__func__,val,rtscts_rate);
        }
        break;
        case IEEE80211_BEACON_RATE_FOR_VAP:
        {
            struct ieee80211_ath_channel *chan = vap->iv_bsschan;
            int beacon_rate;

            if ((!chan) || (chan == IEEE80211_CHAN_ANYC)) {
                qdf_print("Configuring Beacon Rate is deffered as channel is not yet set for VAP ");
                retval = EINVAL;
                break;
            }

            beacon_rate = ol_get_rate_code(chan, val);
            if(beacon_rate == EINVAL) {
                retval = EINVAL;
                break;
            }

            if (ol_target_lithium(psoc)) {
                /* convert beacon's rate code for 8074 */
                beacon_rate = asemble_ratecode_lithium(beacon_rate);
            }
            else {
                beacon_rate = assemble_ratecode_legacy(beacon_rate);
            }
            mlme_cfg.value = beacon_rate;
            retval = vdev_mlme_set_param(vdev_mlme,
                            WLAN_MLME_CFG_BCN_TX_RATE_CODE, mlme_cfg);
        }
        break;

        case IEEE80211_MAX_AMPDU:
        /*should be moved to vap in future & add wmi cmd to update vdev*/
            retval = 0;
		break;
        case IEEE80211_VHT_MAX_AMPDU:
        /*should be moved to vap in future & add wmi cmd to update vdev*/
            retval = 0;
        break;

        case IEEE80211_VHT_SUBFEE:
        case IEEE80211_VHT_MUBFEE:
        case IEEE80211_VHT_SUBFER:
        case IEEE80211_VHT_MUBFER:
        case IEEE80211_VHT_BF_STS_CAP:
        case IEEE80211_SUPPORT_IMPLICITBF:
        case IEEE80211_VHT_BF_SOUNDING_DIM:
            vdev_mlme_set_param(vdev_mlme,
                    WLAN_MLME_CFG_TXBF_CAPS, mlme_cfg);
        break;
#if ATH_SUPPORT_IQUE
        case IEEE80211_ME:

        value.cdp_vdev_param_mcast_en = val;
        cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id, CDP_ENABLE_MCAST_EN, value);
        {
            if (val != 0) {
                cdp_tx_me_alloc_descriptor(soc_txrx_handle, pdev_id);
            } else {
                cdp_tx_me_free_descriptor(soc_txrx_handle, pdev_id);
            }
#if ATH_MCAST_HOST_INSPECT
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
                                      wmi_vdev_param_mcast_indicate, val);
#endif/*ATH_MCAST_HOST_INSPECT*/
        }
	break;
#endif /* ATH_SUPPORT_IQUE */

        case IEEE80211_ENABLE_RTSCTS:
             ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                     wmi_vdev_param_enable_rtscts, val);
        break;
        case IEEE80211_RC_NUM_RETRIES:
            ol_ath_wmi_send_vdev_param( scn, avn->av_if_id,
                    wmi_vdev_param_rc_num_retries, vap->iv_rc_num_retries);
        break;
#if WDS_VENDOR_EXTENSION
        case IEEE80211_WDS_RX_POLICY:
            if ((ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP) ||
                (ieee80211vap_get_opmode(vap) == IEEE80211_M_STA)) {
                if (cdp_set_wds_rx_policy(soc_txrx_handle,
                                      vdev_id, val & WDS_POLICY_RX_MASK) != QDF_STATUS_SUCCESS)
                    return -EINVAL;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops) {
                    struct wlan_objmgr_peer *peer =
                                wlan_objmgr_vdev_try_get_bsspeer(vap->vdev_obj, WLAN_MLME_NB_ID);
                    /*
                     * Send the WDS vendor policy configuration to NSS FW
                     */
                    if (!peer) {
                        qdf_err("Cound not find bss peer for vdev %pK", vap->vdev_obj);
                        return -EINVAL;
                    }
                    ic->nss_radio_ops->ic_nss_ol_wds_extn_peer_cfg_send(scn, peer->macaddr, vdev_id);
                    wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
                }
#endif
            }
        break;
#endif
        case IEEE80211_FEATURE_HIDE_SSID:
             if(val) {
                 if(!IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
                     IEEE80211_VAP_HIDESSID_ENABLE(vap);
                 }
             } else {
                 if(IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
                     IEEE80211_VAP_HIDESSID_DISABLE(vap);
                 }
             }
             retval = 0;
             break;
        case IEEE80211_FEATURE_PRIVACY:
             if(val) {
                 if(!IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
                     IEEE80211_VAP_PRIVACY_ENABLE(vap);
                 }
             } else {
                 if(IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
                     IEEE80211_VAP_PRIVACY_DISABLE(vap);
                 }
             }
             retval = 0;
             break;
        case IEEE80211_FEATURE_DROP_UNENC:
             if(val) {
                if(!IEEE80211_VAP_IS_DROP_UNENC(vap)) {
                    IEEE80211_VAP_DROP_UNENC_ENABLE(vap);
                }
             } else {
                if(IEEE80211_VAP_IS_DROP_UNENC(vap)) {
                    IEEE80211_VAP_DROP_UNENC_DISABLE(vap);
                }
             }
             mlme_cfg.value = val;
             value.cdp_vdev_param_drop_unenc = val;
             if (cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id, CDP_DROP_UNENC, value)
                 != QDF_STATUS_SUCCESS) {
                 retval = EINVAL;
                 break;
             }
             wlan_util_vdev_mlme_set_param(vdev_mlme, WLAN_MLME_CFG_DROP_UNENCRY,
                     mlme_cfg);
             retval = 0;
             break;
        case IEEE80211_SHORT_PREAMBLE:
             if(val) {
                 if(!IEEE80211_IS_SHPREAMBLE_ENABLED(ic)) {
                     IEEE80211_ENABLE_SHPREAMBLE(ic);
                 }
             } else {
                 if(IEEE80211_IS_SHPREAMBLE_ENABLED(ic)) {
                     IEEE80211_DISABLE_SHPREAMBLE(ic);
                 }
             }
             ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_preamble,
                    (val) ? WMI_HOST_VDEV_PREAMBLE_SHORT : WMI_HOST_VDEV_PREAMBLE_LONG);
             retval = 0;
             break;
        case IEEE80211_PROTECTION_MODE:
            if(val)
                IEEE80211_ENABLE_PROTECTION(ic);
            else
                IEEE80211_DISABLE_PROTECTION(ic);
            ic->ic_protmode = val;
            ieee80211_set_protmode(ic);
            retval = 0;
            break;
        case IEEE80211_SHORT_SLOT:
            mlme_cfg.value = !!val;
            vdev_mlme_set_param(vdev_mlme,
                    WLAN_MLME_CFG_SLOT_TIME, mlme_cfg);
            ieee80211_set_shortslottime(ic, mlme_cfg.value);
            wlan_pdev_beacon_update(ic);
            retval = 0;
            break;

        case IEEE80211_SET_CABQ_MAXDUR:
            ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_cabq_maxdur, val);
        break;

        case IEEE80211_FEATURE_MFP_TEST:
            ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_mfptest_set, val);
        break;

        case IEEE80211_VHT_SGIMASK:
             ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                         wmi_vdev_param_vht_sgimask, val);
        break;

        case IEEE80211_VHT80_RATEMASK:
             ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                         wmi_vdev_param_vht80_ratemask, val);
        break;

        case IEEE80211_VAP_RX_DECAP_TYPE:
        {
            if (!ol_ath_validate_rx_decap_type(scn, vap, val)) {
                retval = EINVAL;
            } else {
                mlme_cfg.value = val;
                retval = wlan_util_vdev_mlme_set_param(vdev_mlme,
                                         WLAN_MLME_CFG_RX_DECAP_TYPE, mlme_cfg);

                if (retval == 0) {
                    vap->iv_rx_decap_type = val;
                } else
                    qdf_print("Error %d setting param "
                           "WMI_VDEV_PARAM_RX_DECAP_TYPE with val %u",
                           retval,
                           val);
                }
        }
        break;

        case IEEE80211_VAP_TX_ENCAP_TYPE:
        {
            if (!ol_ath_validate_tx_encap_type(scn, vap, val)) {
                retval = EINVAL;
            } else {
                mlme_cfg.value = val;
                retval = wlan_util_vdev_mlme_set_param(vdev_mlme,
                                         WLAN_MLME_CFG_TX_ENCAP_TYPE, mlme_cfg);

                if (retval == 0) {
                    vap->iv_tx_encap_type = val;
                } else {
                    qdf_print("Error %d setting param "
                           "WMI_VDEV_PARAM_TX_ENCAP_TYPE with val %u",
                           retval,
                           val);
                }
            }
        }
        break;

        case IEEE80211_BW_NSS_RATEMASK:
             ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                         wmi_vdev_param_bw_nss_ratemask, val);
        break;

        case IEEE80211_RX_FILTER_MONITOR:
             if(ic->ic_is_mode_offload(ic)) {
                 value.cdp_pdev_param_fltr_ucast = ic->mon_filter_ucast_data;
                 cdp_txrx_set_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_UCAST_DATA, value);
                 value.cdp_pdev_param_fltr_mcast = ic->mon_filter_mcast_data;
                 cdp_txrx_set_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_MCAST_DATA, value);
                 value.cdp_pdev_param_fltr_none = ic->mon_filter_non_data;
                 cdp_txrx_set_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_NO_DATA, value);
             }
        break;

#if ATH_SUPPORT_NAC
        case IEEE80211_RX_FILTER_NEIGHBOUR_PEERS_MONITOR:
             value.cdp_pdev_param_fltr_neigh_peers = val;
             cdp_txrx_set_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_NEIGH_PEERS, value);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
             if (ic->nss_radio_ops)
                 ic->nss_radio_ops->ic_nss_set_cmd(scn, OSIF_NSS_WIFI_FILTER_NEIGH_PEERS_CMD);
#endif
             IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC, "%s: Monitor Invalid Peers Filter Set Val=%d \n", __func__, val);
        break;
#endif
	case IEEE80211_TXRX_VAP_STATS:
        {
            qdf_nofl_info("Get vap stats\n");
            ol_ath_net80211_get_vap_stats(vap);
            break;
        }
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    case IEEE80211_CLR_RAWMODE_PKT_SIM_STATS:
        clear_rawmode_pkt_sim_stats(vap);
	break;
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */
    case IEEE80211_TXRX_DBG_SET:
	cdp_debug(soc_txrx_handle, vdev_id, val);
        break;
    case IEEE80211_PEER_MUMIMO_TX_COUNT_RESET_SET:
        if (val <= 0) {
            qdf_print("Invalid AID value.");
            return -EINVAL;
        }
        retval = ol_ath_ucfg_reset_peer_mumimo_tx_count(vap, val);
        break;
    case IEEE80211_RATE_DROPDOWN_SET:
    {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW, "%s:Rate Control Logic Hex Value: 0x%X\n", __func__, val);
        retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                            wmi_vdev_param_rate_dropdown_bmap, val);
    }
        break;

    case IEEE80211_TX_PPDU_LOG_CFG_SET:
        if (cdp_fw_stats_cfg(soc_txrx_handle, vdev_id, HTT_DBG_CMN_STATS_TX_PPDU_LOG, val)
                             != QDF_STATUS_SUCCESS)
            return -EINVAL;

        break;
    case IEEE80211_TXRX_FW_STATS:
        {
            struct ol_txrx_stats_req req = {0};
            wlan_dev_t ic = vap->iv_ic;
            struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
            uint32_t target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
#if (UMAC_SUPPORT_VI_DBG || UMAC_VOW_DEBUG)
            osif_dev  *osifp = (osif_dev *)vap->iv_ifp;
#endif
            /*Dont pass to avoid TA */
            if ((lmac_is_target_ar900b(scn->soc->psoc_obj) == false) &&
                    (val > TXRX_FW_STATS_VOW_UMAC_COUNTER) &&
                    (target_type != TARGET_TYPE_QCA8074) &&
                    (target_type != TARGET_TYPE_QCA8074V2) &&
                    (target_type != TARGET_TYPE_QCA6018) &&
                    (target_type != TARGET_TYPE_QCA5018) &&
                    (target_type != TARGET_TYPE_QCN9000)) {
                     qdf_print("Not supported.");
                     return -EINVAL;
                }

            req.print.verbose = 1; /* default */

            /*
             * Backwards compatibility: use the same old input values, but
             * translate from the old values to the corresponding new bitmask
             * value.
             */
            if (val <= TXRX_FW_STATS_RX_RATE_INFO) {
                req.stats_type_upload_mask = 1 << (val - 1);
                if (val == TXRX_FW_STATS_TXSTATS) {
                    /* mask 17th bit as well to get extended tx stats */
                    req.stats_type_upload_mask |= (1 << 17);
                }
            } else if (val == TXRX_FW_STATS_PHYSTATS) {
                qdf_nofl_info("Value 4 for txrx_fw_stats is obsolete \n");
                break;
            } else if (val == TXRX_FW_STATS_PHYSTATS_CONCISE) {
                /*
                 * Stats request 5 is the same as stats request 4,
                 * but with only a concise printout.
                 */
                req.print.concise = 1;
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_PHYSTATS - 1);
            }
            else if (val == TXRX_FW_STATS_TX_RATE_INFO) {
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TX_RATE_INFO - 2);
            }
            else if (val == TXRX_FW_STATS_TID_STATE) { /* for TID queue stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TID_STATE - 2);
            }
            else if (val == TXRX_FW_STATS_TXBF_INFO) { /* for TxBF stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TXBF_INFO - 7);
            }
            else if (val == TXRX_FW_STATS_SND_INFO) { /* for TxBF Snd stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_SND_INFO - 7);
            }
            else if (val == TXRX_FW_STATS_ERROR_INFO) { /* for TxRx error stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_ERROR_INFO - 7);
            }
            else if (val == TXRX_FW_STATS_TX_SELFGEN_INFO) { /* for SelfGen stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TX_SELFGEN_INFO - 7);
            }
            else if (val == TXRX_FW_STATS_TX_MU_INFO) { /* for TX MU stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TX_MU_INFO - 7);
            }
            else if (val == TXRX_FW_SIFS_RESP_INFO) { /* for SIFS RESP stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_SIFS_RESP_INFO - 7);
            }
            else if (val == TXRX_FW_RESET_STATS) { /*for  Reset stats info*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_RESET_STATS - 7);
            }
            else if (val == TXRX_FW_MAC_WDOG_STATS) { /*for  wdog stats info*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_MAC_WDOG_STATS - 7);
            }
            else if (val == TXRX_FW_MAC_DESC_STATS) { /*for fw desc stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_MAC_DESC_STATS - 7);
            }
            else if (val == TXRX_FW_MAC_FETCH_MGR_STATS) { /*for fetch mgr stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_MAC_FETCH_MGR_STATS - 7);
            }
            else if (val == TXRX_FW_MAC_PREFETCH_MGR_STATS) { /*for prefetch mgr stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_MAC_PREFETCH_MGR_STATS - 7);
            } else if (val  == TXRX_FW_COEX_STATS) { /* for coex stats */
                req.stats_type_upload_mask = 1 << (TXRX_FW_COEX_STATS - 8);
            } else if (val == TXRX_FW_HALPHY_STATS) { /*for fetch halphy stats*/
                req.stats_type_upload_mask = 1 << (TXRX_FW_HALPHY_STATS - 8);
            }


#ifdef WLAN_FEATURE_FASTPATH
            /* Get some host stats */
            /* Piggy back on to fw stats command */
            /* TODO : Separate host / fw commands out */
            if (val == TXRX_FW_STATS_HOST_STATS) {
                cdp_host_stats_get(soc_txrx_handle, vdev_id, &req);
            } else if (val == TXRX_FW_STATS_CLEAR_HOST_STATS) {
                if (cdp_host_stats_clr(soc_txrx_handle, vdev_id) != QDF_STATUS_SUCCESS)
                    return -EINVAL;
            } else if (val == TXRX_FW_STATS_CE_STATS) {
                qdf_nofl_info("Value 10 for txrx_fw_stats is obsolete \n");
                break;
#if ATH_SUPPORT_IQUE
            } else if (val == TXRX_FW_STATS_ME_STATS) {
                cdp_host_me_stats(soc_txrx_handle, vdev_id);
#endif
            } else if (val <= TXRX_FW_MAC_PREFETCH_MGR_STATS || val <= TXRX_FW_COEX_STATS)
#endif /* WLAN_FEATURE_FASTPATH */
                {
                    cdp_fw_stats_get(soc_txrx_handle, vdev_id, &req, PER_RADIO_FW_STATS_REQUEST, 0);
#if PEER_FLOW_CONTROL
                    /* MSDU TTL host display */
                    if(val == 1) {
                        if (cdp_host_msdu_ttl_stats(soc_txrx_handle, vdev_id, &req) != QDF_STATUS_SUCCESS)
                            return -EINVAL;
                    }
#endif
                }

            if (val == TXRX_FW_STATS_DURATION_INFO) {
               scn->tx_rx_time_info_flag = 1;
               ic->ic_ath_bss_chan_info_stats(ic, 1);
               break;
            }

            if (val == TXRX_FW_STATS_DURATION_INFO_RESET) {
               scn->tx_rx_time_info_flag = 1;
               ic->ic_ath_bss_chan_info_stats(ic, 2);
               break;
            }

#if UMAC_SUPPORT_VI_DBG
            if( osifp->vi_dbg) {
                if(val == TXRX_FW_STATS_VOW_UMAC_COUNTER)
                    {
                        ieee80211_vi_dbg_print_stats(vap);
                    }
            }
#elif UMAC_VOW_DEBUG

            if( osifp->vow_dbg_en) {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                osif_nss_vdev_get_vow_dbg_stats(osifp);
#endif
                if(val == TXRX_FW_STATS_RXSTATS)
                    {
                        qdf_nofl_info(" %lu VI/mpeg streamer pkt Count recieved at umac\n", osifp->umac_vow_counter);
                    }
                else if( val == TXRX_FW_STATS_VOW_UMAC_COUNTER ) {

                    for( ii = 0; ii < MAX_VOW_CLIENTS_DBG_MONITOR; ii++ )
                        {
                            qdf_nofl_info(" %lu VI/mpeg stream pkt txed at umac for peer %d[%02X:%02X]\n",
                                   osifp->tx_dbg_vow_counter[ii], ii, osifp->tx_dbg_vow_peer[ii][0], osifp->tx_dbg_vow_peer[ii][1]);
                        }

                }
            }
#endif
            break;
        }
    case IEEE80211_PEER_TX_COUNT_SET:
        if (val <= 0){
            qdf_print("Invalid AID value.");
            return -EINVAL;
	}
	    retval = wlan_get_peer_mumimo_tx_count(vap, val);
	    break;
    case IEEE80211_CTSPROT_DTIM_BCN_SET:
	    ol_ath_set_vap_cts2self_prot_dtim_bcn(vap);
	    break;
    case IEEE80211_PEER_POSITION_SET:
	    if (val <= 0) {
            qdf_print("Invalid AID value.");
            return -EINVAL;
        }
	    retval = wlan_get_user_postion(vap, (u_int32_t)val);
	    break;
    case IEEE80211_VAP_TXRX_FW_STATS:
    {
        struct ol_txrx_stats_req req = {0};
        if ((lmac_is_target_ar900b(scn->soc->psoc_obj) == false) &&
                 (val > TXRX_FW_STATS_VOW_UMAC_COUNTER)) { /* Wlan F/W doesnt like this */
              qdf_print("Not supported.");
              return -EINVAL;
        }

        req.print.verbose = 1; /* default */
        if (val == TXRX_FW_STATS_SND_INFO) { /* for TxBF Snd stats*/
            req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_SND_INFO - 7);
        } else if (val == TXRX_FW_STATS_TX_SELFGEN_INFO) { /* for SelfGen stats*/
            req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TX_SELFGEN_INFO - 7);
        } else if (val == TXRX_FW_STATS_TX_MU_INFO) { /* for TX MU stats*/
            req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TX_MU_INFO - 7);
        } else {
            /*
             * The command iwpriv athN vap_txrx_stats is used to get per vap
             * sounding info, selfgen info and tx mu stats only.
             */
            qdf_print("Vap specific stats is implemented only for stats type 14 16 and 17");
            return -EINVAL;
        }
        cdp_fw_stats_get(soc_txrx_handle, vdev_id, &req, PER_VDEV_FW_STATS_REQUEST, 0);
    break;
    }
    case IEEE80211_TXRX_FW_MSTATS:
    {
        struct ol_txrx_stats_req req = {0};
        req.print.verbose = 1;
        req.stats_type_upload_mask = val;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (ic->nss_vops) {
            qdf_err("Not supported with WiFi Offload mode enabled ");
            return -EINVAL;
        }
#endif
        cdp_fw_stats_get(soc_txrx_handle, vdev_id, &req, PER_RADIO_FW_STATS_REQUEST, 0);
        break;
    }

   case IEEE80211_VAP_TXRX_FW_STATS_RESET:
    {
        struct ol_txrx_stats_req req = {0};
#if UMAC_VOW_DEBUG
        osif_dev  *osifp = (osif_dev *)vap->iv_ifp;
#endif
        if (val == TXRX_FW_STATS_SND_INFO) { /* for TxBF Snd stats*/
            req.stats_type_reset_mask = 1 << (TXRX_FW_STATS_SND_INFO - 7);
        } else if (val == TXRX_FW_STATS_TX_SELFGEN_INFO) { /* for SelfGen stats*/
            req.stats_type_reset_mask = 1 << (TXRX_FW_STATS_TX_SELFGEN_INFO - 7);
        } else if (val == TXRX_FW_STATS_TX_MU_INFO) { /* for TX MU stats*/
            req.stats_type_reset_mask = 1 << (TXRX_FW_STATS_TX_MU_INFO - 7);
        } else {
            /*
             * The command iwpriv athN vap_txrx_st_rst is used to reset per vap
             * sounding info, selfgen info and tx mu stats only.
             */
            qdf_print("Vap specific stats reset is implemented only for stats type 14 16 and 17");
            return -EINVAL;
        }
        cdp_fw_stats_get(soc_txrx_handle, vdev_id, &req, PER_VDEV_FW_STATS_REQUEST, 0);
#if UMAC_VOW_DEBUG
        if(osifp->vow_dbg_en)
        {
            for( ii = 0; ii < MAX_VOW_CLIENTS_DBG_MONITOR; ii++ )
            {
                 osifp->tx_dbg_vow_counter[ii] = 0;
            }
            osifp->umac_vow_counter = 0;
        }
#endif
        break;
    }

    case IEEE80211_TXRX_FW_STATS_RESET:
    {
        struct ol_txrx_stats_req req = {0};
        struct cdp_txrx_stats_req cdp_req = {0};
#if UMAC_VOW_DEBUG
        osif_dev  *osifp = (osif_dev *)vap->iv_ifp;
#endif
        req.stats_type_reset_mask = val;
        cdp_fw_stats_get(soc_txrx_handle, vdev_id, &req, PER_RADIO_FW_STATS_REQUEST, 0);
#if UMAC_VOW_DEBUG
        if(osifp->vow_dbg_en)
        {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (ic->nss_vops)
                ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_WIFI_VDEV_VOW_DBG_RST_STATS);
#endif
            for( ii = 0; ii < MAX_VOW_CLIENTS_DBG_MONITOR; ii++ )
            {
                 osifp->tx_dbg_vow_counter[ii] = 0;
            }
            osifp->umac_vow_counter = 0;

        }
#endif
        cdp_req.stats = CDP_TXRX_STATS_0;
        cdp_req.param0 = val;
        cdp_req.param1 = 0x1;
        cdp_txrx_stats_request(soc_txrx_handle, vdev_id, &cdp_req);
        break;
    }
#if ATH_SUPPORT_DSCP_OVERRIDE
    case IEEE80211_DSCP_MAP_ID:
    {
        if(ic->ic_is_mode_offload(ic)) {
            ol_ath_set_vap_dscp_tid_map(vap);
        }
        break;
    }
    case IEEE80211_DP_DSCP_MAP:
    {
        ol_ath_set_pdev_dscp_tid_map(vap, val);
#if ATH_SUPPORT_HS20
        vap->iv_hotspot_xcaps2 |= IEEE80211_EXTCAPIE_QOS_MAP;
#endif
        wlan_vdev_beacon_update(vap);
        break;
    }
#endif /* ATH_SUPPORT_DSCP_OVERRIDE */

    case IEEE80211_CONFIG_VAP_TXPOW_MGMT:
    {
        ol_wlan_txpow_mgmt(vap,(u_int8_t)val);
    }
    break;

    case IEEE80211_CONFIG_MCAST_RC_STALE_PERIOD:
    {
        retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                          wmi_vdev_param_mcast_rc_stale_period,
                                          val);
        if (retval == EOK) {
            vap->iv_mcast_rc_stale_period = val;
        }
    }
    break;

    case IEEE80211_CONFIG_ENABLE_MCAST_RC:
    {
        retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                            wmi_vdev_param_enable_mcast_rc,
                                            !!val);
        if (retval == EOK) {
            vap->iv_enable_mcast_rc = !!val;
        }
    }
    break;
#ifdef WLAN_SUPPORT_FILS
    case IEEE80211_CONFIG_6GHZ_BCAST_PROB_RSP:
    {
        retval = ol_ath_wmi_send_vdev_bcast_prbrsp_param(scn,
                     avn->av_if_id, val);
    }
    break;
#endif
    case IEEE80211_CONFIG_VAP_TXPOW:
    {
        mlme_cfg.value = val;
        vdev_mlme_set_param(vdev_mlme,
                WLAN_MLME_CFG_TX_POWER,
                mlme_cfg);
    }
    break;

    case IEEE80211_CONFIG_TX_CAPTURE:
    {
#if ATH_PERF_PWR_OFFLOAD
        struct ol_ath_soc_softc *soc;
        struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
#endif

#if ATH_DATA_TX_INFO_EN
        if (scn->enable_perpkt_txstats) {
            qdf_print("Disable data_txstats before enabling debug sniffer");
            retval = -EINVAL;
            break;
        }
#endif

        soc = scn->soc;
#ifdef QCA_SUPPORT_RDK_STATS
        if (soc->wlanstats_enabled) {
            qdf_err("Disable peer rate stats before enabling debug sniffer");
            retval = -EINVAL;
            break;
        }
#endif
            /* To handle case when M-copy is enabled through monitor vap */
        if (!val && (ol_ath_is_mcopy_enabled(ic))) {
            retval = -EINVAL;
            break;
        }
        retval = ol_ath_set_debug_sniffer(scn, val);
        if (!ol_target_lithium(psoc)){
            retval = ol_ath_set_tx_capture(scn, val);

            if (!retval)
                ic->ic_tx_capture = val;
        }
    }
    break;

    case IEEE80211_CONFIG_VDEV_PEER_PROTOCOL_COUNT:
        cdp_set_vdev_peer_protocol_count(soc_txrx_handle, vdev_id, val);
        break;
    case IEEE80211_CONFIG_VDEV_PEER_PROTOCOL_DROP_MASK:
        cdp_set_vdev_peer_protocol_drop_mask(soc_txrx_handle, vdev_id, val);
        break;

#if MESH_MODE_SUPPORT
    case IEEE80211_CONFIG_MESH_MCAST:
        qdf_print("%s: Mesh param param:%u value:%u", __func__,param, val);

        retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_mesh_mcast_enable, val);
     break;

    case IEEE80211_CONFIG_RX_MESH_FILTER:
        qdf_print("%s: Mesh filter param:%u value:%u", __func__,param, val);
        value.cdp_vdev_param_mesh_rx_filter = val;
        if (cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id, CDP_MESH_RX_FILTER, value) != QDF_STATUS_SUCCESS)
            retval = EINVAL;

     break;
#endif

        case IEEE80211_CONFIG_HE_EXTENDED_RANGE:
           {
                retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                         wmi_vdev_param_he_range_ext_enable, val);

                IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                     "%s : HE Extended Range %d \n",__func__, val);
           }
        break;
        case IEEE80211_CONFIG_HE_DCM:
           {
                retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                         wmi_vdev_param_he_dcm_enable, val);

                IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                     "%s : HE DCM %d \n",__func__, val);
           }
        break;

        case IEEE80211_TXRX_DP_STATS:
        {
            struct cdp_txrx_stats_req req = {0,};
            req.stats = val;
            cdp_txrx_stats_request(soc_txrx_handle, vdev_id, &req);
            break;
        }

#if 0
        case IEEE80211_CONFIG_HE_FRAGMENTATION:
        {
           /* We are using user-configured HE
            * fragmentation value to advertise
            * in beacon, probe resp and assoc
            * resp and to inform peer about our
            * Rx cap for HE fragmentation level
            * in addba resp ext-element. So, we
            * do not need to send HE fragmentaion
            * level to FW as FW is handling TX
            * only.
            */
        }
        break;
#endif

        case IEEE80211_CONFIG_HE_BSS_COLOR:
        {
            uint32_t he_bsscolor = 0;

            if(!val) {
                WMI_HOST_HEOPS_BSSCOLOR_DISABLE_SET(he_bsscolor,
                        (!IEEE80211_HE_BSS_COLOR_ENABLE));
            }
            else {
                WMI_HOST_HEOPS_BSSCOLOR_SET(he_bsscolor, val);
            }
#if SUPPORT_11AX_D3
            he_bsscolor = (he_bsscolor << HEOP_PARAM_S);
#endif
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                     wmi_vdev_param_he_bss_color, he_bsscolor);

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                 "%s : HE BSS Color  %d \n",__func__, val);
        }
        break;
        case IEEE80211_CONFIG_HE_SU_BFEE:
        case IEEE80211_CONFIG_HE_SU_BFER:
        case IEEE80211_CONFIG_HE_MU_BFEE:
        case IEEE80211_CONFIG_HE_MU_BFER:
        case IEEE80211_CONFIG_HE_UL_MU_OFDMA:
        case IEEE80211_CONFIG_HE_DL_MU_OFDMA:
        case IEEE80211_CONFIG_HE_DL_MU_OFDMA_BFER:
        case IEEE80211_CONFIG_HE_UL_MU_MIMO:
        {
            uint32_t he_bf_cap =0;

            qdf_info("VDEV params:"
                      "HE su_bfee:%d|su_bfer:%d|"
                      "mu_bfee:%d|mu_bfer:%d|"
                      "dl_muofdma:%d|ul_muofdma:%d|"
                      "ul_mumimo:%d|dl_muofdma_bfer:%d",
                    vap->iv_he_su_bfee, vap->iv_he_su_bfer, vap->iv_he_mu_bfee,
                    vap->iv_he_mu_bfer, vap->iv_he_dl_muofdma, vap->iv_he_ul_muofdma,
                    vap->iv_he_ul_mumimo, vap->iv_he_dl_muofdma_bfer);

            WMI_HOST_HE_BF_CONF_SU_BFEE_SET(he_bf_cap, vap->iv_he_su_bfee);
            WMI_HOST_HE_BF_CONF_SU_BFER_SET(he_bf_cap, vap->iv_he_su_bfer);
            WMI_HOST_HE_BF_CONF_MU_BFEE_SET(he_bf_cap, vap->iv_he_mu_bfee);
            WMI_HOST_HE_BF_CONF_MU_BFER_SET(he_bf_cap, vap->iv_he_mu_bfer);
            WMI_HOST_HE_BF_CONF_DL_OFDMA_SET(he_bf_cap, vap->iv_he_dl_muofdma);
            WMI_HOST_HE_BF_CONF_UL_OFDMA_SET(he_bf_cap, vap->iv_he_ul_muofdma);
            WMI_HOST_HE_BF_CONF_UL_MUMIMO_SET(he_bf_cap, vap->iv_he_ul_mumimo);

            HE_SET_BITS(he_bf_cap, HE_BF_CONF_DL_OFDMA_BFER_BIT_POS,
                HE_BF_CONF_DL_OFDMA_BFER_NUM_BITS, vap->iv_he_dl_muofdma_bfer);

            qdf_info("he_bf_cap=0x%x", he_bf_cap);

            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                     wmi_vdev_param_set_hemu_mode, he_bf_cap);
        }
        break;
        case IEEE80211_CONFIG_HE_SOUNDING_MODE:
        {
            qdf_info("VDEV params:"
                    "AC/VHT sounding mode:%s|"
                    "SU/MU sounding mode:%s|"
                    "Trig/Non-Trig sounding mode:%s",
                    WMI_HOST_HE_VHT_SOUNDING_MODE_GET(val) ? "HE" : "VHT",
                    WMI_HOST_SU_MU_SOUNDING_MODE_GET(val) ? "MU" : "SU",
                    WMI_HOST_TRIG_NONTRIG_SOUNDING_MODE_GET(val) ? "Trigged" :
                    "Non-Trigged");

            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                            wmi_vdev_param_set_he_sounding_mode, val);
        }
        break;
        case IEEE80211_CONFIG_HE_LTF:
        {
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                     wmi_vdev_param_set_he_ltf, val);

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                     "%s : HE LTF %d \n",__func__, val);
        }
        break;
        case IEEE80211_CONFIG_HE_AR_GI_LTF:
        {
            if(vap->iv_he_ar_ldpc == IEEE80211_HE_AR_LDPC_DEFAULT){
              val = vap->iv_he_ar_gi_ltf;
            }else{
              val = vap->iv_he_ar_gi_ltf | (vap->iv_he_ar_ldpc << IEEE80211_HE_AR_LDPC_SHIFT);
            }
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                     wmi_vdev_param_autorate_misc_cfg, val);

            qdf_print("%s : HE AUTORATE GI LTF 0x%x ",
                            __func__, vap->iv_he_ar_gi_ltf);
        }
        break;
        case IEEE80211_CONFIG_HE_AR_LDPC:
        {
            val = vap->iv_he_ar_gi_ltf | (vap->iv_he_ar_ldpc << IEEE80211_HE_AR_LDPC_SHIFT);
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                     wmi_vdev_param_autorate_misc_cfg, val);
            qdf_info("%s : HE AUTORATE LDPC 0x%x ",
                           __func__, vap->iv_he_ar_ldpc);
        }
        break;
        case IEEE80211_CONFIG_HE_OP:
        {
            mlme_cfg.value = val;
            vdev_mlme_set_param(vdev_mlme,
                    WLAN_MLME_CFG_HE_OPS, mlme_cfg);

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                     "%s : HE OP %d \n",__func__, val);
        }
        break;
        case IEEE80211_CONFIG_HE_RTSTHRSHLD:
        {
            uint32_t heop = 0;
            struct ieee80211com *ic = vap->iv_ic;

            ol_ath_populate_bsscolor_in_vdev_param_heop(ic, &heop);
#if SUPPORT_11AX_D3
            heop |= (ic->ic_he.heop_param |
                    ((val << IEEE80211_HEOP_RTS_THRESHOLD_S) &
                     IEEE80211_HEOP_RTS_THRESHOLD_MASK));
#else
            heop |= ((ic->ic_he.heop_param &
                ~(IEEE80211_HEOP_BSS_COLOR_MASK << IEEE80211_HEOP_BSS_COLOR_S))
                 |((val << IEEE80211_HEOP_RTS_THRESHOLD_S) &
                 IEEE80211_HEOP_RTS_THRESHOLD_MASK));
#endif
            mlme_cfg.value = heop;
            vdev_mlme_set_param(vdev_mlme,
                    WLAN_MLME_CFG_HE_OPS, mlme_cfg);

            /* Increment the TIM update beacon count to indicate change in
             * HEOP parameter */
            ic->ic_is_heop_param_updated = true;
            wlan_pdev_beacon_update(ic);
            ic->ic_is_heop_param_updated = false;

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                     "%s : HE RTS THRSHLD %d \n",__func__, val);
            break;
        }

        case IEEE80211_FEATURE_DISABLE_CABQ:
            retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id, wmi_vdev_param_disable_cabq, val);
        break;

        case IEEE80211_CONFIG_M_COPY:
            if (val && ic->ic_debug_sniffer) {
                qdf_err("Monitor/M_COPY is already enabled");
                retval = -EINVAL;
                break;
	    }
	    if (val) {
                if(val == MODE_M_COPY || val == MODE_EXT_M_COPY) {
                   if (val == MODE_M_COPY)
                       sniffer_mode = SNIFFER_M_COPY_MODE;
                   else
                       sniffer_mode = SNIFFER_EXT_M_COPY_MODE;

                   if (ol_ath_set_debug_sniffer(scn, sniffer_mode) == 0)
                       ol_ath_pdev_set_param(scn, wmi_pdev_param_set_promisc_mode_cmdid, 1);
                   else
                       qdf_info("Error in enabling m_copy mode");
                } else
                   qdf_info("Invalid value, expected 1 for m_copy mode and 2 for ext_m_copy mode");
	    } else {
	        if (ol_ath_set_debug_sniffer(scn, SNIFFER_DISABLE) == 0) {
                    ol_ath_pdev_set_param(scn,
				          wmi_pdev_param_set_promisc_mode_cmdid, 0);
		} else {
                    qdf_print("Error in disabling m_copy mode");
		}
	    }
	    break;
#if QCN_IE
        case IEEE80211_CONFIG_BCAST_PROBE_RESPONSE:
            ol_ath_set_bpr_wifi3(scn, val);
            break;
#endif

        case IEEE80211_CONFIG_CAPTURE_LATENCY_ENABLE:
            ol_ath_set_capture_latency(scn, val);
            break;

        case IEEE80211_CONFIG_ADDBA_MODE:
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_set_ba_mode, val);

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                     "%s : BA MODE %d \n",__func__, val);
            break;
        case IEEE80211_CONFIG_BA_BUFFER_SIZE:
        {
        /* Map the user set value 0/1 to 2/3 to configure the BA buffer size
         * VDEV PARAM for BA MODE has been extended to configure the BA buffer
         * size along with setting the ADDBA mode. The values 2(buffer size 64)
         * and 3(buffer size 255) are for the BA MODE to configure the BA buffer
         * size. The values 0(Auto mode) and 1(Manual mode) for BA MODE VDEV
         * PARAM will configure the ADDBA mode. */
            val += IEEE80211_BA_MODE_BUFFER_SIZE_OFFSET;
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_set_ba_mode, val);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE ,
                    "%s : BA Buffer size  %d \n",__func__,
                    (val - IEEE80211_BA_MODE_BUFFER_SIZE_OFFSET));
        }
        break;
        case IEEE80211_CONFIG_READ_RXPREHDR:
        {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            osif_dev  *osifp = (osif_dev *)vap->iv_ifp;
            if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                ic->nss_vops->ic_osif_nss_vdev_set_read_rxprehdr(osifp, (uint32_t)val);
            }
#endif
        }
        break;
        case IEEE80211_UPDATE_DEV_STATS:
        {
           struct cdp_dev_stats cdp_stats;
           cdp_ath_getstats(soc_txrx_handle, vdev_id, &cdp_stats,
                            UPDATE_VDEV_STATS);
        }
        break;
        case IEEE80211_CONFIG_RU26:
            retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_ru26_allowed, val);
        break;
        case IEEE80211_BSTEER_EVENT_ENABLE:
        {
#if DBDC_REPEATER_SUPPORT
            uint32_t target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
            /*Disable WDS on STA vaps on secondary radio*/
            if ((target_type == TARGET_TYPE_QCA8074) && (!ic->ic_primary_radio)) {
                wlan_iterate_vap_list(ic, ol_ath_vap_iter_sta_wds_disable, NULL);
            }
#endif
            break;
        }
        case IEEE80211_CONFIG_RAWMODE_OPEN_WAR:
        {
            uint32_t target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

            if (target_type == TARGET_TYPE_QCA8074) {
                retval = ol_ath_wmi_send_vdev_param(scn,
                            avn->av_if_id,
                            wmi_vdev_param_rawmode_open_war, val);
            } else {
                qdf_err("Not supported!!");
                return -EINVAL;
            }
        }
        break;
        case IEEE80211_CONFIG_INDICATE_FT_ROAM:
            retval = ol_ath_send_ft_roam_start_stop(vap, val);
        break;
	case IEEE80211_FEATURE_EXTAP:
	{
	    /* DISABLE DA LEARN at SOC level if extap is enabled on any VAP */
	    if (val) {
                value.cdp_vdev_param_da_war = 0;
                if (cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id,
                                            CDP_ENABLE_DA_WAR, value) != QDF_STATUS_SUCCESS)
                    return -EINVAL;
	    } else {
                value.cdp_vdev_param_da_war = 1;
                if (cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id,
                                            CDP_ENABLE_DA_WAR, value) != QDF_STATUS_SUCCESS)
                    return -EINVAL;
	    }
	}
	break;

        case IEEE80211_CONFIG_ENABLE_MULTI_GROUP_KEY:
            if (ol_target_lithium(psoc)) {
                vap->enable_multi_group_key = val ? 1:0 ;
                mlme_cfg.value = val;
                retval = wlan_util_vdev_mlme_set_param(vdev_mlme,
                        WLAN_MLME_CFG_ENABLE_MULTI_GROUP_KEY, mlme_cfg);
                value.cdp_vdev_param_update_multipass = val;
                if (cdp_txrx_set_vdev_param(soc_txrx_handle,
                        vdev_id,
                        CDP_UPDATE_MULTIPASS, value) != QDF_STATUS_SUCCESS)
                    return -EINVAL;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_vops) {
                    ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp, OSIF_NSS_WIFI_VDEV_CFG_MULTIPASS);
                }
#endif
            }
        break;

        case IEEE80211_CONFIG_MAX_GROUP_KEYS:
            if (ol_target_lithium(psoc) && (val < MAX_VLAN)) {
                vap->max_group_keys = val;
                mlme_cfg.value = val;
                retval = wlan_util_vdev_mlme_set_param(vdev_mlme,
                            WLAN_MLME_CFG_MAX_GROUP_KEYS, mlme_cfg);
            }
        break;
        case IEEE80211_CONFIG_MAX_MTU_SIZE:
        {
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                        wmi_vdev_param_max_mtu_size, val);
        }
        break;

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
        case IEEE80211_CONFIG_CFR_RCC:
        {
            uint8_t pdev_id;

            pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);

            if ((val == 1) && !cdp_get_cfr_rcc(soc_txrx_handle, pdev_id)) {
                ol_ath_subscribe_ppdu_desc_info(scn,
                                                PPDU_DESC_CFR_RCC);
            } else if ((!val && cdp_get_cfr_rcc(soc_txrx_handle, pdev_id))
                       || (val == 2)) {
                ol_ath_unsubscribe_ppdu_desc_info(scn,
                                                  PPDU_DESC_CFR_RCC);
            }
        }
	    break;
#endif
        case IEEE80211_CONFIG_6GHZ_NON_HT_DUP:
        {
            uint32_t mgt_rate = 0;
            retval = ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                                    wmi_vdev_param_6ghz_params, val);

            if((retval >= 0) && IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan)) {
#ifdef WLAN_SUPPORT_FILS
                retval = ol_ath_fd_tmpl_update(vap->vdev_obj);
#endif /* WLAN_SUPPORT_FILS */
                /* By default 6GHz AP is configured to use HE rates for all
                 * mgmt frames. To enable non-HT duplicate frames,
                 * mgmt rates should be set to legacy(non-HT) rates.
                 */
                if(vap->iv_disabled_legacy_rate_set) {
                    wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
                            WLAN_MLME_CFG_TX_MGMT_RATE, &mgt_rate);
                    retval = wlan_set_param(vap, IEEE80211_MGMT_RATE, mgt_rate);
                } else {
                    retval = wlan_set_param(vap, IEEE80211_MGMT_RATE,
                                        IEEE80211_HE_6GHZ_NON_HT_RATE);
                }
            }
        }
        break;

#ifdef PORT_SPIRENT_HK
#ifdef SPT_CAPTURE
        case IEEE80211_CONFIG_CAPTURE_MODE_SET:
             cdp_set_dp_htt_capture_mode(soc_txrx_handle, pdev_id, val);
              /* Getting the capture mode */
		      vap->cap_mode = val;
        break;
#endif // SPT_CAPTURE
#ifdef SPT_BSS_COLOR
        case IEEE80211_CONFIG_BSS_COLOR_STATE_SET:
            cdp_set_bss_state(soc_txrx_handle, vdev_id, val);
        break;
#endif // SPT_BSS_COLOR
#endif
        default:
            /*qdf_print("%s: VAP param unsupported param:%u value:%u", __func__,
                         param, val);*/
        break;
    }

    return(retval);
}

static int
ol_ath_vap_set_ru26_tolerant(struct ieee80211com *ic, bool val)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    int retval = -1;

    if (!ic) {
        return retval;
    }

    scn = OL_ATH_SOFTC_NET80211(ic);
    retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_ru26_allowed, val);
    return retval;
}

static int16_t ol_ath_vap_dyn_bw_rts(struct ieee80211vap *vap, int param)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    int retval = 0;

    retval = ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
			wmi_vdev_param_disable_dyn_bw_rts, param);
    return retval;
}

void ol_ath_get_min_and_max_power(struct ieee80211com *ic,
        int8_t *max_tx_power,
        int8_t *min_tx_power)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wlan_psoc_target_capability_info *target_cap;
    ol_ath_soc_softc_t *soc = scn->soc;
    struct target_psoc_info *tgt_hdl;

    tgt_hdl = wlan_psoc_get_tgt_if_handle(soc->psoc_obj);
    if (!tgt_hdl) {
        target_if_err("%s: target_psoc_info is null", __func__);
        return;
    }
    target_cap = target_psoc_get_target_caps(tgt_hdl);
    *max_tx_power = target_cap->hw_max_tx_power;
    *min_tx_power = target_cap->hw_min_tx_power;
}

uint32_t ol_ath_get_modeSelect(struct ieee80211com *ic)
{
    uint32_t wMode;
    uint32_t netBand;

    wMode = WMI_HOST_REGDMN_MODE_ALL;

    if (!(wMode & HOST_REGDMN_MODE_11A)) {
        wMode &= ~(HOST_REGDMN_MODE_TURBO |
                HOST_REGDMN_MODE_108A |
                HOST_REGDMN_MODE_11A_HALF_RATE);
    }

    if (!(wMode & HOST_REGDMN_MODE_11G)) {
        wMode &= ~(HOST_REGDMN_MODE_108G);
    }

    netBand = WMI_HOST_REGDMN_MODE_ALL;

    if (!(netBand & HOST_REGDMN_MODE_11A)) {
        netBand &= ~(HOST_REGDMN_MODE_TURBO |
                HOST_REGDMN_MODE_108A |
                HOST_REGDMN_MODE_11A_HALF_RATE);
    }

    if (!(netBand & HOST_REGDMN_MODE_11G)) {
        netBand &= ~(HOST_REGDMN_MODE_108G);
    }
    wMode &= netBand;

    return wMode;
}

/* Vap interface functions */
static int
ol_ath_vap_get_param(struct ieee80211vap *vap,
                              ieee80211_param param)
{
    int retval = 0;
    struct ieee80211com *ic = vap->iv_ic;
    ol_txrx_soc_handle soc_txrx_handle;
    uint8_t vdev_id;
    struct wlan_objmgr_psoc *psoc;
    uint8_t pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);
    cdp_config_param_type value = {0};

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    vdev_id = wlan_vdev_get_id(vap->vdev_obj);

    /* Set the VAP param in the target */
    switch (param) {
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
        case IEEE80211_RAWMODE_PKT_SIM_STATS:
            print_rawmode_pkt_sim_stats(vap);
            break;
#endif
#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
        case IEEE80211_TSO_STATS_RESET_GET:
            cdp_tx_rst_tso_stats(soc_txrx_handle, vdev_id);
            break;

        case IEEE80211_TSO_STATS_GET:
            cdp_tx_print_tso_stats(soc_txrx_handle, vdev_id);
            break;
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */
#if HOST_SW_SG_ENABLE
        case IEEE80211_SG_STATS_GET:
            cdp_tx_print_sg_stats(soc_txrx_handle, vdev_id);
            break;
        case IEEE80211_SG_STATS_RESET_GET:
            cdp_tx_rst_sg_stats(soc_txrx_handle, vdev_id);
            break;
#endif /* HOST_SW_SG_ENABLE */
#if RX_CHECKSUM_OFFLOAD
        case IEEE80211_RX_CKSUM_ERR_STATS_GET:
            cdp_print_rx_cksum_stats(soc_txrx_handle, vdev_id);
	    break;
        case IEEE80211_RX_CKSUM_ERR_RESET_GET:
            cdp_rst_rx_cksum_stats(soc_txrx_handle, vdev_id);
            break;
#endif /* RX_CHECKSUM_OFFLOAD */
        case IEEE80211_RX_FILTER_MONITOR:
           cdp_txrx_get_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_UCAST_DATA, &value);
           retval = value.cdp_pdev_param_fltr_ucast ? 0 : MON_FILTER_TYPE_UCAST_DATA;
           IEEE80211_DPRINTF_IC(vap->iv_ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                     "ucast data filter=%d\n", value.cdp_pdev_param_fltr_ucast);

           cdp_txrx_get_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_MCAST_DATA, &value);
           retval |= value.cdp_pdev_param_fltr_mcast ? 0 : MON_FILTER_TYPE_MCAST_DATA;
           IEEE80211_DPRINTF_IC(vap->iv_ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                      "mcast data filter=%d\n", value.cdp_pdev_param_fltr_mcast);

           cdp_txrx_get_pdev_param(soc_txrx_handle, pdev_id, CDP_FILTER_NO_DATA, &value);
           retval |= value.cdp_pdev_param_fltr_none ? 0 : MON_FILTER_TYPE_NON_DATA;
           IEEE80211_DPRINTF_IC(vap->iv_ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                     "Non data(mgmt/action etc.) filter=%d\n", value.cdp_pdev_param_fltr_none);

           break;
        case IEEE80211_CONFIG_VDEV_PEER_PROTOCOL_COUNT:
            retval = cdp_is_vdev_peer_protocol_count_enabled(soc_txrx_handle,
                                                         vdev_id);
            break;
        case IEEE80211_CONFIG_VDEV_PEER_PROTOCOL_DROP_MASK:
            retval = cdp_get_peer_protocol_drop_mask(soc_txrx_handle, vdev_id);
            break;
#if defined(PORT_SPIRENT_HK) && defined(SPT_ADV_STATS)
        case IEEE80211_CONFIG_GET_RX_RATE:
            retval = ol_ath_calculate_rate(soc_txrx_handle, vdev_id, IEEE80211_STATS_RX);
            break;
        case IEEE80211_CONFIG_GET_TX_RATE:
            retval = ol_ath_calculate_rate(soc_txrx_handle, vdev_id, IEEE80211_STATS_TX);
            break;
#endif
        default:
            /*qdf_nofl_info("%s: VAP param unsupported param:%u value:%u\n", __func__,
                    param, val);*/
            break;
    }

    return(retval);
}

#if defined(PORT_SPIRENT_HK) && defined(SPT_ADV_STATS)
static int
ol_ath_vap_get_adv_stats(struct ieee80211vap *vap, ieee80211_param param, void *adv_stats)
{
    struct ieee80211com *ic = NULL;
    ol_txrx_soc_handle soc_txrx_handle = NULL;
    struct cdp_txrx_stats_req req = {0,};
    struct wlan_objmgr_psoc *psoc = NULL;
    int ret = 0;
	uint8_t vdev_id;
    uint8_t pdev_id = 0;
    if (vap == NULL)
       return -1;

    ic = vap->iv_ic;
    if (ic == NULL)
       return -1;

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    if (psoc == NULL)
        return -1;

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    if (soc_txrx_handle == NULL)
        return -1;

    vdev_id = wlan_vdev_get_id(vap->vdev_obj);
    pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);

    switch (param) {
        case IEEE80211_CONFIG_GET_ADV_STATS:

            ret = cdp_get_vdev_txrx_adv_stats(soc_txrx_handle, vdev_id, (void *)adv_stats);
            req.stats = 0xFFFF;
            cdp_txrx_stats_request(soc_txrx_handle, vdev_id, &req);
            if (pdev_id < MAX_NUM_MACS) {
                cdp_bss_collision_counter(soc_txrx_handle, pdev_id);
                cdp_bss_collision_color(soc_txrx_handle, pdev_id);
            }
            return ret;
		case IEEE80211_CONFIG_GET_REV_TXRX_STATS:
			ret = cdp_get_vdev_basic_txrx_stats(soc_txrx_handle, vdev_id, (void *)adv_stats);
			return ret;
        default:
            break;
    }
    return 0;
}

static int ol_ath_vap_get_rx_ru_values(struct wlan_objmgr_pdev *pdev, int pdev_id, void *ru_rx_stats)
{
    ol_txrx_soc_handle soc_txrx_handle = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;

    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL)
        return -1;

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    if (soc_txrx_handle == NULL)
        return -1;

    return cdp_get_vdev_rx_ru_values(soc_txrx_handle, pdev_id, (void *)ru_rx_stats);
}

int ol_ath_calculate_rate(ol_txrx_soc_handle soc_txrx_handle, uint8_t vdev_id, uint8_t direction)
{
    int ratembps = 0;
    int8_t mcs;
    int8_t nss;
    int8_t pkt_type;
    int8_t bw;
    int8_t sgi;
    uint32_t ratekbps = 0;
	struct iv_vdev_rev_txrx_stats *txrx_stats;
	int retVal = 0;

	txrx_stats = (struct iv_vdev_rev_txrx_stats *)kmalloc(sizeof(struct iv_vdev_rev_txrx_stats), GFP_KERNEL);
	if (txrx_stats != NULL) {
		memset(txrx_stats, 0, sizeof(struct iv_vdev_rev_txrx_stats));
		retVal = cdp_get_vdev_basic_txrx_stats(soc_txrx_handle, vdev_id, (void *)txrx_stats);
		if (retVal != -1) {
			if( direction == IEEE80211_STATS_TX ) {
				mcs = txrx_stats->rev_txrx.tx_mcs;
				nss = txrx_stats->rev_txrx.tx_nss;
				pkt_type = txrx_stats->rev_txrx.tx_pkt_type;
				bw = txrx_stats->rev_txrx.tx_bw;
				sgi = txrx_stats->rev_txrx.tx_sgi;
			} else {
				mcs = txrx_stats->rev_txrx.rx_mcs;
				nss = txrx_stats->rev_txrx.rx_nss;
				pkt_type = txrx_stats->rev_txrx.rx_pkt_type;
				bw = txrx_stats->rev_txrx.rx_bw;
				sgi = txrx_stats->rev_txrx.rx_sgi;
			}

			ratekbps = ol_if_getrateindex_with_sgi(mcs, nss, pkt_type, bw, sgi);
			ratembps = ratekbps/1000;
		}
		kfree(txrx_stats);
	}
    return ratembps;
}
#endif
static int
ol_ath_vap_set_ratemask(struct ieee80211vap *vap, u_int8_t preamble,
                        u_int32_t mask_lower32, u_int32_t mask_higher32,
                        u_int32_t mask_lower32_2)
{
    /* higher 32 bit is reserved for beeliner*/
    switch (preamble) {
        case IEEE80211_LEGACY_PREAMBLE:
            vap->iv_ratemask_default = 0;
            vap->iv_legacy_ratemasklower32 = mask_lower32;
            break;
        case IEEE80211_HT_PREAMBLE:
            vap->iv_ratemask_default = 0;
            vap->iv_ht_ratemasklower32 = mask_lower32;
            break;
        case IEEE80211_VHT_PREAMBLE:
            vap->iv_ratemask_default = 0;
            vap->iv_vht_ratemasklower32 = mask_lower32;
            vap->iv_vht_ratemaskhigher32 = mask_higher32;
            vap->iv_vht_ratemasklower32_2 = mask_lower32_2;
            break;
        case IEEE80211_HE_PREAMBLE:
            vap->iv_ratemask_default = 0;
            vap->iv_he_ratemasklower32 = mask_lower32;
            vap->iv_he_ratemaskhigher32 = mask_higher32;
            vap->iv_he_ratemasklower32_2 = mask_lower32_2;
            break;
        default:
            return EINVAL;
            break;
    }
    return ENETRESET;
}

int
ol_ath_vdev_getpn(struct ieee80211vap *vap, struct ol_ath_softc_net80211 *scn, u_int8_t if_id,
                  u_int8_t *macaddr,
                  uint32_t keytype)
{
    struct peer_request_pn_param pn_param;
    struct wmi_unified *pdev_wmi_handle;
    int ret;
    struct wlan_objmgr_peer *peer;
    struct wlan_objmgr_psoc *psoc;
    uint8_t pdev_id;
    struct wlan_objmgr_vdev *vdev;
    int waitcnt = 0;
    struct ieee80211_node *ni;

    static const uint32_t wmi_ciphermap[] = {
        WMI_CIPHER_WEP,
        WMI_CIPHER_TKIP,
        WMI_CIPHER_AES_OCB,
        WMI_CIPHER_AES_CCM,
#if ATH_SUPPORT_WAPI
        WMI_CIPHER_WAPI,
#else
        0xff,
#endif
        WMI_CIPHER_CKIP,
        WMI_CIPHER_AES_CMAC,
        WMI_CIPHER_AES_CCM,
        WMI_CIPHER_AES_CMAC,
        WMI_CIPHER_AES_GCM,
        WMI_CIPHER_AES_GCM,
        WMI_CIPHER_AES_GMAC,
        WMI_CIPHER_AES_GMAC,
        WMI_CIPHER_NONE,
    };

    psoc = scn->soc->psoc_obj;

    if ((vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, if_id,
        WLAN_CRYPTO_ID)) == NULL) {
         qdf_info("%s: vdev object is NULL ",
                             __func__);
         return -1;
    }

    pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));

    if ((peer = wlan_objmgr_get_peer(psoc, pdev_id, macaddr,
        WLAN_CRYPTO_ID)) == NULL) {
        qdf_info("%s: peer object is NULL", __func__);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_CRYPTO_ID);
        return -1;
    }

    if ((ni = wlan_peer_get_mlme_ext_obj(peer)) == NULL) {
        ret = -1;
        qdf_atomic_init(&(ni->getpn));
        goto bad;
    }

    pn_param.vdev_id = if_id;
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    qdf_mem_copy(pn_param.peer_macaddr, macaddr, 6);

    pn_param.key_type  = wmi_ciphermap[keytype];
    /*
     * In Lithium target, WMI_CIPHER_ANY is introduced after
     * WMI_CIPHER_AES_CMAC. which makes legacy chip is not compatible
     * with new wmi defination. To compensate this new enum addition
     * in legacy, we will decrement the cipher value by one.
     * so it would match with legacy enum values.
     */
    if ((pn_param.key_type > WMI_CIPHER_AES_CMAC) && (ol_target_lithium(scn->soc->psoc_obj) == false)) {
        qdf_print("%s[%d] WAR cipher value will be reduced by 1 %d",
                                 __func__, __LINE__, pn_param.key_type);
        pn_param.key_type--;
    }

    qdf_atomic_init(&(ni->getpn));
    ret = wmi_unified_get_pn_send_cmd(pdev_wmi_handle, &pn_param);

#define PEER_PN_TIMEOUTCNT 5
#define PEER_PN_TIMEOUT 300
    while ( (waitcnt < PEER_PN_TIMEOUTCNT) && (qdf_atomic_read(&(ni->getpn)) == 0)) {
        schedule_timeout_interruptible(qdf_system_msecs_to_ticks(PEER_PN_TIMEOUT));
        waitcnt++;
    }
#undef PEER_PN_TIMEOUTCNT
#undef PEER_PN_TIMEOUT
    if (qdf_atomic_read(&(ni->getpn)) != 1) {
        ret = -1;
        qdf_atomic_init(&(ni->getpn));
        goto bad;
    }

bad:
    wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
    wlan_objmgr_vdev_release_ref(vdev, WLAN_CRYPTO_ID);
    return ret;
}

static QDF_STATUS ol_if_configure_peer_hw_vlan(wlan_if_t vap, struct ieee80211_node *ni)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wmi_unified *pdev_wmi_handle;
    struct peer_vlan_config_param v_param;
    QDF_STATUS status;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&v_param, sizeof(struct peer_vlan_config_param), 0);

    /* Enabling hw vlan acceleration in Rx path through wmi */
    v_param.rx_cmd = 1;
    /* Enabling Rx_insert_inner_vlan_tag */
    v_param.rx_insert_c_tag = 1;

    /* Send Wmi Command */
    status = wmi_unified_peer_vlan_config_send(pdev_wmi_handle, ni->ni_macaddr, &v_param);
    if (status == QDF_STATUS_E_FAILURE)
        return QDF_STATUS_E_FAILURE;

    return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
int
ol_ath_vdev_install_key_send(struct ieee80211vap *vap, struct ol_ath_softc_net80211 *scn, u_int8_t if_id,
                             struct wlan_crypto_key *key,
                             u_int8_t *macaddr,
                             u_int8_t def_keyid, u_int8_t force_none,
                             uint32_t keytype)
#else
int
ol_ath_vdev_install_key_send(struct ieee80211vap *vap, struct ol_ath_softc_net80211 *scn, u_int8_t if_id,
                             struct ieee80211_key *key,
                             u_int8_t *macaddr,
                             u_int8_t def_keyid, u_int8_t force_none,
                             uint32_t keytype)
#endif
{
    struct set_key_params param;
    struct ieee80211_node *ni = NULL;
    int ret =0;
    u_int32_t pn[4]={0,0,0,0};
    u_int32_t  michael_key[2];
    enum cdp_sec_type sec_type = cdp_sec_type_none;
    bool unicast = true;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ieee80211com *ic = NULL;
    uint32_t nss_cipher_idx = 0;
#endif
    ol_txrx_soc_handle soc_txrx_handle;
    struct wlan_objmgr_psoc *psoc;
    enum ieee80211_opmode opmode = ieee80211vap_get_opmode(vap);
    uint8_t vdev_id;

    static const u_int8_t wmi_ciphermap[] = {
        WMI_CIPHER_WEP,
        WMI_CIPHER_TKIP,
        WMI_CIPHER_AES_OCB,
        WMI_CIPHER_AES_CCM,
#if ATH_SUPPORT_WAPI
        WMI_CIPHER_WAPI,
#else
        0xff,
#endif
        WMI_CIPHER_CKIP,
        WMI_CIPHER_AES_CMAC,
        WMI_CIPHER_AES_CCM,
        WMI_CIPHER_AES_CMAC,
        WMI_CIPHER_AES_GCM,
        WMI_CIPHER_AES_GCM,
        WMI_CIPHER_AES_GMAC,
        WMI_CIPHER_AES_GMAC,
        WMI_CIPHER_NONE,
    };
    struct wmi_unified *pdev_wmi_handle;
    cdp_config_param_type val = {0};

     pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
     qdf_mem_zero(&param, sizeof(param));
     param.vdev_id = if_id;
     psoc = scn->soc->psoc_obj;
     soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
     vdev_id = wlan_vdev_get_id(vap->vdev_obj);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
     param.key_len = key->wk_keylen;
#else
     param.key_len = key->keylen;
#endif

     if (force_none == 1) {
         param.key_cipher = WMI_CIPHER_NONE;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
     } else if ((key->wk_flags & IEEE80211_KEY_SWCRYPT) == 0) {
#else
     } else if ((key->flags & IEEE80211_KEY_SWCRYPT) == 0) {
#endif
         KASSERT(keytype <
             (sizeof(wmi_ciphermap)/sizeof(wmi_ciphermap[0])),
            ("invalid cipher type %u", keytype));
         param.key_cipher  = wmi_ciphermap[keytype];
     } else
        param.key_cipher = WMI_CIPHER_NONE;

    switch(param.key_cipher)
    {
        case WMI_CIPHER_TKIP:
            sec_type = cdp_sec_type_tkip;
            break;

        case WMI_CIPHER_AES_CCM:
            sec_type = cdp_sec_type_aes_ccmp;
            break;

        case WMI_CIPHER_WAPI:
            sec_type = cdp_sec_type_wapi;
            break;

        case WMI_CIPHER_AES_GCM:
            sec_type = cdp_sec_type_aes_gcmp;
            break;

        case WMI_CIPHER_WEP:
                /*
                 * All eapol rekey frames are in open mode when 802.1x with dynamic wep is used.
                 * Mark Peer is WEP type, so that, when open+eapol frames received,
                 * DP can check and pass the frames to higher layers
                */

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
            if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_8021X)))
#else
            if (RSN_AUTH_IS_8021X(&vap->iv_rsn))
#endif
            {
                sec_type = cdp_sec_type_wep104;
            }
            break;

        default:
            sec_type = cdp_sec_type_none;
    }

     qdf_mem_copy(param.peer_mac,macaddr,QDF_MAC_ADDR_SIZE);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
     param.key_idx = key->wk_keyix;
     param.key_rsc_counter = key->wk_keyrsc;
     param.key_tsc_counter = key->wk_keytsc;
#if defined(ATH_SUPPORT_WAPI)
     qdf_mem_copy(param.rx_iv, key->wk_recviv, sizeof(key->wk_recviv));
     qdf_mem_copy(param.tx_iv, key->wk_txiv, sizeof(key->wk_txiv));
#endif
     qdf_mem_copy(param.key_data, key->wk_key, key->wk_keylen);

     /* Mapping ieee key flags to WMI key flags */
    if (key->wk_flags & IEEE80211_KEY_GROUP) {
         param.key_flags |= GROUP_USAGE;
         unicast = false;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
         nss_cipher_idx |= NSS_CIPHER_MULTICAST;
#endif
    }

    if ( def_keyid )
         param.key_flags |= TX_USAGE;

    if(vap->iv_opmode == IEEE80211_M_MONITOR) {
         if (key->wk_flags & (IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT | IEEE80211_KEY_SWCRYPT))
            param.key_flags |= PAIRWISE_USAGE;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            nss_cipher_idx |= NSS_CIPHER_MULTICAST;
#endif
    }
    else {
         if (key->wk_flags & (IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT)) {
            param.key_flags |= PAIRWISE_USAGE;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            nss_cipher_idx |= NSS_CIPHER_MULTICAST;
#endif
         }
    }
#else
     param.key_idx = key->keyix;
/* First 8 keyix are used for ucast + igtk + bigtk*/
     if(key->keyix >= 8) {
         param.group_key_idx = ((key->keyix -8)/2) + 1;
         if( key->keyix % 2)
             param.key_idx = 2;
         else
             param.key_idx = 1 ;
     }
     param.key_rsc_counter = key->keyrsc;
     param.key_tsc_counter = key->keytsc;
#if defined(ATH_SUPPORT_WAPI)
     qdf_mem_copy(param.rx_iv, key->recviv, sizeof(key->recviv));
     qdf_mem_copy(param.tx_iv, key->txiv, sizeof(key->txiv));
#endif
     qdf_mem_copy(param.key_data, key->keyval, key->keylen);

     /* Mapping ieee key flags to WMI key flags */
    if (key->flags & WLAN_CRYPTO_KEY_GROUP) {
         param.key_flags |= GROUP_USAGE;
         unicast = false;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
         nss_cipher_idx |= NSS_CIPHER_MULTICAST;
#endif
    }
    if ( def_keyid )
         param.key_flags |= TX_USAGE;

    if(vap->iv_opmode == IEEE80211_M_MONITOR) {
         if (key->flags & (IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT | IEEE80211_KEY_SWCRYPT))
            param.key_flags |= PAIRWISE_USAGE;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            nss_cipher_idx |= NSS_CIPHER_UNICAST;
#endif
    }
    else {
         if (key->flags & (WLAN_CRYPTO_KEY_RECV | WLAN_CRYPTO_KEY_XMIT)) {
            param.key_flags |= PAIRWISE_USAGE;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            nss_cipher_idx |= NSS_CIPHER_UNICAST;
#endif
         }
     }
#endif
    if ((keytype == IEEE80211_CIPHER_TKIP)
         || (keytype == IEEE80211_CIPHER_WAPI)) {
        param.key_rxmic_len = RX_MIC_LENGTH;
        param.key_txmic_len = TX_MIC_LENGTH;
        qdf_mem_copy(michael_key, param.key_data + param.key_len - RX_MIC_LENGTH, RX_MIC_LENGTH);
    }

    /* Target expects key_idx 0 for unicast
       other than static wep cipher.
       For vlan group keyix can be greater base keyix.
       For BIGTK keyix can be 6 or 7 */
    if ((param.key_idx >= (IEEE80211_WEP_NKID + 1)) && !param.group_key_idx ) {
        if (!(wlan_pdev_nif_feat_cap_get(vap->iv_ic->ic_pdev_obj,WLAN_PDEV_F_BEACON_PROTECTION)
                            && ((param.key_idx == 6) || (param.key_idx == 7))))
           param.key_idx = 0;
    }

    qdf_debug("Keyix=%d Keylen=%d Keyflags=%x Cipher=%x ",param.key_idx,param.key_len,param.key_flags,param.key_cipher);
    qdf_debug("macaddr %s",ether_sprintf(macaddr));
#if 0
    {
        uint8_t i;
        qdf_print("Key data");
        for(i=0; i<param.key_len; i++)
            qdf_print("0x%x ",param.key_data[i]);
        qdf_print("\n");
    }
#endif
    /*
     * In Lithium target, WMI_CIPHER_ANY is introduced after
     * WMI_CIPHER_AES_CMAC. which makes legacy chip is not compatible
     * with new wmi defination. To compensate this new enum addition
     * in legacy, we will decrement the cipher value by one.
     * so it would match with legacy enum values.
     */
    if ((param.key_cipher > WMI_CIPHER_AES_CMAC) && (ol_target_lithium(scn->soc->psoc_obj) == false)) {
        qdf_print("%s[%d] WAR cipher value will be reduced by 1 %d",
                                 __func__, __LINE__, param.key_cipher);
        param.key_cipher--;
    }

    val.cdp_vdev_param_cipher_en = sec_type;
    cdp_txrx_set_vdev_param(soc_txrx_handle, vdev_id,
                               CDP_ENABLE_CIPHER, val);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    ic = &scn->sc_ic;
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp, OSIF_NSS_VDEV_SECURITY_TYPE_CFG);
    }
#endif

    ret = wmi_unified_setup_install_key_cmd(pdev_wmi_handle, &param);
    ni = ieee80211_vap_find_node(vap,macaddr,WLAN_MLME_SB_ID);
    if((ni == NULL) || (sec_type == cdp_sec_type_none))
       goto err_ignore_pn;

    /* Need to handle rx_pn for WAPI  */

    if ((opmode == IEEE80211_M_STA) || (ni != vap->iv_bss)) {
        cdp_set_pn_check(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj), ni->ni_macaddr, sec_type, pn);
        cdp_set_key_sec_type(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj), ni->ni_macaddr,sec_type, unicast);

        /* set MIC key for dp layer TKIP defrag */
        if (sec_type == cdp_sec_type_tkip)
            cdp_set_key(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj), ni->ni_macaddr,
                        unicast, michael_key);

        if ((ni->ni_associd) && (ni->is_ft_reauth)) {
            wmi_unified_peer_ft_roam_send(pdev_wmi_handle, ni->ni_macaddr, vap->iv_unit);
            ni->is_ft_reauth = 0;
        }
    }

    if (vap->enable_multi_group_key && ni->vlan_id) {
        /* HW supports insert in Rx for lithium HW and sending the same for
         * legacy targets is no op. So sending configuration unconditionally */
        if (ol_if_configure_peer_hw_vlan(vap, ni) != QDF_STATUS_SUCCESS)
            qdf_err("Failed to configure vlan hw acceleration support for Rx");

        cdp_peer_set_vlan_id(soc_txrx_handle,  wlan_vdev_get_id(vap->vdev_obj),
                macaddr, ni->vlan_id);
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_radio_ops) {
       ic->nss_radio_ops->ic_nss_ol_set_peer_sec_type(scn, macaddr, wlan_vdev_get_id(vap->vdev_obj),
                                         nss_cipher_idx, sec_type, (uint8_t *) michael_key);

       if (vap->enable_multi_group_key && ni->vlan_id)
           ic->nss_radio_ops->ic_nss_ol_peer_set_vlan_id(scn, macaddr, ni->vlan_id);
    }
#endif
err_ignore_pn:
    if(ni)
        ieee80211_free_node(ni, WLAN_MLME_SB_ID);

    /* Zero-out local key variables */
    qdf_mem_zero(&param, sizeof(struct set_key_params));
    return ret;
}

static int
ol_ath_vap_listen(struct ieee80211vap *vap)
{
    /* Target vdev will be in listen state once it is created
     * No need to send any command to target
     */
    return 0;
}

/* No Op for Perf offload */
static int ol_ath_vap_dfs_cac(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
#if OL_ATH_SUPPORT_LED
#if QCA_LTEU_SUPPORT
    if (!wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT)) {
#endif
#if OL_ATH_SUPPORT_LED_POLL
        if (scn->soc->led_blink_rate_table) {
            OS_SET_TIMER(&scn->scn_led_poll_timer, LED_POLL_TIMER);
        }
#else
        OS_CANCEL_TIMER(&scn->scn_led_blink_timer);
        OS_CANCEL_TIMER(&scn->scn_led_poll_timer);
        scn->scn_blinking = OL_BLINK_ON_START;
        if(lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_IPQ4019) {
            ipq4019_wifi_led(scn, OL_LED_OFF);
        } else {
            ol_ath_gpio_output(scn, scn->scn_led_gpio, 0);
        }
        if (scn->soc->led_blink_rate_table) {
            OS_SET_TIMER(&scn->scn_led_blink_timer, 10);
        }
#endif
#if QCA_LTEU_SUPPORT
    }
#endif
#endif /* OL_ATH_SUPPORT_LED */

    if (ol_target_lithium(scn->soc->psoc_obj) && scn->is_scn_stats_timer_init)
        qdf_timer_mod(&(scn->scn_stats_timer), scn->pdev_stats_timer);


    return 0;
}

static int ol_ath_root_authorize(struct ieee80211vap *vap, u_int32_t authorize)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = vap->iv_bss;
    ol_txrx_soc_handle soc_txrx_handle;
    if(!ic || !ni) {
        return -EINVAL;
    }
    scn = OL_ATH_SOFTC_NET80211(ic);
    avn = OL_ATH_VAP_NET80211(ni->ni_vap);

    if(!scn || !avn) {
        return -EINVAL;
    }
    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    cdp_peer_authorize(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj), ni->peer_obj->macaddr,
                       authorize);
    return ol_ath_node_set_param(scn, ni->ni_macaddr, WMI_HOST_PEER_AUTHORIZE, authorize, avn->av_if_id);
}

static int ol_ath_enable_radar_table(struct ieee80211com *ic,
                           struct ieee80211vap *vap, u_int8_t precac,
                            u_int8_t i_dfs)
{
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    bool is_precac_timer_running = false;
    struct ol_ath_softc_net80211 *scn = NULL;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
    uint32_t ignore_dfs = 0;
#endif

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_err(" pdev is null");
        return -1;
    }
    scn = OL_ATH_SOFTC_NET80211(ic);
    psoc = wlan_pdev_get_psoc(pdev);

    if (precac) {
         dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
         if (dfs_rx_ops && dfs_rx_ops->dfs_is_precac_timer_running) {
             if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                     QDF_STATUS_SUCCESS) {
                 return -1;
             }
             dfs_rx_ops->dfs_is_precac_timer_running(pdev,
                     &is_precac_timer_running);
             wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
         }
    }
    /* use the vap bsschan for dfs configure */
    if ((IEEE80211_IS_CHAN_DFS(vap->iv_bsschan) ||
            ((IEEE80211_IS_CHAN_11AC_VHT160(vap->iv_bsschan) ||
              IEEE80211_IS_CHAN_11AC_VHT80_80(vap->iv_bsschan) ||
              IEEE80211_IS_CHAN_11AXA_HE160(vap->iv_bsschan) ||
              IEEE80211_IS_CHAN_11AXA_HE80_80(vap->iv_bsschan))
             && IEEE80211_IS_CHAN_DFS_CFREQ2(vap->iv_bsschan))) ||
            (is_precac_timer_running)) {
        if ((ic->ic_opmode == IEEE80211_M_HOSTAP ||
             ic->ic_opmode == IEEE80211_M_IBSS ||
             (ic->ic_opmode == IEEE80211_M_STA
#if ATH_SUPPORT_STA_DFS
             && ieee80211com_has_cap_ext(ic, IEEE80211_CEXT_STADFS)
#endif
             ))) {
           if (i_dfs) {
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
              dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
              if (dfs_rx_ops && dfs_rx_ops->dfs_is_radar_enabled)
                  dfs_rx_ops->dfs_is_radar_enabled(pdev, &ignore_dfs);

              if(!ignore_dfs)
                  ol_ath_init_and_enable_radar_table(scn);
#else
              ol_ath_init_and_enable_radar_table(scn);
#endif /* HOST_DFS_SPOOF_TEST */
           }
           else {
               ol_ath_init_and_enable_radar_table(scn);
           }
        }
    }

    return 0;
}

int ol_ath_vdev_param_capabilities_set(struct ol_ath_softc_net80211 *scn,
                            struct ol_ath_vap_net80211* avn, uint32_t value)
{
    value |= avn->vdev_param_capabilities;
    if (ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
            wmi_vdev_param_capabilities, value) == 0) {
        avn->vdev_param_capabilities = value;
        return 0;
    }

    return -1;
}

static QDF_STATUS ol_ath_send_prb_rsp_tmpl(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn || !avn->av_pr_rsp_wbuf)
        return QDF_STATUS_E_FAILURE;

    qdf_spin_lock_bh(&avn->avn_lock);
    ol_ath_prb_resp_tmpl_send(scn, avn->av_if_id, vap);
    qdf_spin_unlock_bh(&avn->avn_lock);

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_ath_send_bcn_tmpl(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    qdf_spin_lock_bh(&avn->avn_lock);
    ol_ath_bcn_tmpl_send(scn, avn->av_if_id, vap);
    qdf_spin_unlock_bh(&avn->avn_lock);
    return QDF_STATUS_SUCCESS;
}

#if WLAN_SUPPORT_FILS
static QDF_STATUS ol_ath_send_fd_tmpl(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    qdf_spin_lock_bh(&avn->avn_lock);
    target_if_fd_offload(vdev, avn->av_if_id);
    qdf_spin_unlock_bh(&avn->avn_lock);
    return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_SUPPORT_FILS */

void
ol_ath_prb_rsp_alloc(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;

    if (!vap) {
        qdf_err("IC is NULL");
        return;
    }
    ic = vap->iv_ic;
    if(!ic) {
        qdf_err("IC is NULL");
        return;
    }

    avn = OL_ATH_VAP_NET80211(vap);
    if(!avn) {
        qdf_err("AVN is NULL");
        return;
    }

    qdf_spin_lock_bh(&avn->avn_lock);
    ol_ath_20tu_prb_rsp_alloc(ic, avn->av_if_id);
    qdf_spin_unlock_bh(&avn->avn_lock);
}

static void ol_ath_vap_is_2gvht_en(struct wlan_objmgr_pdev *pdev,
                                   void *obj, void *arg)
{
    struct wlan_objmgr_vdev *vdev = obj;
    struct ieee80211vap *vap;
    struct vdev_mlme_obj *vdev_mlme = NULL;
    uint8_t *is_2gvht_en = (uint8_t *)arg;

    if (!vdev)
        return;

    vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(
                                vdev, WLAN_UMAC_COMP_MLME);
    if (!vdev_mlme)
        return;

    vap = vdev_mlme->ext_vdev_ptr;
    if (!vap)
        return;

    if (ieee80211_vap_256qam_is_set(vap))
        *is_2gvht_en = 1;
}

static int ol_ath_update_phy_mode(struct mlme_channel_param *ch_param,
                                  struct ieee80211com *ic)
{
    struct ieee80211_ath_channel *c = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct wlan_objmgr_pdev *pdev;
    uint8_t is_2gvht_en = 0;

    scn = OL_ATH_SOFTC_NET80211(ic);
    pdev = ic->ic_pdev_obj;
    c = ic->ic_curchan;
    if (!c || !scn || !pdev)
        return -1;

    if (c->ic_freq < 3000) {
        ch_param->phy_mode = WMI_HOST_MODE_11G;
    } else {
        ch_param->phy_mode = WMI_HOST_MODE_11A;
    }

    if (IEEE80211_IS_CHAN_11AXA_HE80_80(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE80_80;
    else if (IEEE80211_IS_CHAN_11AXA_HE160(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE160;
    else if (IEEE80211_IS_CHAN_11AXA_HE80(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE80;
    else if (IEEE80211_IS_CHAN_11AXA_HE40PLUS(c) || IEEE80211_IS_CHAN_11AXA_HE40MINUS(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE40;
    else if (IEEE80211_IS_CHAN_11AXA_HE20(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE20;
    else if (IEEE80211_IS_CHAN_11AC_VHT80_80(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT80_80;
    else  if (IEEE80211_IS_CHAN_11AC_VHT160(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT160;
    else if (IEEE80211_IS_CHAN_11AC_VHT80(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT80;
    else if (IEEE80211_IS_CHAN_11AC_VHT40PLUS(c) || IEEE80211_IS_CHAN_11AC_VHT40MINUS(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT40;
    else if (IEEE80211_IS_CHAN_11AC_VHT20(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT20;
    else if (IEEE80211_IS_CHAN_11NA_HT40PLUS(c) || IEEE80211_IS_CHAN_11NA_HT40MINUS(c))
        ch_param->phy_mode = WMI_HOST_MODE_11NA_HT40;
    else if (IEEE80211_IS_CHAN_11NA_HT20(c))
        ch_param->phy_mode = WMI_HOST_MODE_11NA_HT20;
    else if (IEEE80211_IS_CHAN_11AXG_HE40PLUS(c) || IEEE80211_IS_CHAN_11AXG_HE40MINUS(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE40_2G;
    else if (IEEE80211_IS_CHAN_11AXG_HE20(c))
        ch_param->phy_mode = WMI_HOST_MODE_11AX_HE20_2G;
    else if (IEEE80211_IS_CHAN_11NG_HT40PLUS(c) || IEEE80211_IS_CHAN_11NG_HT40MINUS(c))
        ch_param->phy_mode = WMI_HOST_MODE_11NG_HT40;
    else if (IEEE80211_IS_CHAN_11NG_HT20(c))
        ch_param->phy_mode = WMI_HOST_MODE_11NG_HT20;

    wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
                                      ol_ath_vap_is_2gvht_en,
                                      &is_2gvht_en, 0, WLAN_VDEV_TARGET_IF_ID);

    if (is_2gvht_en) {
        if (ol_target_lithium(scn->soc->psoc_obj)) {
            switch(ch_param->phy_mode) {
                case WMI_HOST_MODE_11NG_HT20:
                    ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT20_2G;
                    break;
                case WMI_HOST_MODE_11NG_HT40:
                    ch_param->phy_mode = WMI_HOST_MODE_11AC_VHT40_2G;
                    break;
                default:
                    break;
            }
        }
    }
    return 0;
}

void wlan_tbtt_sync_timer_start_stop(enum tbtt_sync_timer val)
{
    uint8_t soc_idx;
    ol_ath_soc_softc_t *soc;

    for (soc_idx = 0; soc_idx < ol_num_global_soc; soc_idx++) {
        soc = ol_global_soc[soc_idx];
        if (soc && ol_target_lithium(soc->psoc_obj)) {
            switch(val) {
            case TBTT_SYNC_TIMER_START:
                if (!soc->tbtt_offset_sync_timer_running) {
                    qdf_timer_start(&(soc->tbtt_offset_sync_timer),
                                    DEFAULT_TBTT_SYNC_TIMER);
                    soc->tbtt_offset_sync_timer_running = 1;
                } else {
                    QDF_TRACE(QDF_MODULE_ID_6GHZ, QDF_TRACE_LEVEL_DEBUG,
                              "%s: Tbtt sync timer is running", __func__);
                }
                break;
            case TBTT_SYNC_TIMER_STOP:
                if (soc->tbtt_offset_sync_timer_running) {
                    qdf_timer_stop(&(soc->tbtt_offset_sync_timer));
                    soc->tbtt_offset_sync_timer_running = 0;
                }
                break;
            default: /* No Op */
                break;
            }
        }
    }
}

static QDF_STATUS ol_ath_vap_up_complete(struct wlan_objmgr_vdev *vdev)
{
    struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
    enum ieee80211_opmode opmode;
    struct ieee80211vap *tempvap;
    bool is_fw_cfgd_for_collision_detcn = false;
    bool enable_ap_coll_detn  =
            cfg_get(psoc, CFG_OL_AP_BSS_COLOR_COLLISION_DETECTION);
    bool enable_sta_coll_detn =
            cfg_get(psoc, CFG_OL_STA_BSS_COLOR_COLLISION_DETECTION);
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    opmode = ieee80211vap_get_opmode(vap);
    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    switch (vap->iv_opmode) {
            case IEEE80211_M_HOSTAP:
                /* if user has configured to enable bss color
                 * collision detection by available INI then
                 * only configure FW for bss color detection
                 * on AP
                 */
                if (enable_ap_coll_detn) {
                   /* go throught the vap list to see if we have
                    * already configured fw for color detection
                    */
                    TAILQ_FOREACH(tempvap, &ic->ic_vaps, iv_next) {
                        if(tempvap->iv_he_bsscolor_detcn_configd_vap) {
                            is_fw_cfgd_for_collision_detcn = true;
                            break;
                        }
                    }

                   /* if collision detection in fw is not already
                    * configured then do configuration for this vap
                    */
                    if (!is_fw_cfgd_for_collision_detcn) {
                        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                                QDF_TRACE_LEVEL_INFO,
                                "Configuring fw for AP mode bsscolor "
                                "collision detection for vdev-id: 0x%x",
                                avn->av_if_id);

                        /* configure fw for bsscolor collision detection */
                        ol_ath_config_bsscolor_offload(vap, false);
                        /* register for bsscolor collision detection event */
                        ol_ath_mgmt_register_bsscolor_collision_det_config_event(ic);

                        /* mark the vap for which collision detection
                         * has been configured
                         */
                        vap->iv_he_bsscolor_detcn_configd_vap = true;
                    }
                }

            break;
            case IEEE80211_M_STA:
                /* if user has configured to enable bss color
                 * collision detection by available INI then
                 * only configure FW for bss color detection
                 * on STA
                 */
                if (enable_sta_coll_detn) {
                    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                            QDF_TRACE_LEVEL_INFO,
                            "Configuring fw for STA mode bsscolor "
                            "collision detection for vdev-id: 0x%x",
                            avn->av_if_id);

                    /* configure fw for bsscolor collision detection
                     * and for handling bsscolor change announcement
                     */
                    ol_ath_config_bsscolor_offload(vap, false);
                }
            break;
            default:
                QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                        "Non-ap/non-sta mode of operation. "
                        "No configuration required for BSS Color");
            break;
        }

    avn->av_restart_in_progress = FALSE;

#if QCA_LTEU_SUPPORT
    if (!wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj,
                                    WLAN_SOC_F_LTEU_SUPPORT)) {
#endif
#if OL_ATH_SUPPORT_LED
#if OL_ATH_SUPPORT_LED_POLL
        if (scn->soc->led_blink_rate_table ) {
            OS_SET_TIMER(&scn->scn_led_poll_timer, LED_POLL_TIMER);
        }
#else
        OS_CANCEL_TIMER(&scn->scn_led_blink_timer);
        OS_CANCEL_TIMER(&scn->scn_led_poll_timer);
        if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA5018) ||
            (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6018)) {
            scn->scn_blinking = OL_BLINK_DONE;
        } else {
            scn->scn_blinking = OL_BLINK_ON_START;
        }

        if(lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_IPQ4019) {
            ipq4019_wifi_led(scn, OL_LED_OFF);
        } else {
            ol_ath_gpio_output(scn, scn->scn_led_gpio, 0);
        }
        if (scn->soc->led_blink_rate_table) {
            OS_SET_TIMER(&scn->scn_led_blink_timer, 10);
        }
#endif
#endif /* OL_ATH_SUPPORT_LED */
#if QCA_LTEU_SUPPORT
    }
#endif

#if WLAN_SUPPORT_FILS
    /* allocate fils discovery buffer and send FILS config to FW */
    target_if_fd_reconfig(vap->vdev_obj);
#endif

    if (ol_target_lithium(scn->soc->psoc_obj) && scn->is_scn_stats_timer_init)
        qdf_timer_mod(&(scn->scn_stats_timer), scn->pdev_stats_timer);

    if (opmode == IEEE80211_M_HOSTAP) {
        qdf_timer_mod(&(scn->auth_timer), DEFAULT_AUTH_CLEAR_TIMER);
    }

    if (IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan) &&
        opmode == IEEE80211_M_HOSTAP &&
        !IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
        wlan_tbtt_sync_timer_start_stop(TBTT_SYNC_TIMER_START);
    }

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ol_ath_vap_up_pre_init(struct wlan_objmgr_vdev *vdev, bool restart)
{
    enum ieee80211_opmode opmode;
    int status  = 0;
    ol_txrx_soc_handle soc_txrx_handle;
    bool reassoc = false;
#if MESH_MODE_SUPPORT
    int value = 0;
#endif
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    struct ieee80211_node *ni = NULL;
    cdp_config_param_type val = {0};

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    opmode = ieee80211vap_get_opmode(vap);
    ni = vap->iv_bss;
    if (!ni)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    soc_txrx_handle =
	   wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));

#if SUPPORT_11AX_D3
    /* Update the HEOP flag in IC if the curchan is a 6GHz channel */
    if(IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan)) {
        ic->ic_he.heop_param |=
            (1 << IEEE80211_HEOP_6GHZ_INFO_PRESENT_S);
    }
#endif /* SUPPORT_11AX_D3 */

    switch (opmode) {
        case IEEE80211_M_STA:
            ic->ic_vap_set_param(vap, IEEE80211_VHT_SUBFEE, 0);
            reassoc = restart;
            ol_ath_net80211_newassoc(ni, !reassoc);
            break;
        case IEEE80211_M_HOSTAP:
        case IEEE80211_M_IBSS:
            if (vap->iv_special_vap_mode) {
                if(!vap->iv_smart_monitor_vap &&
                   lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_AR9888) {
                    /* Set Rx decap to RAW mode */
                    vap->iv_rx_decap_type = htt_cmn_pkt_type_raw;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                    if (ic->nss_vops) {
                        ic->nss_vops->ic_osif_nss_vdev_set_cfg(
                                                      (osif_dev *)vap->iv_ifp,
                                                      OSIF_NSS_VDEV_DECAP_TYPE);
                    }
#endif
                    if (ol_ath_pdev_set_param(scn,
                                              wmi_pdev_param_rx_decap_mode,
                                              htt_cmn_pkt_type_raw) != EOK)
                        mlme_err("Error setting rx decap mode to RAW");
                }

                vap->iv_special_vap_is_monitor = 1;
                return QDF_STATUS_E_CANCELED;
            }

            if (vap->iv_enable_vsp) {
                ol_ath_vdev_param_capabilities_set(scn, avn,
                                                   WMI_HOST_VDEV_VOW_ENABLED);
            }
#if MESH_MODE_SUPPORT
            /* If this is a mesh vap and Beacon is enabled for it,
             * send WMI capabiltiy to FW to enable Beacon */
            if(vap->iv_mesh_vap_mode) {
                value = 0;
                if(vap->iv_mesh_cap & MESH_CAP_BEACON_ENABLED) {
                   mlme_info("%s, Enabling Beacon on Mesh Vap (vdev id: %d)",
                             __func__, (OL_ATH_VAP_NET80211(vap))->av_if_id);
                   value = WMI_HOST_VDEV_BEACON_SUPPORT;
                }
                ol_ath_vdev_param_capabilities_set(scn, avn, value);
            }
#endif

            /**
             * For non-offload beacon, free previous deferred beacon buffers
             * in RESTART
             */
            if (restart && !vap->iv_bcn_offload_enable)
               ol_ath_beacon_free(ic, avn->av_if_id);

            /* allocate beacon buffer
             * Move to vap after removing dependencies on avn
             */
            ol_ath_beacon_alloc(ic, avn->av_if_id);

            /**
             * For offload beacon, free previous deferred beacon buffers
             * in RESTART
             */
            if (restart && vap->iv_bcn_offload_enable)
               ol_ath_beacon_free(ic, avn->av_if_id);

            /* If vap coming up is Non Tx vap (in Mbssid set)
             * and if the bcast probe resp buffer is already present
             * and bcast prb rsp is enabled by user, call ieee80211_prb_rsp_alloc_init
             * with Tx vaps avn to update it with this non tx vap info.
             * After updating, send the template.
             */
            if (IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan)) {
                if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
                    struct ieee80211vap *tx_vap;
                    struct ol_ath_vap_net80211 *tx_avn;

                    tx_vap = ic->ic_mbss.transmit_vap;
                    tx_avn = OL_ATH_VAP_NET80211(tx_vap);
                    /* Sanity check for prb rsp buffer, the buffer should be
                     * created by Tx vap as Tx vap comes up before Non Tx vap
                     */
                    if (tx_avn->av_pr_rsp_wbuf && tx_vap->iv_he_6g_bcast_prob_rsp) {
                        tx_avn->av_pr_rsp_wbuf = ieee80211_prb_rsp_alloc_init(ni,
                                                 &tx_avn->av_prb_rsp_offsets);
                        if (tx_avn->av_pr_rsp_wbuf) {
                           if (QDF_STATUS_SUCCESS != ic->ic_prb_rsp_tmpl_send(tx_vap->vdev_obj))
                               qdf_debug("%s:%d: 20TU prb rsp send failed", __func__, __LINE__);
                        }
                    }
                }
             }
#if WLAN_SUPPORT_FILS
            /* allocate fils discovery buffer and send FILS config to FW */
            target_if_fd_reconfig(vdev);
#endif
            ic->ic_vap_set_param(vap, IEEE80211_VHT_SUBFEE, 0);
        break;
    case IEEE80211_M_MONITOR:
        if (vap->iv_smart_monitor_vap) {
            status = cdp_set_monitor_mode(soc_txrx_handle,
                                          wlan_vdev_get_id(vdev),
                                          vap->iv_smart_monitor_vap);
        } else {
            if (vap->iv_lite_monitor) {
                val.cdp_pdev_param_dbg_snf = SNIFFER_M_COPY_MODE;
                status = cdp_txrx_set_pdev_param(
                                         soc_txrx_handle,
                                         wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj),
                                         CDP_CONFIG_DEBUG_SNIFFER,
                                         val);
	    } else {
                status = cdp_set_monitor_mode(
                                          soc_txrx_handle,
                                          wlan_vdev_get_id(vdev),
                                          0);
            }
        }

        if (status) {
            /*Already up, return with correct status*/
            avn->av_restart_in_progress = FALSE;
            mlme_err("Unable to bring up in monitor");
            return QDF_STATUS_E_FAILURE;
        }
    default:
        break;
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp,
                                               OSIF_NSS_VDEV_DECAP_TYPE);
    }
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vdev_set_cfg((osif_dev *)vap->iv_ifp,
                                               OSIF_NSS_VDEV_ENCAP_TYPE);
    }
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vap_up((osif_dev *)vap->iv_ifp);
    }
#endif

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_ath_vap_down(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
#if ATH_SUPPORT_DFS
    struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#endif

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT

    /*
    * Avoid NSS call for non existing vops
    */
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vap_down((osif_dev *)vap->iv_ifp);
    }
#endif

    avn->av_ol_resmgr_wait = FALSE;

    if(!ieee80211_get_num_vaps_up(ic)) {
#if ATH_SUPPORT_DFS
        dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);

	if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) ==
                                             QDF_STATUS_SUCCESS) {
            if (dfs_rx_ops && dfs_rx_ops->dfs_reset_dfs_prevchan)
                dfs_rx_ops->dfs_reset_dfs_prevchan(pdev);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
       }
#endif
        if (ol_target_lithium(scn->soc->psoc_obj) &&
            scn->is_scn_stats_timer_init) {
#ifdef QCA_SUPPORT_ADFS_RCAC
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                    QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
            }
            if (dfs_rx_ops && dfs_rx_ops->dfs_rcac_sm_deliver_evt)
                dfs_rx_ops->dfs_rcac_sm_deliver_evt(pdev,
                        DFS_RCAC_SM_EV_RCAC_STOP);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
#endif /* QCA_SUPPORT_ADFS_RCAC */
            ol_if_dfs_reset_agile_cac(ic);
            qdf_timer_sync_cancel(&(scn->scn_stats_timer));
        }
    }

    if (IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan) &&
        vap->iv_opmode == IEEE80211_M_HOSTAP &&
        !IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
        wlan_tbtt_sync_timer_start_stop(TBTT_SYNC_TIMER_STOP);
    }

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_ath_vap_start_post_init(
                                     struct wlan_objmgr_vdev *vdev,
                                     struct ieee80211_ath_channel *chan,
                                     bool restart)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
#if OBSS_PD
    struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
#endif

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    if (!restart && !qdf_atomic_read(&vap->iv_is_start_sent)) {
        avn->av_ol_resmgr_wait = FALSE;
        return QDF_STATUS_E_FAILURE;
    }
#if OBSS_PD
    ol_ath_send_cfg_obss_spatial_reuse_param(scn, avn->av_if_id);

    /* Ensure that Spatial Reuse is disabled if both AP and STA VAPs
     * exist on same pdev for chipsets other than QCN9000
     */
    if (lmac_get_tgt_type(psoc) != TARGET_TYPE_QCN9000) {
        ol_ath_sr_validate_vap_config(ic, scn, SR_TYPE_NON_SRG_OBSS_PD);
        ol_ath_sr_validate_vap_config(ic, scn, SR_TYPE_SRG_OBSS_PD);
    }
#endif

    /* The channel configured in target is not same always with the
     * vap desired channel due to 20/40 coexistence scenarios,
     * so channel is saved to configure on VDEV START RESP */
    avn->av_ol_resmgr_chan = chan;

    /* Restore dcs state so that interference detection will work */
    if (ic->ic_dcs_restore) {
        ic->ic_dcs_restore(ic);
    }

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_ath_vap_start_pre_init(
                                     struct wlan_objmgr_vdev *vdev,
                                     struct ieee80211_ath_channel *chan,
                                     int restart)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    struct vdev_mlme_obj *vdev_mlme = NULL;
    int chan_mode;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    vdev_mlme = vap->vdev_mlme;
    if (!vdev_mlme)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    chan_mode = ieee80211_chan2mode(chan);
    vdev_mlme->mgmt.generic.phy_mode =
                 ol_get_phymode_info(scn, chan_mode, vap->iv_256qam);

    if (scn->soc->target_status == OL_TRGET_STATUS_EJECT ||
        scn->soc->target_status == OL_TRGET_STATUS_RESET) {
        mlme_info("Target recovery in progress");
        if (!restart)
            wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                               WLAN_VDEV_SM_EV_START_REQ_FAIL,
                                               0, NULL);
        else
            wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                               WLAN_VDEV_SM_EV_RESTART_REQ_FAIL,
                                               0, NULL);

        return QDF_STATUS_E_CANCELED;
    }

    avn->av_ol_resmgr_wait = TRUE;

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_ath_vap_stop_pre_init(struct wlan_objmgr_vdev *vdev)
{
    enum ieee80211_opmode opmode;
    struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
    ol_txrx_soc_handle soc_txrx_handle;
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    bool clear_ic_csa_flags= false;
    struct ieee80211vap *tmp_vap;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    opmode = ieee80211vap_get_opmode(vap);
    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vap);
    if (!avn)
        return QDF_STATUS_E_FAILURE;

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

    switch (opmode) {
    case IEEE80211_M_HOSTAP:
        if (vap->iv_special_vap_mode && vap->iv_special_vap_is_monitor) {
            cdp_reset_monitor_mode(soc_txrx_handle,
                                   wlan_objmgr_pdev_get_pdev_id(pdev),
                                   vap->iv_smart_monitor_vap);
            vap->iv_special_vap_is_monitor = 0;

           /* Enable ol_stats to re-apply the monitor status ring filter
            * if enable_ol_stats is enabled
            */
           if (pdev_cp_stats_ap_stats_tx_cal_enable_get(pdev))
               cdp_enable_enhanced_stats(soc_txrx_handle, wlan_objmgr_pdev_get_pdev_id(pdev));
        }
#ifdef MU_CAP_WAR_ENABLED
        if (vap->iv_mu_cap_war.mu_cap_war == 1)
        {
            MU_CAP_WAR *war = &vap->iv_mu_cap_war;
            qdf_spin_lock_bh(&war->iv_mu_cap_lock);
            if (war->iv_mu_timer_state == MU_TIMER_PENDING)
                war->iv_mu_timer_state = MU_TIMER_STOP;
            OS_CANCEL_TIMER(&war->iv_mu_cap_timer);
            qdf_spin_unlock_bh(&war->iv_mu_cap_lock);
        }
#endif
        if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
            /* Clear CSA vap params if CSA in progress for this vap */
            if (vap->iv_bcn_csa_tmp_sent) {
                vap->iv_bcn_csa_tmp_sent=false;
                vap->channel_switch_state =0;
                vap->iv_chanchange_count=0;
                ieee80211vap_clear_flag(vap, IEEE80211_F_CHANSWITCH);
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
                        "%s : %d Clear vap related CSA flags, vap = %d (%s)\n",
                        __func__, __LINE__, vap->iv_unit, vap->iv_netdev_name);
            }

            /* Do not clear CSA related ic flags if CSA is in progress for any
             * other vap.
             */
            TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
                if (tmp_vap->iv_bcn_csa_tmp_sent) {
                    clear_ic_csa_flags = false;
                    break;
                } else
                    clear_ic_csa_flags = true;
            }

            /* Clear CSA IE params for the last vap stop */
            if (clear_ic_csa_flags) {
                ic->ic_flags_ext2 &= ~IEEE80211_FEXT2_CSA_WAIT;
                ieee80211com_clear_flags(ic, IEEE80211_F_CHANSWITCH);
                IEEE80211_CHANCHANGE_STARTED_CLEAR(ic);
                IEEE80211_CHANCHANGE_BY_BEACONUPDATE_CLEAR(ic);
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
                        "%s : %d Clear ic related CSA flags, vap = %d (%s)\n",
                        __func__, __LINE__, vap->iv_unit, vap->iv_netdev_name);
            }
        }
        break;
    default:
        break;
    }
    /* Cancelling cswitch timer doesn't mean it
     * was ever run, so the CSA flag may need clearing.
     * Clear it when the last vap is going down.
     */
    if (ieee80211_get_num_active_vaps(ic) == 0) {
        ic->ic_flags &= ~IEEE80211_F_CHANSWITCH;
    }

    /* Interface is brought down, So UMAC is not waiting for
     * target response
     */
    avn->av_ol_resmgr_wait = FALSE;

    /*
     * free any pending nbufs in the flow control queue
     */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if(!scn->nss_radio.nss_rctx)
#endif
    {
        cdp_tx_flush_buffers(soc_txrx_handle, wlan_vdev_get_id(vdev));
    }

    /* NOTE: Call the ol_ath_beacon_stop always before sending vdev_stop
     * to Target. ol_ath_beacon_stop puts the beacon buffer to
     * deferred_bcn_list and this beacon buffer gets freed,
     * when stopped event recieved from target. If the ol_ath_beacon_stop
     * called after wmi_unified_vdev_stop_send, then Target could
     * respond with vdev stopped event immidiately and deferred_bcn_list
     * is still be empty and the beacon buffer is not freed.
     */
    ol_ath_beacon_stop(scn, avn);
    if (IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan))
        ol_ath_prb_rsp_stop(scn,avn);

#if WLAN_SUPPORT_FILS
    /* puts fd buffer to deferred_fd_list which gets freed when
     * stopped event is received from target.
     */
    target_if_fd_stop(vdev);
#endif

    /*
     * Start the timer for vap stopped event after ol_ath_beacon_stop
     * puts the beacon buffer in to deferred_bcn_list
     */
    if ((scn->soc->target_status == OL_TRGET_STATUS_EJECT) ||
        (scn->soc->target_status == OL_TRGET_STATUS_RESET)) {
        /* target ejected/reset,  so generate the stopped event */
        wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                           WLAN_VDEV_SM_EV_STOP_RESP,
                                           0, NULL);
        return QDF_STATUS_E_CANCELED;
    }

#if QCA_LTEU_SUPPORT
    if (!wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj,
                                    WLAN_SOC_F_LTEU_SUPPORT)) {
#endif
#if OL_ATH_SUPPORT_LED
        OS_CANCEL_TIMER(&scn->scn_led_blink_timer);
        OS_CANCEL_TIMER(&scn->scn_led_poll_timer);
        scn->scn_blinking = OL_BLINK_STOP;
        if (scn->soc->led_blink_rate_table) {
            OS_SET_TIMER(&scn->scn_led_blink_timer, 10);
        }
#endif
#if QCA_LTEU_SUPPORT
    }
#endif

    return QDF_STATUS_SUCCESS;
}

static void ol_ath_vap_cleanup(struct ieee80211vap *vap)
{
    struct ieee80211vap *tmpvap;
    struct ieee80211com *ic = vap->iv_ic;

#if WLAN_SUPPORT_FILS
        /* Free FILS FD buffer on receiving stopped event */
     target_if_fd_free(vap->vdev_obj);
#endif
     /* 6Ghz 20 tu probe response free */
     if (IEEE80211_IS_CHAN_6GHZ(ic->ic_curchan) && vap->iv_he_6g_bcast_prob_rsp)
         ic->ic_prb_rsp_free(ic, vap->vdev_obj->vdev_objmgr.vdev_id, vap);

     if (vap->iv_he_bsscolor_detcn_configd_vap) {
         /* if bsscolor collision detection
          * event was registered for this
          * vap then disable color detection
          * for this vap
          */
         QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                 QDF_TRACE_LEVEL_INFO,
                 "Disabling bsscolor collision detection "
                 "for stopping vap. vdev-id: 0x%x",
                 OL_ATH_VAP_NET80211(vap)->av_if_id);
         ol_ath_config_bsscolor_offload(vap, true);
         vap->iv_he_bsscolor_detcn_configd_vap = false;

          /* configure collision detection
           * for the next avaailable ap vap
           */
         TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
             if ((tmpvap->iv_opmode == IEEE80211_M_HOSTAP)
                     && tmpvap->iv_is_up) {
                 QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                         QDF_TRACE_LEVEL_INFO,
                         "Configuring fw for AP mode bsscolor "
                         "collision detection for the next "
                         "active vap. vdev-id: 0x%x",
                         OL_ATH_VAP_NET80211(tmpvap)->av_if_id);
                 tmpvap->iv_he_bsscolor_detcn_configd_vap = true;
                 ol_ath_config_bsscolor_offload(tmpvap, false);
                 break;
             }
         }
     }

     /*
      * If MBSS tx VAP has been stopped, reset the bit of non-tx VAPs that
      * indicates if profile has been added for the VAP in MBSS IE of tx VAP's
      * beacon buffer
      */
     if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                    WLAN_PDEV_F_MBSS_IE_ENABLE)) {
         if (!IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
             TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                 if (tmpvap != vap) {
                     qdf_atomic_set(&tmpvap->iv_mbss.iv_added_to_mbss_ie, 0);
                 }
             }
         }
     }

}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
static int
ol_ath_key_alloc(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (scn != NULL) {
        if (k->wk_flags & IEEE80211_KEY_GROUP) {
#if 0
            qdf_print(" Group Keyidx set=%d \n ",k - vap->iv_nw_keys);
#endif
            return k - vap->iv_nw_keys;
        }

        /* target handles key index fetch, host
           returns a key index value which is
           always greater than 0-3 (wep index) */

        if(k->wk_keyix == IEEE80211_KEYIX_NONE) {
            k->wk_keyix= IEEE80211_WEP_NKID + 1;
#if 0
            qdf_print(" Unicast Keyidx set=%d \n ",k->wk_keyix);
#endif
            return k->wk_keyix;
        }

    }
    return -1;
}

/* set the key in the target */
static int
ol_ath_key_set(struct ieee80211vap *vap, struct ieee80211_key *k,
                       const u_int8_t peermac[QDF_MAC_ADDR_SIZE])
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    const struct ieee80211_cipher *cip = k->wk_cipher;
    u_int8_t gmac[QDF_MAC_ADDR_SIZE];
    int opmode, force_cipher_none = 0;
    u_int8_t def_kid_enable = 0;

    ASSERT(cip != NULL);

    if (cip == NULL)
        return 0;

    if (k->wk_keyix == IEEE80211_KEYIX_NONE) {
        qdf_print("%s Not setting Key, keyidx=%u ",__func__,k->wk_keyix);
        return 0;
    }

    IEEE80211_ADDR_COPY(gmac, peermac);

    opmode = ieee80211vap_get_opmode(vap);

    if (k->wk_flags & IEEE80211_KEY_GROUP) {

        switch (opmode) {

        case IEEE80211_M_STA:
#if ATH_SUPPORT_WRAP
            if (avn->av_is_psta && !(avn->av_is_mpsta)){
                qdf_print("%s:Ignore set group key for psta",__func__);
                return 1;
            }
#endif
            /* Setting the multicast key in sta bss (AP) peer entry */
            if (IEEE80211_IS_BROADCAST(gmac)) {
                IEEE80211_ADDR_COPY(gmac,&(vap->iv_bss->ni_macaddr));
            }
            break;

        case IEEE80211_M_HOSTAP:
             /* Setting the multicast key in self i.e AP peer entry */
            IEEE80211_ADDR_COPY(gmac,&vap->iv_myaddr);
            break;
        }

    }
    /* If the key id matches with default tx keyid or privacy is not enabled
     * (First key to be loaded) Then consider this key as the default tx key
     */
    if((cip->ic_cipher == IEEE80211_CIPHER_WEP) &&
           ((vap->iv_def_txkey == k->wk_keyix)
                  || (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY) == 0)))
    {
       def_kid_enable = 1;
    }
    /* Force Cipher to NONE if the vap is configured in RAW mode with cipher as
     * dynamic WEP. For dynamic WEP, null/dummy keys are to be plumbed into the
     * firmware with cipher set to NONE
     */
    if (k->wk_flags & IEEE80211_KEY_DUMMY) {
        force_cipher_none = 1;
    }
   /* send the key to wmi layer  */
   if (ol_ath_vdev_install_key_send(vap, scn, avn->av_if_id, k, gmac, def_kid_enable,0, cip->ic_cipher)) {
       qdf_print("Unable to send the key to target ");
       return -1;
   }
   /* assuming wmi will be always success */
   return 1;
}

static int
ol_ath_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k,
                  struct ieee80211_node *ni)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ieee80211_key tmp_key;
    const struct ieee80211_cipher *cip = k->wk_cipher;
    u_int8_t gmac[QDF_MAC_ADDR_SIZE];

    if (k->wk_keyix == IEEE80211_KEYIX_NONE) {
        qdf_print("%s: Not deleting key, keyidx=%u ",__func__,k->wk_keyix);
        return 0;
    }

    memset(&tmp_key,0,sizeof(struct ieee80211_key));
    tmp_key.wk_valid = k->wk_valid;
    tmp_key.wk_flags = k->wk_flags;
    tmp_key.wk_keyix = k->wk_keyix;
    tmp_key.wk_cipher = k->wk_cipher;
    tmp_key.wk_private = k->wk_private;
    tmp_key.wk_clearkeyix = k->wk_clearkeyix;
    tmp_key.wk_keylen=k->wk_keylen;

    if (ni == NULL) {
        IEEE80211_ADDR_COPY(gmac,&(vap->iv_myaddr));
    } else{
        IEEE80211_ADDR_COPY(gmac, &(ni->ni_macaddr));
    }

    /* send the key to wmi layer  */
    if (ol_ath_vdev_install_key_send(vap, scn, avn->av_if_id,
             (struct ieee80211_key*)(&tmp_key), gmac, 0, 1, cip->ic_cipher)) {
       qdf_print("Unable to send the key to target");
       return -1;
    }
#endif
    /* assuming wmi will be always success */
    return 1;
}
#endif

static void ol_ath_get_vdev_bcn_stats(struct ieee80211vap *vap)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    struct stats_request_params param;
    uint8_t addr[QDF_MAC_ADDR_SIZE];
    wmi_unified_t pdev_wmi_handle;
    qdf_mem_set(&param, sizeof(param), 0);
    qdf_ether_addr_copy(addr, vap->iv_myaddr);
    param.vdev_id = avn->av_if_id;
    param.pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
    param.stats_id = WMI_HOST_REQUEST_BCN_STAT;

    pdev_wmi_handle = lmac_get_pdev_wmi_unified_handle(scn->sc_pdev);

    if (pdev_wmi_handle) {
        wmi_unified_stats_request_send(pdev_wmi_handle, addr, &param);
    } else {
        qdf_err("WMI handle is NULL\n");
    }
}

static void ol_ath_reset_vdev_bcn_stats(struct ieee80211vap *vap)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    struct stats_request_params param;
    uint8_t addr[QDF_MAC_ADDR_SIZE];
    wmi_unified_t pdev_wmi_handle;

    qdf_mem_set(&param, sizeof(param), 0);
    qdf_ether_addr_copy(addr, vap->iv_myaddr);
    param.vdev_id = avn->av_if_id;
    param.pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
    param.stats_id = WMI_HOST_REQUEST_BCN_STAT_RESET;

    pdev_wmi_handle = lmac_get_pdev_wmi_unified_handle(scn->sc_pdev);

    if (pdev_wmi_handle) {
        wmi_unified_stats_request_send(pdev_wmi_handle, addr, &param);
    } else {
        qdf_err("WMI handle is NULL\n");
    }
}

/**
 * ol_vdev_add_dfs_violated_chan_to_nol() - Add dfs violated channel to NOL.
 * If an AP vap tries to come up on a NOL channel, FW sends a failure in
 * vap's start response (DFS_VIOLATION) and this channel is added to NOL.
 * @ic: Pointer to radio object.
 * @chan:Pointer to the ic channel structure.
 */
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
void
ol_vdev_add_dfs_violated_chan_to_nol(struct ieee80211com *ic,
                                     struct ieee80211_ath_channel *chan)
{
    struct wlan_objmgr_pdev *pdev;

    pdev = ic->ic_pdev_obj;
    ieee80211_dfs_channel_mark_radar(ic, chan);
}
#endif

/**
 * ol_vdev_pick_random_chan_and_restart() - Find a random channel and restart
 * vap. This is the action taken when FW sends a dfs violation failure in start
 * response of the vap. A random non-DFS channel is preferred first. If it
 * fails, a random DFS channel is chosen. Irrespective of the state of the
 * vap, all the vaps created are restarted.
 *
 * @vap: Pointer to vap object.
 */

void ol_vdev_pick_random_chan_and_restart(wlan_if_t vap)
{
    struct ieee80211com  *ic;

    ic = vap->iv_ic;
    IEEE80211_CSH_NONDFS_RANDOM_ENABLE(ic);
    ieee80211_dfs_action(vap, NULL, true);
    vap->vap_start_failure = false;
    IEEE80211_CSH_NONDFS_RANDOM_DISABLE(ic);
}

static QDF_STATUS
ol_ath_vap_start_response_event_handler(struct vdev_start_response *rsp,
                                        struct vdev_mlme_obj *vdev_mlme)
{
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211com  *ic;
    wlan_if_t vaphandle;
    struct ol_ath_vap_net80211 *avn;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_vdev *vdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#if defined(QCA_SUPPORT_AGILE_DFS)
    bool is_agile_precac_enabled = false;
#endif
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
    uint32_t dfs_region;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
    struct ieee80211_ath_channel *chan = NULL;
#endif
    uint16_t vdev_id;

    vdev = vdev_mlme->vdev;
    vaphandle = wlan_vdev_get_mlme_ext_obj(vdev);
    if(!vaphandle)
       return QDF_STATUS_E_FAILURE;

    psoc = wlan_vdev_get_psoc(vdev);
    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
    if (!dfs_rx_ops)
       return QDF_STATUS_E_FAILURE;

    avn = OL_ATH_VAP_NET80211(vaphandle);
    scn = avn->av_sc;
    ic = &scn->sc_ic;
    vdev_id = wlan_vdev_get_id(vdev);

    pdev = wlan_vdev_get_pdev(vdev);

    switch (vaphandle->iv_opmode) {

        case IEEE80211_M_MONITOR:
               /* Handle same as HOSTAP */
        case IEEE80211_M_HOSTAP:
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
           /* FW to send CHAN_BLOCKED error code in start resp if host comes
            * up in a DFS channel after spoof test failure. In this case,
            * rebuild ic chan list and restart the vaps with non-DFS chan.
            */
            if (rsp->status == WLAN_MLME_HOST_VDEV_START_CHAN_BLOCKED) {
                if (!ic->ic_rebuilt_chanlist) {
                    if (!ieee80211_dfs_rebuild_chan_list_with_non_dfs_channels(ic)) {
                        ol_vdev_pick_random_chan_and_restart(vaphandle);
                        return QDF_STATUS_E_AGAIN;
                    } else {
                        return QDF_STATUS_E_CANCELED;
                    }
                }
                else {
                    qdf_err("****** channel list is rebuilt ***\n");
                }
                return QDF_STATUS_E_AGAIN;
            }
#endif
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
            reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
            if (reg_rx_ops &&
                wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
                                                          QDF_STATUS_SUCCESS) {
                vaphandle->channel_switch_state = 0;
                return QDF_STATUS_E_FAILURE;
            }
            reg_rx_ops->get_dfs_region(pdev, &dfs_region);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

            if (rsp->status == WLAN_MLME_HOST_VDEV_START_CHAN_DFS_VIOLATION) {
                /* Firmware response states: Channel is invalid due to
                 * DFS Violation.  Following are the action taken:
                 * 1) Add the violated channel to NOL.
                 * 2) Restart vaps with a random channel.
                 * In case of multiple vaps, ensure that action is taken only
                 * once during the first vap's start failure. Remember the
                 * failure event and bypass the action if failure event is
                 * received for other vaps.
                 */
                QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                          "Error %s : failed vdev start vap %d "
                          "status %d\n", __func__,
                          vdev_id, rsp->status);

                /* As we return from vap's start response, release
                 * all the acquired references and locks.
                 */
                vaphandle->channel_switch_state = 0;

                if (!vaphandle->vap_start_failure_action_taken) {
                    /* Why should the following variables be set or reset here?
                     * 1.channel_switch_state: Consider a case where user
                     * tries to send CSA on a NOL channel.This variable will
                     * be set in beacon update and will be cleared only on
                     * successful vap start response. As it is a failure here,
                     * it will not be reset. If not reset, subsequent CSA will
                     * fail in beacon update assuming that the previous CSA is
                     * still in progress.
                     *
                     * 2.vap_start_failure: To mark the state that vap start
                     * has failed so that vdev restart action is done in
                     * dfs_action not via CSA as a CSA in NOL chan is a
                     * violation.
                     */

                    vaphandle->vap_start_failure = true;
                    chan = avn->av_ol_resmgr_chan;
                    ieee80211com_clear_flags(ic, IEEE80211_F_DFS_CHANSWITCH_PENDING);
                    ol_vdev_add_dfs_violated_chan_to_nol(ic, chan);
                    ol_vdev_pick_random_chan_and_restart(vaphandle);

                } else {
                    QDF_TRACE(QDF_MODULE_ID_DEBUG, QDF_TRACE_LEVEL_ERROR,
                              "%s: Vap start failure action taken. "
                              "Ignore the error\n", __func__);
                }
                return QDF_STATUS_E_AGAIN;
            }
#endif
#if defined(QCA_SUPPORT_AGILE_DFS)
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                                             QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }

            if (dfs_rx_ops && dfs_rx_ops->dfs_get_agile_precac_enable)
                dfs_rx_ops->dfs_get_agile_precac_enable(pdev,
                                                        &is_agile_precac_enabled);

            if(is_agile_precac_enabled) {
                if (dfs_rx_ops && dfs_rx_ops->dfs_agile_precac_start)
                    dfs_rx_ops->dfs_agile_precac_start(pdev);
            }
            wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
#endif/* AGILE PRECAC START */

#ifdef QCA_SUPPORT_ADFS_RCAC
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                                             QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
	    if (dfs_rx_ops && dfs_rx_ops->dfs_rcac_sm_deliver_evt)
                dfs_rx_ops->dfs_rcac_sm_deliver_evt(pdev,
                        DFS_RCAC_SM_EV_RCAC_START);
	    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
#endif /* QCA_SUPPORT_ADFS_RCAC */

            /* Resetting the ol_resmgr_wait flag*/
            avn->av_ol_resmgr_wait = FALSE;
            break;
        default:
            break;
    }

    return QDF_STATUS_SUCCESS;
}

/* WMI event handler for Roam events */
static int
ol_ath_vdev_roam_event_handler(
    ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_roam_event evt;
    struct ieee80211vap *vap;
    struct wlan_objmgr_vdev *vdev;
    struct wmi_unified *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (!wmi_handle) {
        qdf_err("wmi_handle is null");
        return -EINVAL;
    }

    if(wmi_extract_vdev_roam_param(wmi_handle, data, &evt)) {
        return -1;
    }
    vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc->psoc_obj, evt.vdev_id, WLAN_MLME_SB_ID);
    if (vdev == NULL) {
        qdf_print("Unable to find vdev for %d vdev_id", evt.vdev_id);
        return -EINVAL;
    }
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap) {
        switch (evt.reason) {
            case WMI_HOST_ROAM_REASON_BMISS:
                ASSERT(vap->iv_opmode == IEEE80211_M_STA);
                ieee80211_mlme_sta_bmiss_ind(vap);
                break;
            case WMI_HOST_ROAM_REASON_BETTER_AP:
                /* FIX THIS */
            default:
                break;
        }
    }
    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

    return 0;
}

/* Device Interface functions */
static void ol_ath_vap_iter_vap_create(void *arg, wlan_if_t vap)
{

    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t *pid_mask = (u_int32_t *) arg;

    u_int8_t myaddr[QDF_MAC_ADDR_SIZE];
    u_int8_t id = 0;
#if ATH_SUPPORT_WRAP
    /* Proxy STA VAP has its own mac address */
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    if (avn->av_is_psta)
        return;
#endif
    ieee80211vap_get_macaddr(vap, myaddr);
    ATH_GET_VAP_ID(myaddr, ic->ic_myaddr, id);
    (*pid_mask) |= (1 << id);
}

#if ATH_SUPPORT_NAC
int
ol_ath_neighbour_rx(struct ieee80211vap *vap, u_int32_t idx,
                   enum ieee80211_nac_param nac_cmd , enum ieee80211_nac_mactype nac_type ,
                   u_int8_t macaddr[QDF_MAC_ADDR_SIZE])
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    u_int32_t action = nac_cmd;
    u_int32_t type = nac_type;
    struct set_neighbour_rx_params param;
    ol_txrx_soc_handle soc_txrx_handle;
    struct wmi_unified *pdev_wmi_handle;
    char nullmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    if (IEEE80211_IS_MULTICAST((uint8_t *)macaddr) ||
           IEEE80211_ADDR_EQ((uint8_t *)macaddr, nullmac)) {
           qdf_print("%s: NAC client / BSSID is invalid", __func__);
           return -1;
    }
    /* For NAC client, we send the client addresses to FW for all platforms.
     * Legacy and HKv2 FW can handle it. For HKv1 FW ignores this command and
     * does not use this address.
     */
    if (type == IEEE80211_NAC_MACTYPE_CLIENT) {
        soc_txrx_handle = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));
        if (nac_cmd == IEEE80211_NAC_PARAM_LIST) {
#if ATH_SUPPORT_NAC_RSSI
            cdp_vdev_get_neighbour_rssi(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj),
                                        macaddr, &vap->iv_nac.client[idx].rssi);
#endif
        } else {
            qdf_mem_set(&param, sizeof(param), 0);
            param.vdev_id = avn->av_if_id;
            param.idx = idx;
            param.action = action;
            param.type = type;
            if (pdev_wmi_handle == NULL) {
                qdf_err("Wmi handle is NULL!!");
                return -1;
            }
            if(wmi_unified_vdev_set_neighbour_rx_cmd_send(pdev_wmi_handle, macaddr, &param)) {
                qdf_err("Unable to send NAC to target");
                return -1;
            }
            cdp_update_filter_neighbour_peers(soc_txrx_handle,
                                              wlan_vdev_get_id(vap->vdev_obj), nac_cmd, macaddr);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC,
                    "%s :vdev =%x, idx=%x, action=%x, macaddr[0][5]=%2x%2x",
                    __func__, param.vdev_id, idx, action, macaddr[0],macaddr[5]);
          }
    } else if (type == IEEE80211_NAC_MACTYPE_BSSID){

	qdf_mem_set(&param, sizeof(param), 0);
	param.vdev_id = avn->av_if_id;
	param.idx = idx;
	param.action = action;
	param.type = type;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC,
            "%s :vdev =%x, idx=%x, action=%x, macaddr[0][5]=%2x%2x",
            __func__, param.vdev_id, idx, action, macaddr[0],macaddr[5]);

    	if(wmi_unified_vdev_set_neighbour_rx_cmd_send(pdev_wmi_handle, macaddr, &param)) {
        	qdf_print("Unable to send neighbor rx command to target");
        	return -1;
    	}
    }

    /* assuming wmi will be always success */
    return 1;
}

int
ol_ath_neighbour_get_max_addrlimit(struct ieee80211vap *vap, enum ieee80211_nac_mactype nac_type)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

	if (!scn || !scn->soc || !scn->soc->psoc_obj)
		return 0;

	if (nac_type == IEEE80211_NAC_MACTYPE_BSSID)
	return ic->ic_nac_bssid;
    else if (nac_type == IEEE80211_NAC_MACTYPE_CLIENT)
	return ic->ic_nac_client;
    else
	return 0;

}
#endif
#if ATH_SUPPORT_WRAP
static inline u_int8_t ol_ath_get_qwrap_num_vdevs(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;
    struct wlan_objmgr_psoc *psoc;

    if (!pdev) {
        qdf_err("null pdev");
        return 0;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (!psoc) {
        qdf_err("null psoc");
        return 0;
    }

    if (cfg_get(scn->soc->psoc_obj, CFG_OL_QWRAP_ENABLE)) {
        return init_deinit_get_qwrap_vdevs_for_pdev_id(psoc,
                lmac_get_pdev_idx(pdev));
    } else {
        return init_deinit_get_total_vdevs_for_pdev_id(psoc,
                lmac_get_pdev_idx(pdev));
    }
}
#endif

#if ATH_SUPPORT_NAC_RSSI
static int
ol_ath_config_for_nac_rssi(struct ieee80211vap *vap,
                   enum ieee80211_nac_rssi_param nac_cmd ,  u_int8_t bssid_macaddr[QDF_MAC_ADDR_SIZE],
                   u_int8_t client_macaddr[QDF_MAC_ADDR_SIZE], u_int8_t chan_num)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    if (nac_cmd == IEEE80211_NAC_RSSI_PARAM_LIST) {
        if (!cdp_vdev_get_neighbour_rssi(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj),
                                    client_macaddr, &vap->iv_nac_rssi.client_rssi))
            vap->iv_nac_rssi.client_rssi_valid = 1;
    } else {
        if (cdp_vdev_config_for_nac_rssi(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj),
                                         nac_cmd, bssid_macaddr, client_macaddr, chan_num)) {
            qdf_nofl_info("Unable to send the scan nac rssi command to target \n");
            return -1;
        }
    }

   return 1;
}

#endif

wlan_if_t osif_get_vap(osif_dev *osifp);

static QDF_STATUS ol_ath_nss_vap_destroy(struct wlan_objmgr_vdev *vdev)
{
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct vdev_osif_priv *vdev_osifp = wlan_vdev_get_ospriv(vdev);
    void *osifp_handle;
    int32_t nss_if = -1;
    enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return QDF_STATUS_E_FAILURE;

    ic = vap->iv_ic;
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    osifp_handle = vdev_osifp->legacy_osif_priv;
    nss_if = ((osif_dev *)osifp_handle)->nss_ifnum;

    if (nss_if && (nss_if != -1) && (nss_if != NSS_PROXY_VAP_IF_NUMBER)) {
        osif_nss_vdev_dealloc(osifp_handle, nss_if);
    }

    if (opmode == QDF_STA_MODE) {
        /* For STAVAP, self peer will get created on FW
         * for accountability, decrement peer count here
         */
        qdf_atomic_inc(&scn->peer_count);
    }
#endif
    return QDF_STATUS_SUCCESS;
}

QDF_STATUS ol_ath_nss_vap_create(struct vdev_mlme_obj *vdev_mlme)
{
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ieee80211vap *vap = vdev_mlme->ext_vdev_ptr;
    struct ieee80211com *ic = vap->iv_ic;
    struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
    struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
    struct vdev_osif_priv *vdev_osifp = wlan_vdev_get_ospriv(vdev);
    enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);
    void *osifp_handle;
    int32_t nss_if = -1;

    osifp_handle = vdev_osifp->legacy_osif_priv;
    if (ic->nss_vops) {
        nss_if = ((osif_dev *)osifp_handle)->nss_ifnum;
        mlme_debug("nss-wifi:#0 VAP# vdev_id %d vap %pK osif %pK nss_if %d ",
                   wlan_vdev_get_id(vdev), vap, osifp_handle, nss_if);
        /*
         * For 11ax radio monitor vap creation in NSS should be avoided
         */
        if (opmode == QDF_MONITOR_MODE && ol_target_lithium(psoc)) {
            ((osif_dev *)osifp_handle)->nss_ifnum = NSS_PROXY_VAP_IF_NUMBER;
        } else {
            if (ic->nss_vops->ic_osif_nss_vap_create(vap, osifp_handle, nss_if) == -1) {
                mlme_err("NSS WiFi Offload Unabled to attach vap ");
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
#endif
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_ath_vap_create_init(struct vdev_mlme_obj *vdev_mlme)
{
    uint32_t target_type;
    struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
    struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
    enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
    struct ieee80211vap *vap = vdev_mlme->ext_vdev_ptr;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    int32_t nss_if = -1;
    struct vdev_osif_priv *vdev_osifp = wlan_vdev_get_ospriv(vdev);
    void *osifp_handle;
#endif

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
        goto ol_ath_vap_create_init_end;

    scn = OL_ATH_SOFTC_NET80211(ic);
    avn = OL_ATH_VAP_NET80211(vap);
    if (!scn || !avn) {
        mlme_err("Invalid input to OL create init");
        goto ol_ath_vap_create_init_end;
    }

    target_type = lmac_get_tgt_type(psoc);
#if ATH_SUPPORT_WRAP
    scn->sc_nwrapvaps = ic->ic_nwrapvaps;
    scn->sc_nscanpsta = ic->ic_nscanpsta;
    scn->sc_npstavaps = ic->ic_npstavaps;
    if (vap->iv_mpsta) {
        qdf_spin_lock_bh(&scn->sc_mpsta_vap_lock);
        scn->sc_mcast_recv_vap = vap;
        qdf_spin_unlock_bh(&scn->sc_mpsta_vap_lock);
    }

    avn->av_is_wrap = vap->iv_wrap;
    avn->av_is_mpsta= vap->iv_mpsta;
    avn->av_is_psta = vap->iv_psta;
    avn->av_use_mat = vap->iv_mat;
    if (vap->iv_mat)
        OS_MEMCPY(avn->av_mat_addr, vap->iv_mat_addr, QDF_MAC_ADDR_SIZE);

    if (vap->iv_psta) {
        if (avn->av_is_mpsta) {
            OS_MEMCPY(avn->av_mat_addr, vap->iv_myaddr, QDF_MAC_ADDR_SIZE);
        }
    }

    /*
     * This is only needed for Peregrine, remove this once we have HW CAP bit added
     * for enhanced ProxySTA support.
     */
    if (target_type == TARGET_TYPE_AR9888) {
        /* enter ProxySTA mode when the first WRAP or PSTA VAP is created */
        if (ic->ic_nwrapvaps + ic->ic_npstavaps == 1)
            (void)ol_ath_pdev_set_param(scn, wmi_pdev_param_proxy_sta_mode, 1);
    }
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops) {
        if (opmode == QDF_MONITOR_MODE && ol_target_lithium(psoc)) {
            /*
             * Avoid NSS interface allocation for 11ax radio in monitor mode.
             *  Monitor mode handled in host.
             */
            nss_if = NSS_PROXY_VAP_IF_NUMBER;
        } else {
            osifp_handle = vdev_osifp->legacy_osif_priv;
            nss_if = ic->nss_vops->ic_osif_nss_vdev_alloc(scn, vap,
                                                          osifp_handle);
            mlme_debug("nss-wifi:#0 VAP# vap %pK  nss_if %d ", vap, nss_if);
            if (nss_if == -1) {
                goto ol_ath_vap_create_init_end;
            }

            ((osif_dev *)osifp_handle)->nss_ifnum = nss_if;
        }
    }
#endif

    if (opmode == QDF_STA_MODE) {
        /* For STAVAP, self peer will get created on FW
         * for accountability, decrement peer count here
         */
        qdf_atomic_dec(&scn->peer_count);
    }

    return QDF_STATUS_SUCCESS;

ol_ath_vap_create_init_end:
    if (avn) {
        qdf_spinlock_destroy(&avn->avn_lock);
    }
    return QDF_STATUS_E_FAILURE;
}

static struct ieee80211vap *
ol_ath_vap_create_pre_init(struct vdev_mlme_obj *vdev_mlme, int flags)
{
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL;
    struct ol_ath_vap_net80211* avn = NULL;
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;
    target_resource_config *tgt_cfg;
    uint32_t target_type;
    uint8_t vlimit_exceeded = false;
#if defined(PORT_SPIRENT_HK) && defined(SPT_MULTI_CLIENTS)
    uint32_t pdev_id;
#endif
    enum QDF_OPMODE opmode;
    void *osifp_handle;
    struct vdev_osif_priv *vdev_osifp = NULL;
    uint8_t max_monitor_count = 0;

    vdev = vdev_mlme->vdev;
    pdev = wlan_vdev_get_pdev(vdev);
    if (!pdev) {
        mlme_err("pdev is null");
        return NULL;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (!psoc) {
        mlme_err("psoc is null");
        return NULL;
    }

    opmode = wlan_vdev_mlme_get_opmode(vdev);
    tgt_cfg = lmac_get_tgt_res_cfg(psoc);
    if (!tgt_cfg) {
        mlme_err("psoc target res cfg is null");
        return NULL;
    }
    target_type = lmac_get_tgt_type(psoc);

    osif_priv = wlan_pdev_get_ospriv(pdev);
    if (osif_priv == NULL) {
        mlme_err("osif_priv is NULL");
        return NULL;
    }

    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    if (ol_ath_target_start(scn->soc)) {
        mlme_err("failed to start the firmware");
        return NULL;
    }

    max_monitor_count = wlan_pdev_get_max_monitor_vdev_count(pdev);

    qdf_spin_lock_bh(&scn->scn_lock);
    if (opmode == QDF_MONITOR_MODE) {
        scn->mon_vdev_count++;
        if (scn->mon_vdev_count > max_monitor_count) {
            qdf_spin_unlock_bh(&scn->scn_lock);
            goto ol_ath_vap_create_pre_init_err;
        }
    } else {
        if (scn->special_ap_vap && !(scn->smart_ap_monitor)) {
            scn->vdev_count++;
            qdf_spin_unlock_bh(&scn->scn_lock);
            goto ol_ath_vap_create_pre_init_err;
        } else {
            if (flags & IEEE80211_SPECIAL_VAP) {

                if ((flags & IEEE80211_SMART_MONITOR_VAP) &&
                    !(scn->smart_ap_monitor)) {
                    scn->smart_ap_monitor = 1;
                } else if ((scn->vdev_count != 0) ||
                           (scn->mon_vdev_count != 0) ) {
                    scn->vdev_count++;
                    qdf_spin_unlock_bh(&scn->scn_lock);
                    goto ol_ath_vap_create_pre_init_err;
                }
                scn->special_ap_vap = 1;
            }
        }
        scn->vdev_count++;
    }
    qdf_spin_unlock_bh(&scn->scn_lock);

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
        goto ol_ath_vap_create_pre_init_err;

    /* AR988X supports at max 16 vaps and all these can be in AP mode */
    if (target_type == TARGET_TYPE_AR9888) {
        if ((scn->vdev_count + scn->mon_vdev_count) > tgt_cfg->num_vdevs) {
            vlimit_exceeded = true;
        }
    } else if (scn->vdev_count > (wlan_pdev_get_max_vdev_count(pdev) - max_monitor_count)) {
	    vlimit_exceeded = true;
    }

    if (vlimit_exceeded) {
        goto ol_ath_vap_create_pre_init_err;
    }

    /* allocate memory for vap structure
     * check if we are recovering or creating the VAP
     *
     * This code should be moved to legacy mlme after avn
     * dependencies are handle
     */
    vdev_osifp = wlan_vdev_get_ospriv(vdev);
    osifp_handle = vdev_osifp->legacy_osif_priv;
    vap = osif_get_vap(osifp_handle);
    if(vap == NULL) {
       /* create the corresponding VAP */
       avn = (struct ol_ath_vap_net80211 *)qdf_mempool_alloc(scn->soc->qdf_dev,
                                                 scn->soc->mempool_ol_ath_vap);
       if (avn == NULL) {
           mlme_err("Can't allocate memory for ath_vap.");
           goto ol_ath_vap_create_pre_init_err;
       }
       wlan_minidump_log(avn, sizeof(*avn), psoc,
                         WLAN_MD_CP_EXT_VDEV, "ol_ath_vap_net80211");
    } else {
       avn = OL_ATH_VAP_NET80211(vap);
    }

    OS_MEMZERO(avn, sizeof(struct ol_ath_vap_net80211));
    avn->av_sc = scn;
    avn->av_if_id = wlan_vdev_get_id(vdev);
    avn->av_restart_in_progress = FALSE;
    qdf_spinlock_create(&avn->avn_lock);
    TAILQ_INIT(&avn->deferred_bcn_list);
    TAILQ_INIT(&avn->deferred_prb_rsp_list);
    vap = &avn->av_vap;

#if ATH_SUPPORT_WRAP
#if defined(PORT_SPIRENT_HK) && defined(SPT_MULTI_CLIENTS)
    pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
    if ((opmode == QDF_STA_MODE) && (((main_proxy_flag0 == 0) && (pdev_id == 0)) || ((main_proxy_flag1 == 0) && (pdev_id == 1))|| ((main_proxy_flag2 == 0) && (pdev_id == 2)))) {
#else
    if ((opmode == QDF_STA_MODE) && (flags & IEEE80211_CLONE_MACADDR)) {
#endif
        if (!(flags & IEEE80211_WRAP_NON_MAIN_STA)) {
#if defined(PORT_SPIRENT_HK) && defined(SPT_MULTI_CLIENTS)
            if (pdev_id == 0) {
                main_proxy_flag0 = 1;
            }
            if (pdev_id == 1) {
                main_proxy_flag1 = 1;
            }
            if (pdev_id == 2) {
                main_proxy_flag2 = 1;
            } 
#endif
            qdf_spin_lock_bh(&scn->sc_mpsta_vap_lock);
            scn->sc_mcast_recv_vap = vap;
            qdf_spin_unlock_bh(&scn->sc_mpsta_vap_lock);
        }
    }
#endif

    if (opmode == QDF_MONITOR_MODE) {
        if (flags & IEEE80211_MONITOR_LITE_VAP) {
            vap->iv_lite_monitor = 1;
            ol_ath_set_debug_sniffer(scn, SNIFFER_M_COPY_MODE);
        }
    }

    return vap;

ol_ath_vap_create_pre_init_err:
    qdf_spin_lock_bh(&scn->scn_lock);
    if (opmode == QDF_MONITOR_MODE) {
        scn->mon_vdev_count--;
    } else {
        scn->vdev_count--;
    }
    qdf_spin_unlock_bh(&scn->scn_lock);
    return NULL;
}

static QDF_STATUS
ol_ath_vap_create_post_init(struct vdev_mlme_obj *vdev_mlme, int flags)
{
    struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
    struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
    enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
    struct ieee80211vap *vap = vdev_mlme->ext_vdev_ptr;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    uint32_t target_type;
    struct vdev_osif_priv *vdev_osifp = NULL;
    uint8_t vdev_id = wlan_vdev_get_id(vdev);
    void *osifp_handle;
    osif_dev *osdev_priv;
    int retval = 0;
    struct wmi_unified *pdev_wmi_handle;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
        return QDF_STATUS_E_FAILURE;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return QDF_STATUS_E_FAILURE;

    psoc = wlan_vdev_get_psoc(vdev);
    vap = vdev_mlme->ext_vdev_ptr;
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
#if ATH_SUPPORT_WRAP
    scn->sc_nstavaps = ic->ic_nstavaps;
#endif

     /*
     * If BMISS offload is supported, we disable the SW Bmiss timer on host for STA vaps.
     * The timer is initialised only for STA vaps.
     */
    if (wmi_service_enabled(pdev_wmi_handle, wmi_service_bcn_miss_offload) &&
        (opmode == QDF_STA_MODE)) {
        u_int32_t tmp_id;
        int8_t tmp_name[] = "tmp";

        tmp_id = ieee80211_mlme_sta_swbmiss_timer_alloc_id(vap, tmp_name);
        ieee80211_mlme_sta_swbmiss_timer_disable(vap, tmp_id);
    }

#ifdef MU_CAP_WAR_ENABLED
    ieee80211_mucap_vattach(vap);
#endif

    /* Intialize VAP interface functions */
    vap->iv_up_pre_init = ol_ath_vap_up_pre_init;
    vap->iv_up_complete = ol_ath_vap_up_complete;
    vap->iv_down = ol_ath_vap_down;
    vap->iv_stop_pre_init = ol_ath_vap_stop_pre_init;
    vap->iv_start_pre_init = ol_ath_vap_start_pre_init;
    vap->iv_start_post_init = ol_ath_vap_start_post_init;
    vap->iv_vap_start_rsp_handler = ol_ath_vap_start_response_event_handler;
    vap->iv_dfs_cac = ol_ath_vap_dfs_cac;
    vap->iv_peer_rel_ref = ol_ath_rel_ref_for_logical_del_peer;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    vap->iv_key_alloc = ol_ath_key_alloc;
    vap->iv_key_delete = ol_ath_key_delete;
    vap->iv_key_set = ol_ath_key_set;
#endif
    vap->iv_root_authorize = ol_ath_root_authorize;
    vap->iv_enable_radar_table = ol_ath_enable_radar_table;
    vap->iv_cleanup = ol_ath_vap_cleanup;
#if ATH_SUPPORT_NAC
    vap->iv_neighbour_rx = ol_ath_neighbour_rx;
    vap->iv_neighbour_get_max_addrlimit = ol_ath_neighbour_get_max_addrlimit;
#endif
#if ATH_SUPPORT_NAC_RSSI
    vap->iv_scan_nac_rssi = ol_ath_config_for_nac_rssi;
#endif
#if ATH_SUPPORT_WRAP
    vap->iv_wrap_mat_tx = ol_if_wrap_mat_tx;
    vap->iv_wrap_mat_rx = ol_if_wrap_mat_rx;
#endif
    if (ol_target_lithium(psoc)) {
        vap->get_vdev_bcn_stats = ol_ath_get_vdev_bcn_stats;
        vap->reset_bcn_stats = ol_ath_reset_vdev_bcn_stats;
    } else {
        vap->get_vdev_bcn_stats = NULL;
        vap->reset_bcn_stats = NULL;
    }

    target_type = lmac_get_tgt_type(psoc);
    vdev_osifp = wlan_vdev_get_ospriv(vdev);
    osifp_handle = vdev_osifp->legacy_osif_priv;
    osdev_priv = (osif_dev *)osifp_handle;

    /* Send Param indicating LP IOT vap as requested by FW */
    if (opmode == QDF_SAP_MODE) {
        if  (flags & IEEE80211_LP_IOT_VAP) {
             if(ol_ath_wmi_send_vdev_param(scn, vdev_id,
                                           wmi_vdev_param_sensor_ap, 1)) {
                mlme_err("Unable to send param LP IOT ");
            }
        }
        /* If Beacon offload service enabled */
        if (ol_ath_is_beacon_offload_enabled(scn->soc)) {
            vap->iv_bcn_offload_enable = 1;
        }
    }


    /*
     * Don't set promiscuous bit in smart monitor vap
     * Smar monitor vap - filters specific to other
     * configured neighbour AP BSSID & its associated clients
     */
    if (vap->iv_special_vap_mode && !vap->iv_smart_monitor_vap) {
        retval = ol_ath_pdev_set_param(scn,
                wmi_pdev_param_set_promisc_mode_cmdid, 1);
        if (retval) {
            mlme_err("Unable to send param promisc_mode");
        }
    }
#if ATH_SUPPORT_DSCP_OVERRIDE
    if (vap->iv_dscp_map_id) {
        ol_ath_set_vap_dscp_tid_map(vap);
    }
#endif

    osdev_priv->wifi3_0_fast_path = 0;

    if ((opmode == QDF_SAP_MODE))
        osdev_priv->wifi3_0_fast_path = 1;

    if ((target_type != TARGET_TYPE_QCA8074V2) &&
        (target_type != TARGET_TYPE_QCA6018) &&
        (target_type != TARGET_TYPE_QCA5018) &&
        (target_type != TARGET_TYPE_QCN9000)) {
            osdev_priv->wifi3_0_fast_path = 0;
    }

#if ATH_SUPPORT_WRAP
    if (vap->iv_wrap) {
        osdev_priv->wifi3_0_fast_path = 0;
    }
#endif

#if MESH_MODE_SUPPORT
    if (vap->iv_mesh_vap_mode) {
        osdev_priv->wifi3_0_fast_path = 0;
    }
#endif

#if ATH_SUPPORT_NAC
    if (vap->iv_smart_monitor_vap) {
        osdev_priv->wifi3_0_fast_path = 0;
    }
#endif

    if (vap->iv_special_vap_mode) {
        osdev_priv->wifi3_0_fast_path = 0;
    }

    /*
     * Register the vap setup functions for offload
     * functions here. */
    osif_vap_setup_ol(vap, osifp_handle);
    return QDF_STATUS_SUCCESS;
}

static void ol_ath_update_vdev_restart_param(struct ieee80211vap *vap,
                                             bool reset,
                                             bool restart_success)
{
    struct ol_ath_vap_net80211 *avn = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    uint8_t restart_reason = ic->ic_restart_reason;

    avn = OL_ATH_VAP_NET80211(vap);

    if (!reset)
            avn->av_ol_resmgr_wait = TRUE;

    if (restart_success)
        avn->av_ol_resmgr_chan = ic->ic_curchan;

    if (restart_success && restart_reason) {
        if (restart_reason & PDEV_CFG_TXCHAINMASK) {
            mlme_info("Updating TX chainmask");
            config_txchainmask(ic, NULL);
        }

        if (restart_reason & PDEV_CFG_RXCHAINMASK) {
            mlme_info("Updating RX chainmask");
            config_rxchainmask(ic, NULL);
        }

        ic->ic_restart_reason = 0;
    }
}

/*
 * VAP free
 */
static void
ol_ath_vap_free(struct ieee80211vap *vap)
{
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    struct ol_ath_softc_net80211 *scn = avn->av_sc;

    wlan_minidump_remove(avn);
    qdf_mempool_free(scn->soc->qdf_dev, scn->soc->mempool_ol_ath_vap, avn);
}

/*
 * VAP delete
 */
static void ol_ath_vap_post_delete(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
#if defined(PORT_SPIRENT_HK) && defined(SPT_MULTI_CLIENTS)
    uint32_t pdev_id;
#endif

    if(ieee80211vap_get_opmode(vap) == IEEE80211_M_STA) {
        qdf_atomic_inc(&scn->peer_count);
    }
#if defined(PORT_SPIRENT_HK) && defined(SPT_MULTI_CLIENTS)
    if (vap->iv_mpsta || (!vap->iv_mpsta && !vap->iv_psta)) {
        pdev_id = lmac_get_pdev_idx(scn->sc_pdev);
            if (pdev_id == 0) {
                main_proxy_flag0 = 0;
            }
            if (pdev_id == 1) {
                main_proxy_flag1 = 0;
            }
            if (pdev_id == 2) {
                main_proxy_flag2 = 0;
            }
    }
#endif

    if(vap->iv_special_vap_mode) {
        vap->iv_special_vap_mode = 0;
        scn->special_ap_vap = 0;
    }

    if (vap->iv_lite_monitor) {
        vap->iv_lite_monitor = 0;
        ol_ath_set_debug_sniffer(scn, SNIFFER_DISABLE);
    }

#if QCN_IE
    if (vap->iv_bpr_enable) {
        vap->iv_bpr_enable = 0;
        ol_ath_set_bpr_wifi3(scn, vap->iv_bpr_enable);
    }
#endif

    /* detach VAP from the procotol stack */
#ifdef MU_CAP_WAR_ENABLED
    ieee80211_mucap_vdetach(vap);
#endif
    ieee80211_vap_detach(vap);
    qdf_spin_lock_bh(&scn->scn_lock);
    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_MONITOR) {
        scn->mon_vdev_count--;
    } else {
        scn->vdev_count--;
    }
    qdf_spin_unlock_bh(&scn->scn_lock);
}

static void
ol_ath_vap_delete(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    uint32_t target_type;

    if (!vdev)
        return;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
        return;

    ic = vap->iv_ic;
    if (!ic)
        return;

    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn)
        return;

    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

#if ATH_SUPPORT_WRAP
    scn->sc_nwrapvaps = ic->ic_nwrapvaps;
    scn->sc_npstavaps = ic->ic_npstavaps;
    scn->sc_nscanpsta = ic->ic_nscanpsta;
    if (vap->iv_psta) {
        qdf_spin_lock_bh(&scn->sc_mpsta_vap_lock);
        if (scn->sc_mcast_recv_vap == vap) {
            scn->sc_mcast_recv_vap = NULL;
        }
        qdf_spin_unlock_bh(&scn->sc_mpsta_vap_lock);
    }

    /* exit ProxySTA mode when the last WRAP or PSTA VAP is deleted */
    if (target_type == TARGET_TYPE_AR9888) {
        /* Only needed for Peregrine */
        if (vap->iv_wrap || vap->iv_psta) {
            if (ic->ic_nwrapvaps + ic->ic_npstavaps == 0) {
                (void)ol_ath_pdev_set_param(scn, wmi_pdev_param_proxy_sta_mode, 0);
            }
        }
    }
#endif

#if ATH_SUPPORT_NAC
    if (vap->iv_smart_monitor_vap) {
        scn->smart_ap_monitor = 0;
    }
#endif
}

/*
 * pre allocate a mac address and return it in bssid
 */
static int
ol_ath_vap_alloc_macaddr(struct ieee80211com *ic, u_int8_t *bssid)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int id = 0, id_mask = 0;
    int nvaps = 0;
   //  DPRINTF(scn, ATH_DEBUG_STATE, "%s \n", __func__);
    /* do a full search to mark all the allocated vaps */
    nvaps = wlan_iterate_vap_list(ic,ol_ath_vap_iter_vap_create,(void *) &id_mask);

    id_mask |= scn->sc_prealloc_idmask; /* or in allocated ids */


    if (IEEE80211_ADDR_IS_VALID(bssid) ) {
        /* request to preallocate a specific address */
        /* check if it is valid and it is available */
        u_int8_t tmp_mac2[QDF_MAC_ADDR_SIZE];
        u_int8_t tmp_mac1[QDF_MAC_ADDR_SIZE];
        IEEE80211_ADDR_COPY(tmp_mac1, ic->ic_my_hwaddr);
        IEEE80211_ADDR_COPY(tmp_mac2, bssid);

        if (ic->ic_is_macreq_enabled(ic)) {
            /* Ignore locally/globally administered bits */
            ATH_SET_VAP_BSSID_MASK_ALTER(tmp_mac1);
            ATH_SET_VAP_BSSID_MASK_ALTER(tmp_mac2);
        } else {
            tmp_mac1[ATH_VAP_ID_INDEX] &= ~(ATH_VAP_ID_MASK >> ATH_VAP_ID_SHIFT);
            if (ATH_VAP_ID_INDEX < (QDF_MAC_ADDR_SIZE - 1))
                tmp_mac1[ATH_VAP_ID_INDEX+1] &= ~( ATH_VAP_ID_MASK << ( OCTET-ATH_VAP_ID_SHIFT ) );

            tmp_mac1[0] |= IEEE802_MAC_LOCAL_ADMBIT ;
            tmp_mac2[ATH_VAP_ID_INDEX] &= ~(ATH_VAP_ID_MASK >> ATH_VAP_ID_SHIFT);
            if (ATH_VAP_ID_INDEX < (QDF_MAC_ADDR_SIZE - 1))
                tmp_mac2[ATH_VAP_ID_INDEX+1] &= ~( ATH_VAP_ID_MASK << ( OCTET-ATH_VAP_ID_SHIFT ) );
        }
        if (!IEEE80211_ADDR_EQ(tmp_mac1,tmp_mac2) ) {
            qdf_print("%s[%d]: Invalid mac address requested %s  ",__func__,__LINE__,ether_sprintf(bssid));
            return -1;
        }
        ATH_GET_VAP_ID(bssid, ic->ic_my_hwaddr, id);

        if ((id_mask & (1 << id)) != 0) {
            qdf_print("%s[%d]:mac address already allocated %s",__func__,__LINE__,ether_sprintf(bssid));
            return -1;
        }
     }
     else {

        for (id = 0; id < ATH_BCBUF; id++) {
             /* get the first available slot */
             if ((id_mask & (1 << id)) == 0)
                 break;
        }
        if (id == ATH_BCBUF) {
           /* no more ids left */
          qdf_print("%s[%d]:No more free slots left ",__func__,__LINE__);
          // DPRINTF(scn, ATH_DEBUG_STATE, "%s No more free slots left \n", __func__);
           return -1;
        }

    }


    /* set the allocated id in to the mask */
    scn->sc_prealloc_idmask |= (1 << id);

    return 0;
}

/*
 * free a  pre allocateed  mac addresses.
 */
static int
ol_ath_vap_free_macaddr(struct ieee80211com *ic, u_int8_t *bssid)
{
    int id = 0;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    ATH_GET_VAP_ID(bssid, ic->ic_my_hwaddr, id);

    /* reset the allocated id in to the mask */
    scn->sc_prealloc_idmask &= ~(1 << id);

    return 0;
}


void
ol_ath_vap_soc_attach(ol_ath_soc_softc_t *soc)
{
    wmi_unified_t wmi_handle;

    wmi_handle = lmac_get_wmi_unified_hdl(soc->psoc_obj);
    /* Register WMI event handlers */
    wmi_unified_register_event_handler(wmi_handle, wmi_roam_event_id,
                                       ol_ath_vdev_roam_event_handler, WMI_RX_UMAC_CTX);
#if ATH_PROXY_NOACK_WAR
    wmi_unified_register_event_handler(wmi_handle, wmi_pdev_reserve_ast_entry_event_id,
                                    ol_ath_pdev_proxy_ast_reserve_event_handler, WMI_RX_UMAC_CTX);
#endif

}
/* Intialization functions */
void
ol_ath_vap_attach(struct ieee80211com *ic)
{
    ic->ic_vap_create_pre_init = ol_ath_vap_create_pre_init;
    ic->ic_vap_create_init = ol_ath_vap_create_init;
    ic->ic_vap_create_post_init = ol_ath_vap_create_post_init;
    ic->ic_nss_vap_create = ol_ath_nss_vap_create;
    ic->ic_nss_vap_destroy = ol_ath_nss_vap_destroy;
    ic->ic_vap_delete = ol_ath_vap_delete;
    ic->ic_vap_post_delete = ol_ath_vap_post_delete;
    ic->ic_vap_free = ol_ath_vap_free;
    ic->ic_vap_alloc_macaddr = ol_ath_vap_alloc_macaddr;
    ic->ic_vap_free_macaddr = ol_ath_vap_free_macaddr;
    ic->ic_vap_set_param = ol_ath_vap_set_param;
    ic->ic_vap_sifs_trigger = ol_ath_vap_sifs_trigger;
    ic->ic_vap_set_ratemask = ol_ath_vap_set_ratemask;
    ic->ic_vap_dyn_bw_rts = ol_ath_vap_dyn_bw_rts;
    ic->ic_ol_net80211_set_mu_whtlist = ol_net80211_set_mu_whtlist;
    ic->ic_vap_get_param = ol_ath_vap_get_param;
#if defined(PORT_SPIRENT_HK) && defined(SPT_ADV_STATS)
    ic->ic_vap_get_adv_stats = ol_ath_vap_get_adv_stats;
    ic->ic_vap_get_rx_ru_values= ol_ath_vap_get_rx_ru_values;
#endif
    ic->ic_vap_set_qdepth_thresh = ol_ath_vap_set_qdepth_thresh;
#if ATH_SUPPORT_WRAP
    ic->ic_get_qwrap_num_vdevs = ol_ath_get_qwrap_num_vdevs;
#endif
#if OBSS_PD
    ic->ic_spatial_reuse = ol_ath_send_derived_obsee_spatial_reuse_param;
    ic->ic_is_spatial_reuse_enabled = ol_ath_is_spatial_reuse_enabled;
#endif
    ic->ic_set_ru26_tolerant = ol_ath_vap_set_ru26_tolerant;
    ic->ic_bcn_tmpl_send = ol_ath_send_bcn_tmpl;
#if WLAN_SUPPORT_FILS
    ic->ic_fd_tmpl_send = ol_ath_send_fd_tmpl;
#endif /* WLAN_SUPPORT_FILS */
    ic->ic_update_phy_mode = ol_ath_update_phy_mode;
    ic->ic_update_restart_param = ol_ath_update_vdev_restart_param;
    ic->ic_prb_rsp_tmpl_send = ol_ath_send_prb_rsp_tmpl;
    ic->ic_prb_rsp_tmpl_alloc = ol_ath_prb_rsp_alloc;
}

/*
 * This API retrieves the vap pointer from object manager
 * it increments the ref count on finding vap. The caller
 * has to decrement ref count with ol_ath_release_vap()
 */
struct ieee80211vap *
ol_ath_pdev_vap_get(struct wlan_objmgr_pdev *pdev, u_int8_t vdev_id)
{

    struct wlan_objmgr_vdev *vdev = NULL;
    struct ieee80211vap *vap;

    if (pdev == NULL) {
       qdf_print("%s:pdev is NULL ", __func__);
       return NULL;
    }
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_MLME_SB_ID) !=
                                             QDF_STATUS_SUCCESS) {
       return NULL;
    }
    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if (vdev == NULL) {
       wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
       QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_INFO_LOW, "%s:vdev is not found (id:%d) \n", __func__, vdev_id);
       return NULL;
    }
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    if (vap == NULL) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }
    return vap;
}
qdf_export_symbol(ol_ath_pdev_vap_get);

/*
 * This API retrieves the vap pointer from object manager
 * it increments the ref count on finding vap. The caller
 * has to decrement ref count with ol_ath_release_vap()
 */
struct ieee80211vap *
ol_ath_vap_get(struct ol_ath_softc_net80211 *scn, u_int8_t vdev_id)
{
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct ieee80211vap *vap;

    if (!scn) {
        qdf_info ("scn is NULL");
        return NULL;
    }
    pdev = scn->sc_pdev;

    if (pdev == NULL) {
       qdf_print("%s:pdev is NULL ", __func__);
       return NULL;
    }
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_MLME_SB_ID) !=
                                             QDF_STATUS_SUCCESS) {
       return NULL;
    }
    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if (vdev == NULL) {
       wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
       QDF_TRACE(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_INFO_LOW, "%s:vdev is not found (id:%d) \n", __func__, vdev_id);
       return NULL;
    }
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    if (vap == NULL) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }
    return vap;
}
qdf_export_symbol(ol_ath_vap_get);

/*
 * Returns the corresponding vap based on vdev
 * Doest not involve looping through vap list to compare the vdevid to get the vap and doesnt
 * consume more CPU cycles.
 * TODO: Try to avoid using ol_ath_vap_get and switch over to ol_ath_getvap to get the vap information.
 */
struct ieee80211vap *
ol_ath_getvap(osif_dev *osdev)
{
    struct ieee80211vap *vap;

    if(osdev->ctrl_vdev == NULL) {
        return NULL;
    }

    vap = wlan_vdev_get_mlme_ext_obj(osdev->ctrl_vdev);

    return vap;
}
qdf_export_symbol(ol_ath_getvap);

u_int8_t
ol_ath_vap_get_myaddr(struct ol_ath_softc_net80211 *scn, u_int8_t vdev_id, u_int8_t *macaddr)
{
    struct wlan_objmgr_pdev *pdev = scn->sc_pdev;
    struct wlan_objmgr_vdev *vdev = NULL;

    if(pdev == NULL) {
       return 0;
    }

    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_MLME_SB_ID) !=
                                            QDF_STATUS_SUCCESS) {
       return 0;
    }
    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    if (vdev == NULL) {
       return 0;
    }
    IEEE80211_ADDR_COPY(macaddr, wlan_vdev_mlme_get_macaddr(vdev));

    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

    return 1;
}
qdf_export_symbol(ol_ath_vap_get_myaddr);

void ol_ath_release_vap(struct ieee80211vap *vap)
{
    struct wlan_objmgr_vdev *vdev = vap->vdev_obj;

    if (vdev == NULL) {
       qdf_print("%s: vdev can't be NULL", __func__);
       QDF_ASSERT(0);
       return;
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}
qdf_export_symbol(ol_ath_release_vap);

void  ol_ath_vap_tx_lock(osif_dev *osdev)
{
	wlan_if_t vap = osdev->os_if;
	struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)(vap->iv_ic);
	ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

	cdp_vdev_tx_lock(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj));
}

void  ol_ath_vap_tx_unlock(osif_dev *osdev)
{
	wlan_if_t vap = osdev->os_if;
	struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)(vap->iv_ic);
	ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

	cdp_vdev_tx_unlock(soc_txrx_handle, wlan_vdev_get_id(vap->vdev_obj));
}

bool ol_ath_is_regulatory_offloaded(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wmi_unified *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    if (!wmi_handle) {
        qdf_err("wmi_handle is null");
        return false;
    }

    if (wmi_service_enabled(wmi_handle, wmi_service_regulatory_db))
        return true;
    else
        return false;
}

int ol_ath_send_ft_roam_start_stop(struct ieee80211vap *vap, uint32_t start)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_ath_vap_net80211 *avn = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = vap->iv_bss;

    if(!ic || !ni) {
        return -EINVAL;
    }

    scn = OL_ATH_SOFTC_NET80211(ic);
    avn = OL_ATH_VAP_NET80211(ni->ni_vap);
    if(!scn || !avn) {
        return -EINVAL;
    }

    return ol_ath_node_set_param(scn, ni->ni_macaddr, WMI_HOST_PEER_PARAM_ENABLE_FT, start, avn->av_if_id);
}


QDF_STATUS
ol_ath_set_pcp_tid_map(ol_txrx_vdev_handle vdev, uint32_t mapid)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    struct vap_pcp_tid_map_params params;
    struct ol_ath_vap_net80211 *avn;
    struct wmi_unified *wmi_hndl;
    osif_dev *osifp = (osif_dev *)vdev;

    vap = ol_ath_getvap(osifp);
    if (!vap)
        return QDF_STATUS_E_INVAL;

    avn = OL_ATH_VAP_NET80211(vap);
    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn || !avn)
        return QDF_STATUS_E_INVAL;

    wmi_hndl = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!wmi_hndl)
        return QDF_STATUS_E_INVAL;

    params.vdev_id = avn->av_if_id;
    if (mapid)
        params.pcp_to_tid_map = vap->iv_pcp_tid_map;
    else
        params.pcp_to_tid_map = ic->ic_pcp_tid_map;

    return wmi_unified_vdev_pcp_tid_map_cmd_send(wmi_hndl, &params);
}
qdf_export_symbol(ol_ath_set_pcp_tid_map);

QDF_STATUS
ol_ath_set_tidmap_prty(ol_txrx_vdev_handle vdev, uint32_t prec_val)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    struct vap_tidmap_prec_params params;
    struct ol_ath_vap_net80211 *avn;
    struct wmi_unified *wmi_hndl;
    osif_dev *osifp = (osif_dev *)vdev;

    vap = ol_ath_getvap(osifp);
    if (!vap)
        return QDF_STATUS_E_INVAL;

    avn = OL_ATH_VAP_NET80211(vap);
    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);
    if (!scn || !avn)
        return QDF_STATUS_E_INVAL;

    wmi_hndl = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!wmi_hndl)
        return QDF_STATUS_E_INVAL;

    params.vdev_id = avn->av_if_id;
    /* Target expects the value to be 1-based */
    params.map_precedence = (prec_val + 1);
    return wmi_unified_vdev_tidmap_prec_cmd_send(wmi_hndl, &params);
}

qdf_export_symbol(ol_ath_set_tidmap_prty);

void ol_ath_update_vap_caps(struct ieee80211vap *vap, struct ieee80211com *ic)
{
    vap->vdev_mlme->proto.generic.nss = ieee80211_getstreams(ic, ic->ic_tx_chainmask);

    vap->iv_he_ul_nss = ieee80211_getstreams(ic, ic->ic_rx_chainmask);

    vap->iv_he_max_nc = HECAP_PHY_MAX_NC_GET_FROM_IC
        ((&(ic->ic_he.hecap_phyinfo
        [IC_HECAP_PHYDWORD_IDX0])));

}
#endif
