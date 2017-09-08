/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#define _RTW_DEBUG_C_

#include <drv_types.h>
#include <hal_data.h>

u32 GlobalDebugLevel = _drv_err_;

#ifdef CONFIG_DEBUG_RTL871X

	u64 GlobalDebugComponents = \
			_module_rtl871x_xmit_c_ |
			_module_xmit_osdep_c_ |
			_module_rtl871x_recv_c_ |
			_module_recv_osdep_c_ |
			_module_rtl871x_mlme_c_ |
			_module_mlme_osdep_c_ |
			_module_rtl871x_sta_mgt_c_ |
			_module_rtl871x_cmd_c_ |
			_module_cmd_osdep_c_ |
			_module_rtl871x_io_c_ |
			_module_io_osdep_c_ |
			_module_os_intfs_c_|
			_module_rtl871x_security_c_|
			_module_rtl871x_eeprom_c_|
			_module_hal_init_c_|
			_module_hci_hal_init_c_|
			_module_rtl871x_ioctl_c_|
			_module_rtl871x_ioctl_set_c_|
			_module_rtl871x_ioctl_query_c_|
			_module_rtl871x_pwrctrl_c_|
			_module_hci_intfs_c_|
			_module_hci_ops_c_|
			_module_hci_ops_os_c_|
			_module_rtl871x_ioctl_os_c|
			_module_rtl8712_cmd_c_|
			_module_hal_xmit_c_|
			_module_rtl8712_recv_c_ |
			_module_mp_ |
			_module_efuse_;

#endif /* CONFIG_DEBUG_RTL871X */

#include <rtw_version.h>

void dump_drv_version(void *sel)
{
	DBG_871X_SEL_NL(sel, "%s %s\n", DRV_NAME, DRIVERVERSION);
}

void dump_drv_cfg(void *sel)
{
	char *kernel_version = utsname()->release;

	DBG_871X_SEL_NL(sel, "\nKernel Version: %s\n", kernel_version);
	DBG_871X_SEL_NL(sel, "Driver Version: %s\n", DRIVERVERSION);
	DBG_871X_SEL_NL(sel, "------------------------------------------------\n");
#ifdef CONFIG_IOCTL_CFG80211
	DBG_871X_SEL_NL(sel, "CFG80211\n");
	#ifdef RTW_USE_CFG80211_STA_EVENT
	DBG_871X_SEL_NL(sel, "RTW_USE_CFG80211_STA_EVENT\n");
	#endif
#else
	DBG_871X_SEL_NL(sel, "WEXT\n");
#endif

	DBG_871X_SEL_NL(sel, "DBG:%d\n", DBG);
#ifdef CONFIG_DEBUG
	DBG_871X_SEL_NL(sel, "CONFIG_DEBUG\n");
#endif

#ifdef CONFIG_CONCURRENT_MODE
	DBG_871X_SEL_NL(sel, "CONFIG_CONCURRENT_MODE\n");
#endif

#ifdef CONFIG_POWER_SAVING
	DBG_871X_SEL_NL(sel, "CONFIG_POWER_SAVING\n");
#endif

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	DBG_871X_SEL_NL(sel, "LOAD_PHY_PARA_FROM_FILE - REALTEK_CONFIG_PATH=%s\n", REALTEK_CONFIG_PATH);
	#ifdef CONFIG_CALIBRATE_TX_POWER_BY_REGULATORY
	DBG_871X_SEL_NL(sel, "CONFIG_CALIBRATE_TX_POWER_BY_REGULATORY\n");
	#endif
	#ifdef CONFIG_CALIBRATE_TX_POWER_TO_MAX
	DBG_871X_SEL_NL(sel, "CONFIG_CALIBRATE_TX_POWER_TO_MAX\n");
	#endif
#endif

#ifdef CONFIG_DISABLE_ODM
	DBG_871X_SEL_NL(sel, "CONFIG_DISABLE_ODM\n");
#endif

#ifdef CONFIG_MINIMAL_MEMORY_USAGE
	DBG_871X_SEL_NL(sel, "CONFIG_MINIMAL_MEMORY_USAGE\n");
#endif

	DBG_871X_SEL_NL(sel, "CONFIG_RTW_ADAPTIVITY_EN = %d\n", CONFIG_RTW_ADAPTIVITY_EN);
#if (CONFIG_RTW_ADAPTIVITY_EN)
	DBG_871X_SEL_NL(sel, "ADAPTIVITY_MODE = %s\n", (CONFIG_RTW_ADAPTIVITY_MODE) ? "carrier_sense" : "normal");
#endif

	#ifdef CONFIG_USB_TX_AGGREGATION
	DBG_871X_SEL_NL(sel, "CONFIG_USB_TX_AGGREGATION\n");
	#endif
	#ifdef CONFIG_USB_RX_AGGREGATION
	DBG_871X_SEL_NL(sel, "CONFIG_USB_RX_AGGREGATION\n");
	#endif
	#ifdef CONFIG_USE_USB_BUFFER_ALLOC_TX
	DBG_871X_SEL_NL(sel, "CONFIG_USE_USB_BUFFER_ALLOC_TX\n");
	#endif
	#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	DBG_871X_SEL_NL(sel, "CONFIG_USE_USB_BUFFER_ALLOC_RX\n");
	#endif
	#ifdef CONFIG_PREALLOC_RECV_SKB
	DBG_871X_SEL_NL(sel, "CONFIG_PREALLOC_RECV_SKB\n");
	#endif
	#ifdef CONFIG_FIX_NR_BULKIN_BUFFER
	DBG_871X_SEL_NL(sel, "CONFIG_FIX_NR_BULKIN_BUFFER\n");
	#endif


	DBG_871X_SEL_NL(sel, "MAX_XMITBUF_SZ = %d\n", MAX_XMITBUF_SZ);
	DBG_871X_SEL_NL(sel, "MAX_RECVBUF_SZ = %d\n", MAX_RECVBUF_SZ);

}

void dump_log_level(void *sel)
{
	DBG_871X_SEL_NL(sel, "log_level:%d\n", GlobalDebugLevel);
}

void mac_reg_dump(void *sel, _adapter *adapter)
{
	int i, j = 1;

	DBG_871X_SEL_NL(sel, "======= MAC REG =======\n");

	for(i=0x0;i<0x800;i+=4)
	{
		if(j%4==1)
			DBG_871X_SEL_NL(sel, "0x%03x",i);
		DBG_871X_SEL(sel, " 0x%08x ", rtw_read32(adapter,i));
		if((j++)%4 == 0)
			DBG_871X_SEL(sel, "\n");
	}

#ifdef CONFIG_RTL8814A
	{
		for(i=0x1000;i<0x1650;i+=4)
		{
			if(j%4==1)
				DBG_871X_SEL_NL(sel, "0x%03x",i);
			DBG_871X_SEL(sel, " 0x%08x ", rtw_read32(adapter,i));
			if((j++)%4 == 0)
				DBG_871X_SEL(sel, "\n");
		}
	}
#endif /* CONFIG_RTL8814A */
}

void bb_reg_dump(void *sel, _adapter *adapter)
{
	int i, j = 1;

	DBG_871X_SEL_NL(sel, "======= BB REG =======\n");
	for(i=0x800;i<0x1000;i+=4)
	{
		if(j%4==1)
			DBG_871X_SEL_NL(sel, "0x%03x",i);
		DBG_871X_SEL(sel, " 0x%08x ", rtw_read32(adapter,i));
		if((j++)%4 == 0)
			DBG_871X_SEL(sel, "\n");
	}
}

void rf_reg_dump(void *sel, _adapter *adapter)
{
	int i, j = 1, path;
	u32 value;
	u8 rf_type = 0;
	u8 path_nums = 0;

	rtw_hal_get_hwreg(adapter, HW_VAR_RF_TYPE, (u8 *)(&rf_type));
	if((_RF_1T2R == rf_type) ||(_RF_1T1R ==rf_type ))
		path_nums = 1;
	else
		path_nums = 2;

	DBG_871X_SEL_NL(sel, "======= RF REG =======\n");

	for (path=0;path<path_nums;path++) {
		DBG_871X_SEL_NL(sel, "RF_Path(%x)\n",path);
		for (i=0;i<0x100;i++) {
			value = rtw_hal_read_rfreg(adapter, path, i, 0xffffffff);
			if(j%4==1)
				DBG_871X_SEL_NL(sel, "0x%02x ",i);
			DBG_871X_SEL(sel, " 0x%08x ",value);
			if((j++)%4==0)
				DBG_871X_SEL(sel, "\n");
		}
	}
}

static u8 fwdl_test_chksum_fail = 0;
static u8 fwdl_test_wintint_rdy_fail = 0;

bool rtw_fwdl_test_trigger_chksum_fail(void)
{
	if (fwdl_test_chksum_fail) {
		DBG_871X_LEVEL(_drv_always_, "fwdl test case: trigger chksum_fail\n");
		fwdl_test_chksum_fail--;
		return _TRUE;
	}
	return _FALSE;
}

bool rtw_fwdl_test_trigger_wintint_rdy_fail(void)
{
	if (fwdl_test_wintint_rdy_fail) {
		DBG_871X_LEVEL(_drv_always_, "fwdl test case: trigger wintint_rdy_fail\n");
		fwdl_test_wintint_rdy_fail--;
		return _TRUE;
	}
	return _FALSE;
}

static u32 g_wait_hiq_empty_ms = 0;

u32 rtw_get_wait_hiq_empty_ms(void)
{
	return g_wait_hiq_empty_ms;
}

static u8 del_rx_ampdu_test_no_tx_fail = 0;

bool rtw_del_rx_ampdu_test_trigger_no_tx_fail(void)
{
	if (del_rx_ampdu_test_no_tx_fail) {
		DBG_871X_LEVEL(_drv_always_, "del_rx_ampdu test case: trigger no_tx_fail\n");
		del_rx_ampdu_test_no_tx_fail--;
		return _TRUE;
	}
	return _FALSE;
}

void rtw_sink_rtp_seq_dbg( _adapter *adapter,_pkt *pkt)
{
	struct recv_priv *precvpriv = &(adapter->recvpriv);
	if( precvpriv->sink_udpport > 0)
	{
		if(*((u16*)((pkt->data)+0x24)) == cpu_to_be16(precvpriv->sink_udpport))
		{
			precvpriv->pre_rtp_rxseq= precvpriv->cur_rtp_rxseq;
			precvpriv->cur_rtp_rxseq = be16_to_cpu(*((u16*)((pkt->data)+0x2C)));
			if( precvpriv->pre_rtp_rxseq+1 != precvpriv->cur_rtp_rxseq)
				DBG_871X("%s : RTP Seq num from %d to %d\n",__FUNCTION__,precvpriv->pre_rtp_rxseq,precvpriv->cur_rtp_rxseq);
		}
	}
}

void sta_rx_reorder_ctl_dump(void *sel, struct sta_info *sta)
{
	struct recv_reorder_ctrl *reorder_ctl;
	int i;

	for (i = 0; i < 16; i++) {
		reorder_ctl = &sta->recvreorder_ctrl[i];
		if (reorder_ctl->ampdu_size != RX_AMPDU_SIZE_INVALID || reorder_ctl->indicate_seq != 0xFFFF) {
			DBG_871X_SEL_NL(sel, "tid=%d, enable=%d, ampdu_size=%u, indicate_seq=%u\n"
				, i, reorder_ctl->enable, reorder_ctl->ampdu_size, reorder_ctl->indicate_seq
			);
		}
	}
}

void dump_adapters_status(void *sel, struct dvobj_priv *dvobj)
{
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(dvobj);
	int i;
	_adapter *iface;
	u8 u_ch, u_bw, u_offset;

	DBG_871X_SEL_NL(sel, "%-2s %-8s %-17s %-4s %-7s %s\n"
		, "id", "ifname", "macaddr", "port", "ch", "status");

	DBG_871X_SEL_NL(sel, "------------------------------------------\n");

	for (i = 0; i < dvobj->iface_nums; i++) {
		iface = dvobj->padapters[i];
		if (iface) {
			DBG_871X_SEL_NL(sel, "%2d %-8s "MAC_FMT" %4hhu %3u,%u,%u "MLME_STATE_FMT" %s%s\n"
				, i, ADPT_ARG(iface)
				, MAC_ARG(adapter_mac_addr(iface))
				, get_iface_type(iface)
				, iface->mlmeextpriv.cur_channel
				, iface->mlmeextpriv.cur_bwmode
				, iface->mlmeextpriv.cur_ch_offset
				, ADPT_MLME_S_ARG(iface)
				, rtw_is_surprise_removed(iface)?" SR":""
				, rtw_is_drv_stopped(iface)?" DS":""
			);
		}
	}

	DBG_871X_SEL_NL(sel, "------------------------------------------\n");

	rtw_get_ch_setting_union(dvobj->padapters[IFACE_ID0], &u_ch, &u_bw, &u_offset);
	DBG_871X_SEL_NL(sel, "%34s %3u,%u,%u\n"
		, "union:"
		, u_ch, u_bw, u_offset
	);

	DBG_871X_SEL_NL(sel, "%34s %3u,%u,%u\n"
		, "oper:"
		, dvobj->oper_channel
		, dvobj->oper_bwmode
		, dvobj->oper_ch_offset
	);

	#ifdef CONFIG_DFS_MASTER
	if (rfctl->radar_detect_ch != 0) {
		DBG_871X_SEL_NL(sel, "%34s %3u,%u,%u"
			, "radar_detect:"
			, rfctl->radar_detect_ch
			, rfctl->radar_detect_bw
			, rfctl->radar_detect_offset
		);

		if (IS_UNDER_CAC(rfctl))
			DBG_871X_SEL(sel, ", cac:%d\n", rtw_systime_to_ms(rfctl->cac_end_time - rtw_get_current_time()));
		else
			DBG_871X_SEL(sel, "\n");
	}
	#endif
}

#define SEC_CAM_ENT_ID_TITLE_FMT "%-2s"
#define SEC_CAM_ENT_ID_TITLE_ARG "id"
#define SEC_CAM_ENT_ID_VALUE_FMT "%2u"
#define SEC_CAM_ENT_ID_VALUE_ARG(id) (id)

#define SEC_CAM_ENT_TITLE_FMT "%-6s %-17s %-32s %-3s %-7s %-2s %-2s %-5s"
#define SEC_CAM_ENT_TITLE_ARG "ctrl", "addr", "key", "kid", "type", "MK", "GK", "valid"
#define SEC_CAM_ENT_VALUE_FMT "0x%04x "MAC_FMT" "KEY_FMT" %3u %-7s %2u %2u %5u"
#define SEC_CAM_ENT_VALUE_ARG(ent) \
	(ent)->ctrl \
	, MAC_ARG((ent)->mac) \
	, KEY_ARG((ent)->key) \
	, ((ent)->ctrl) & 0x03 \
	, security_type_str((((ent)->ctrl) >> 2) & 0x07) \
	, (((ent)->ctrl) >> 5) & 0x01 \
	, (((ent)->ctrl) >> 6) & 0x01 \
	, (((ent)->ctrl) >> 15) & 0x01

void dump_sec_cam_ent(void *sel, struct sec_cam_ent *ent, int id)
{
	if (id >= 0) {
		DBG_871X_SEL_NL(sel, SEC_CAM_ENT_ID_VALUE_FMT " " SEC_CAM_ENT_VALUE_FMT"\n"
			, SEC_CAM_ENT_ID_VALUE_ARG(id), SEC_CAM_ENT_VALUE_ARG(ent));
	} else {
		DBG_871X_SEL_NL(sel, SEC_CAM_ENT_VALUE_FMT"\n", SEC_CAM_ENT_VALUE_ARG(ent));
	}
}

void dump_sec_cam_ent_title(void *sel, u8 has_id)
{
	if (has_id) {
		DBG_871X_SEL_NL(sel, SEC_CAM_ENT_ID_TITLE_FMT " " SEC_CAM_ENT_TITLE_FMT"\n"
			, SEC_CAM_ENT_ID_TITLE_ARG, SEC_CAM_ENT_TITLE_ARG);
	} else {
		DBG_871X_SEL_NL(sel, SEC_CAM_ENT_TITLE_FMT"\n", SEC_CAM_ENT_TITLE_ARG);
	}
}

void dump_sec_cam(void *sel, _adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct cam_ctl_t *cam_ctl = &dvobj->cam_ctl;
	struct sec_cam_ent ent;
	int i;

	DBG_871X_SEL_NL(sel, "HW sec cam:\n");
	dump_sec_cam_ent_title(sel, 1);
	for (i = 0; i < cam_ctl->num; i++) {
		rtw_sec_read_cam_ent(adapter, i, (u8 *)(&ent.ctrl), ent.mac, ent.key);
		dump_sec_cam_ent(sel , &ent, i);
	}
}

