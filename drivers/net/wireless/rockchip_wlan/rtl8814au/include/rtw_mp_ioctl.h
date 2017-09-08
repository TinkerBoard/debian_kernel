/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
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
#ifndef _RTW_MP_IOCTL_H_
#define _RTW_MP_IOCTL_H_

#include <mp_custom_oid.h>
#include <rtw_mp.h>

#if 0
#define TESTFWCMDNUMBER			1000000
#define TEST_H2CINT_WAIT_TIME		500
#define TEST_C2HINT_WAIT_TIME		500
#define HCI_TEST_SYSCFG_HWMASK		1
#define _BUSCLK_40M			(4 << 2)
#endif
//------------------------------------------------------------------------------
typedef struct CFG_DBG_MSG_STRUCT {
	u32 DebugLevel;
	u32 DebugComponent_H32;
	u32 DebugComponent_L32;
}CFG_DBG_MSG_STRUCT,*PCFG_DBG_MSG_STRUCT;

typedef struct _RW_REG {
	u32 offset;
	u32 width;
	u32 value;
}mp_rw_reg,RW_Reg, *pRW_Reg;

//for OID_RT_PRO_READ16_EEPROM & OID_RT_PRO_WRITE16_EEPROM
typedef struct _EEPROM_RW_PARAM {
	u32 offset;
	u16 value;
}eeprom_rw_param,EEPROM_RWParam, *pEEPROM_RWParam;

typedef struct _EFUSE_ACCESS_STRUCT_ {
	u16	start_addr;
	u16	cnts;
	u8	data[0];
}EFUSE_ACCESS_STRUCT, *PEFUSE_ACCESS_STRUCT;

typedef struct _BURST_RW_REG {
	u32 offset;
	u32 len;
	u8 Data[256];
}burst_rw_reg,Burst_RW_Reg, *pBurst_RW_Reg;

typedef struct _USB_VendorReq{
	u8	bRequest;
	u16	wValue;
	u16	wIndex;
	u16	wLength;
	u8	u8Dir;//0:OUT, 1:IN
	u8	u8InData;
}usb_vendor_req, USB_VendorReq, *pUSB_VendorReq;

typedef struct _DR_VARIABLE_STRUCT_ {
	u8 offset;
	u32 variable;
}DR_VARIABLE_STRUCT;

//int mp_start_joinbss(_adapter *padapter, NDIS_802_11_SSID *pssid);

//void _irqlevel_changed_(_irqL *irqlevel, /*BOOLEAN*/unsigned char bLower);
#ifdef PLATFORM_OS_XP
static void _irqlevel_changed_(_irqL *irqlevel, u8 bLower)
{

	if (bLower == LOWER) {
		*irqlevel = KeGetCurrentIrql();

		if (*irqlevel > PASSIVE_LEVEL) {
			KeLowerIrql(PASSIVE_LEVEL);
		}
	} else {
		if (KeGetCurrentIrql() == PASSIVE_LEVEL) {
			KeRaiseIrql(DISPATCH_LEVEL, irqlevel);
		}
	}

}
#else
#define _irqlevel_changed_(a,b)
#endif

//oid_rtl_seg_81_80_20

// oid_rtl_seg_87_11_00
NDIS_STATUS oid_rt_pro_read16_eeprom_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_write16_eeprom_hdl (struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro8711_wi_poll_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro8711_pkt_loss_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_rd_attrib_mem_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_wr_attrib_mem_hdl (struct oid_par_priv* poid_par_priv);
NDIS_STATUS  oid_rt_pro_set_rf_intfs_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_poll_rx_status_hdl(struct oid_par_priv* poid_par_priv);
// oid_rtl_seg_87_11_20
NDIS_STATUS oid_rt_pro_cfg_debug_message_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_set_data_rate_ex_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_set_basic_rate_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_read_tssi_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_set_power_tracking_hdl(struct oid_par_priv* poid_par_priv);
//oid_rtl_seg_87_11_50
NDIS_STATUS oid_rt_pro_qry_pwrstate_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_set_pwrstate_hdl(struct oid_par_priv* poid_par_priv);
//oid_rtl_seg_87_11_F0
NDIS_STATUS oid_rt_pro_h2c_set_rate_table_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_h2c_get_rate_table_hdl(struct oid_par_priv* poid_par_priv);


//oid_rtl_seg_87_12_00
NDIS_STATUS oid_rt_pro_encryption_ctrl_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_add_sta_info_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_dele_sta_info_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_query_dr_variable_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_rx_packet_type_hdl(struct oid_par_priv* poid_par_priv);

NDIS_STATUS oid_rt_pro_read_efuse_hdl(struct oid_par_priv *poid_par_priv);
NDIS_STATUS oid_rt_pro_write_efuse_hdl(struct oid_par_priv *poid_par_priv);
NDIS_STATUS oid_rt_pro_rw_efuse_pgpkt_hdl(struct oid_par_priv *poid_par_priv);
NDIS_STATUS oid_rt_get_efuse_current_size_hdl(struct oid_par_priv *poid_par_priv);
NDIS_STATUS oid_rt_pro_efuse_hdl(struct oid_par_priv *poid_par_priv);
NDIS_STATUS oid_rt_pro_efuse_map_hdl(struct oid_par_priv *poid_par_priv);

NDIS_STATUS oid_rt_set_crystal_cap_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_set_rx_packet_type_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_get_efuse_max_size_hdl(struct oid_par_priv* poid_par_priv);
NDIS_STATUS oid_rt_pro_set_tx_agc_offset_hdl(struct oid_par_priv* poid_par_priv);

NDIS_STATUS oid_rt_pro_set_pkt_test_mode_hdl(struct oid_par_priv* poid_par_priv);

NDIS_STATUS oid_rt_get_thermal_meter_hdl(struct oid_par_priv* poid_par_priv);

NDIS_STATUS oid_rt_set_power_down_hdl(struct oid_par_priv* poid_par_priv);

NDIS_STATUS oid_rt_get_power_mode_hdl(struct oid_par_priv* poid_par_priv);

extern struct oid_obj_priv oid_rtl_seg_81_80_00[32];
extern struct oid_obj_priv oid_rtl_seg_81_80_20[16];
extern struct oid_obj_priv oid_rtl_seg_81_80_40[6];
extern struct oid_obj_priv oid_rtl_seg_81_80_80[3];

extern struct oid_obj_priv oid_rtl_seg_81_85[1];
extern struct oid_obj_priv oid_rtl_seg_81_87[5];

extern struct oid_obj_priv oid_rtl_seg_87_11_00[32];
extern struct oid_obj_priv oid_rtl_seg_87_11_20[5];
extern struct oid_obj_priv oid_rtl_seg_87_11_50[2];
extern struct oid_obj_priv oid_rtl_seg_87_11_80[1];
extern struct oid_obj_priv oid_rtl_seg_87_11_B0[1];
extern struct oid_obj_priv oid_rtl_seg_87_11_F0[16];

extern struct oid_obj_priv oid_rtl_seg_87_12_00[32];

struct rwreg_param{
	u32 offset;
	u32 width;
	u32 value;
};

struct bbreg_param{
	u32 offset;
	u32 phymask;
	u32 value;
};
/*
struct rfchannel_param{
	u32 ch;
	u32 modem;
};
*/
struct txpower_param{
	u32 pwr_index;
};


struct datarate_param{
	u32 rate_index;
};


struct rfintfs_parm {
	u32 rfintfs;
};

typedef struct _mp_xmit_parm_ {
	u8 enable;
	u32 count;
	u16 length;
	u8 payload_type;
	u8 da[ETH_ALEN];
}MP_XMIT_PARM, *PMP_XMIT_PARM;

struct mp_xmit_packet {
	u32 len;
	u32 mem[MAX_MP_XMITBUF_SZ >> 2];
};

struct psmode_param {
	u32 ps_mode;
	u32 smart_ps;
};

//for OID_RT_PRO_READ16_EEPROM & OID_RT_PRO_WRITE16_EEPROM
struct eeprom_rw_param {
	u32 offset;
	u16 value;
};

struct mp_ioctl_handler {
	u32 paramsize;
	u32 (*handler)(struct oid_par_priv* poid_par_priv);
	u32 oid;
};

struct mp_ioctl_param{
	u32 subcode;
	u32 len;
	u8 data[0];
};

u32 mp_ioctl_xmit_packet_hdl(struct oid_par_priv* poid_par_priv);

#ifdef _RTW_MP_IOCTL_C_

#define GEN_MP_IOCTL_HANDLER(sz, hdl, oid) {sz, hdl, oid},

#define EXT_MP_IOCTL_HANDLER(sz, subcode, oid) {sz, mp_ioctl_ ## subcode ## _hdl, oid},

#else /* _RTW_MP_IOCTL_C_ */

#endif /* _RTW_MP_IOCTL_C_ */

#endif

