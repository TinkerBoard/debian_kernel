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
#define _IOCTL_LINUX_C_

#include <drv_types.h>
#include <rtw_mp.h>
#include <rtw_mp_ioctl.h>
#include "../../hal/phydm/phydm_precomp.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
#define  iwe_stream_add_event(a, b, c, d, e)  iwe_stream_add_event(b, c, d, e)
#define  iwe_stream_add_point(a, b, c, d, e)  iwe_stream_add_point(b, c, d, e)
#endif

#ifdef CONFIG_80211N_HT
extern int rtw_ht_enable;
#endif


#define RTL_IOCTL_WPA_SUPPLICANT	SIOCIWFIRSTPRIV+30

#define SCAN_ITEM_SIZE 768
#define MAX_CUSTOM_LEN 64
#define RATE_COUNT 4

#ifdef CONFIG_GLOBAL_UI_PID
extern int ui_pid[3];
#endif

// combo scan
#define WEXT_CSCAN_AMOUNT 9
#define WEXT_CSCAN_BUF_LEN		360
#define WEXT_CSCAN_HEADER		"CSCAN S\x01\x00\x00S\x00"
#define WEXT_CSCAN_HEADER_SIZE		12
#define WEXT_CSCAN_SSID_SECTION		'S'
#define WEXT_CSCAN_CHANNEL_SECTION	'C'
#define WEXT_CSCAN_NPROBE_SECTION	'N'
#define WEXT_CSCAN_ACTV_DWELL_SECTION	'A'
#define WEXT_CSCAN_PASV_DWELL_SECTION	'P'
#define WEXT_CSCAN_HOME_DWELL_SECTION	'H'
#define WEXT_CSCAN_TYPE_SECTION		'T'


extern u8 key_2char2num(u8 hch, u8 lch);
extern u8 str_2char2num(u8 hch, u8 lch);
extern void macstr2num(u8 *dst, u8 *src);
extern u8 convert_ip_addr(u8 hch, u8 mch, u8 lch);

u32 rtw_rates[] = {1000000,2000000,5500000,11000000,
	6000000,9000000,12000000,18000000,24000000,36000000,48000000,54000000};

static const char * const iw_operation_mode[] =
{
	"Auto", "Ad-Hoc", "Managed",  "Master", "Repeater", "Secondary", "Monitor"
};

static int hex2num_i(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex2byte_i(const char *hex)
{
	int a, b;
	a = hex2num_i(*hex++);
	if (a < 0)
		return -1;
	b = hex2num_i(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

/**
 * hwaddr_aton - Convert ASCII string to MAC address
 * @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 */
static int hwaddr_aton_i(const char *txt, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++) {
		int a, b;

		a = hex2num_i(*txt++);
		if (a < 0)
			return -1;
		b = hex2num_i(*txt++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
		if (i < 5 && *txt++ != ':')
			return -1;
	}

	return 0;
}

static void indicate_wx_custom_event(_adapter *padapter, char *msg)
{
	u8 *buff, *p;
	union iwreq_data wrqu;

	if (strlen(msg) > IW_CUSTOM_MAX) {
		DBG_871X("%s strlen(msg):%zu > IW_CUSTOM_MAX:%u\n", __FUNCTION__ , strlen(msg), IW_CUSTOM_MAX);
		return;
	}

	buff = rtw_zmalloc(IW_CUSTOM_MAX+1);
	if(!buff)
		return;

	_rtw_memcpy(buff, msg, strlen(msg));

	_rtw_memset(&wrqu,0,sizeof(wrqu));
	wrqu.data.length = strlen(msg);

	DBG_871X("%s %s\n", __FUNCTION__, buff);
#ifndef CONFIG_IOCTL_CFG80211
	wireless_send_event(padapter->pnetdev, IWEVCUSTOM, &wrqu, buff);
#endif

	rtw_mfree(buff, IW_CUSTOM_MAX+1);

}


static void request_wps_pbc_event(_adapter *padapter)
{
	u8 *buff, *p;
	union iwreq_data wrqu;


	buff = rtw_malloc(IW_CUSTOM_MAX);
	if(!buff)
		return;

	_rtw_memset(buff, 0, IW_CUSTOM_MAX);

	p=buff;

	p+=sprintf(p, "WPS_PBC_START.request=TRUE");

	_rtw_memset(&wrqu,0,sizeof(wrqu));

	wrqu.data.length = p-buff;

	wrqu.data.length = (wrqu.data.length<IW_CUSTOM_MAX) ? wrqu.data.length:IW_CUSTOM_MAX;

	DBG_871X("%s\n", __FUNCTION__);

#ifndef CONFIG_IOCTL_CFG80211
	wireless_send_event(padapter->pnetdev, IWEVCUSTOM, &wrqu, buff);
#endif

	if(buff)
	{
		rtw_mfree(buff, IW_CUSTOM_MAX);
	}

}

#ifdef CONFIG_SUPPORT_HW_WPS_PBC
void rtw_request_wps_pbc_event(_adapter *padapter)
{
#ifdef RTK_DMP_PLATFORM
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,12))
	kobject_uevent(&padapter->pnetdev->dev.kobj, KOBJ_NET_PBC);
#else
	kobject_hotplug(&padapter->pnetdev->class_dev.kobj, KOBJ_NET_PBC);
#endif
#else

	if ( padapter->pid[0] == 0 )
	{	//	0 is the default value and it means the application monitors the HW PBC doesn't privde its pid to driver.
		return;
	}

#endif

	rtw_led_control(padapter, LED_CTL_START_WPS_BOTTON);
}
#endif//#ifdef CONFIG_SUPPORT_HW_WPS_PBC

void indicate_wx_scan_complete_event(_adapter *padapter)
{
	union iwreq_data wrqu;
	struct	mlme_priv *pmlmepriv = &padapter->mlmepriv;

	_rtw_memset(&wrqu, 0, sizeof(union iwreq_data));

	//DBG_871X("+rtw_indicate_wx_scan_complete_event\n");
#ifndef CONFIG_IOCTL_CFG80211
	wireless_send_event(padapter->pnetdev, SIOCGIWSCAN, &wrqu, NULL);
#endif
}


void rtw_indicate_wx_assoc_event(_adapter *padapter)
{
	union iwreq_data wrqu;
	struct	mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX		*pnetwork = (WLAN_BSSID_EX*)(&(pmlmeinfo->network));

	_rtw_memset(&wrqu, 0, sizeof(union iwreq_data));

	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	if(check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)==_TRUE )
		_rtw_memcpy(wrqu.ap_addr.sa_data, pnetwork->MacAddress, ETH_ALEN);
	else
		_rtw_memcpy(wrqu.ap_addr.sa_data, pmlmepriv->cur_network.network.MacAddress, ETH_ALEN);

	DBG_871X_LEVEL(_drv_always_, "assoc success\n");
#ifndef CONFIG_IOCTL_CFG80211
	wireless_send_event(padapter->pnetdev, SIOCGIWAP, &wrqu, NULL);
#endif
}

void rtw_indicate_wx_disassoc_event(_adapter *padapter)
{
	union iwreq_data wrqu;

	_rtw_memset(&wrqu, 0, sizeof(union iwreq_data));

	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	_rtw_memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);

#ifndef CONFIG_IOCTL_CFG80211
	DBG_871X_LEVEL(_drv_always_, "indicate disassoc\n");
	wireless_send_event(padapter->pnetdev, SIOCGIWAP, &wrqu, NULL);
#endif
}

/*
uint	rtw_is_cckrates_included(u8 *rate)
{
		u32	i = 0;

		while(rate[i]!=0)
		{
			if  (  (((rate[i]) & 0x7f) == 2)	|| (((rate[i]) & 0x7f) == 4) ||
			(((rate[i]) & 0x7f) == 11)  || (((rate[i]) & 0x7f) == 22) )
			return _TRUE;
			i++;
		}

		return _FALSE;
}

uint	rtw_is_cckratesonly_included(u8 *rate)
{
	u32 i = 0;

	while(rate[i]!=0)
	{
			if  (  (((rate[i]) & 0x7f) != 2) && (((rate[i]) & 0x7f) != 4) &&
				(((rate[i]) & 0x7f) != 11)  && (((rate[i]) & 0x7f) != 22) )
			return _FALSE;
			i++;
	}

	return _TRUE;
}
*/

static int search_p2p_wfd_ie(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop)
{
#ifdef CONFIG_P2P
	struct wifidirect_info	*pwdinfo = &padapter->wdinfo;
#ifdef CONFIG_WFD
	if ( SCAN_RESULT_ALL == pwdinfo->wfd_info->scan_result_type )
	{

	}
	else if ( ( SCAN_RESULT_P2P_ONLY == pwdinfo->wfd_info->scan_result_type ) ||
		      ( SCAN_RESULT_WFD_TYPE == pwdinfo->wfd_info->scan_result_type ) )
#endif // CONFIG_WFD
	{
		if(!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
		{
			u32	blnGotP2PIE = _FALSE;

			//	User is doing the P2P device discovery
			//	The prefix of SSID should be "DIRECT-" and the IE should contains the P2P IE.
			//	If not, the driver should ignore this AP and go to the next AP.

			//	Verifying the SSID
			if ( _rtw_memcmp( pnetwork->network.Ssid.Ssid, pwdinfo->p2p_wildcard_ssid, P2P_WILDCARD_SSID_LEN ) )
			{
				u32	p2pielen = 0;

				//	Verifying the P2P IE
				if (rtw_get_p2p_ie_from_scan_queue(&pnetwork->network.IEs[0], pnetwork->network.IELength, NULL, &p2pielen, pnetwork->network.Reserved[0]))
				{
					blnGotP2PIE = _TRUE;
				}
			}

			if ( blnGotP2PIE == _FALSE )
			{
				return _FALSE;
			}

		}
	}

#ifdef CONFIG_WFD
	if ( SCAN_RESULT_WFD_TYPE == pwdinfo->wfd_info->scan_result_type )
	{
		u32	blnGotWFD = _FALSE;
		u8	wfd_ie[ 128 ] = { 0x00 };
		uint	wfd_ielen = 0;

		if ( rtw_get_wfd_ie_from_scan_queue( &pnetwork->network.IEs[0], pnetwork->network.IELength,  wfd_ie, &wfd_ielen, pnetwork->network.Reserved[0]) )
		{
			u8	wfd_devinfo[ 6 ] = { 0x00 };
			uint	wfd_devlen = 6;

			if ( rtw_get_wfd_attr_content( wfd_ie, wfd_ielen, WFD_ATTR_DEVICE_INFO, wfd_devinfo, &wfd_devlen) )
			{
				if ( pwdinfo->wfd_info->wfd_device_type == WFD_DEVINFO_PSINK )
				{
					//	the first two bits will indicate the WFD device type
					if ( ( wfd_devinfo[ 1 ] & 0x03 ) == WFD_DEVINFO_SOURCE )
					{
						//	If this device is Miracast PSink device, the scan reuslt should just provide the Miracast source.
						blnGotWFD = _TRUE;
					}
				}
				else if ( pwdinfo->wfd_info->wfd_device_type == WFD_DEVINFO_SOURCE )
				{
					//	the first two bits will indicate the WFD device type
					if ( ( wfd_devinfo[ 1 ] & 0x03 ) == WFD_DEVINFO_PSINK )
					{
						//	If this device is Miracast source device, the scan reuslt should just provide the Miracast PSink.
						//	Todo: How about the SSink?!
						blnGotWFD = _TRUE;
					}
				}
			}
		}

		if ( blnGotWFD == _FALSE )
		{
			return _FALSE;
		}
	}
#endif // CONFIG_WFD

#endif //CONFIG_P2P
	return _TRUE;
}
 static inline char *iwe_stream_mac_addr_proess(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
	/*  AP MAC address  */
	iwe->cmd = SIOCGIWAP;
	iwe->u.ap_addr.sa_family = ARPHRD_ETHER;

	_rtw_memcpy(iwe->u.ap_addr.sa_data, pnetwork->network.MacAddress, ETH_ALEN);
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_ADDR_LEN);
	return start;
}
 static inline char * iwe_stream_essid_proess(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{

	/* Add the ESSID */
	iwe->cmd = SIOCGIWESSID;
	iwe->u.data.flags = 1;
	iwe->u.data.length = min((u16)pnetwork->network.Ssid.SsidLength, (u16)32);
	start = iwe_stream_add_point(info, start, stop, iwe, pnetwork->network.Ssid.Ssid);
	return start;
}

 static inline char * iwe_stream_chan_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
	if(pnetwork->network.Configuration.DSConfig<1 /*|| pnetwork->network.Configuration.DSConfig>14*/)
		pnetwork->network.Configuration.DSConfig = 1;

	 /* Add frequency/channel */
	iwe->cmd = SIOCGIWFREQ;
	iwe->u.freq.m = rtw_ch2freq(pnetwork->network.Configuration.DSConfig) * 100000;
	iwe->u.freq.e = 1;
	iwe->u.freq.i = pnetwork->network.Configuration.DSConfig;
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_FREQ_LEN);
	return start;
}
 static inline char * iwe_stream_mode_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe,u16 cap)
{
	/* Add mode */
	if(cap & (WLAN_CAPABILITY_IBSS |WLAN_CAPABILITY_BSS)){
		iwe->cmd = SIOCGIWMODE;
		if (cap & WLAN_CAPABILITY_BSS)
			iwe->u.mode = IW_MODE_MASTER;
		else
			iwe->u.mode = IW_MODE_ADHOC;

		start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_UINT_LEN);
	}
	return start;
 }
 static inline char * iwe_stream_encryption_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe,u16 cap)
{

	/* Add encryption capability */
	iwe->cmd = SIOCGIWENCODE;
	if (cap & WLAN_CAPABILITY_PRIVACY)
		iwe->u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe->u.data.flags = IW_ENCODE_DISABLED;
	iwe->u.data.length = 0;
	start = iwe_stream_add_point(info, start, stop, iwe, pnetwork->network.Ssid.Ssid);
	return start;

}

 static inline char * iwe_stream_protocol_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
 {
 	u16 ht_cap=_FALSE,vht_cap = _FALSE;
	u32 ht_ielen = 0, vht_ielen = 0;
	char *p;
	u8 ie_offset = (pnetwork->network.Reserved[0] == 2? 0:12);// Probe Request

	//parsing HT_CAP_IE
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], _HT_CAPABILITY_IE_, &ht_ielen, pnetwork->network.IELength-ie_offset);
	if(p && ht_ielen>0)
		ht_cap = _TRUE;

	#ifdef CONFIG_80211AC_VHT
	//parsing VHT_CAP_IE
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], EID_VHTCapability, &vht_ielen, pnetwork->network.IELength-ie_offset);
	if(p && vht_ielen>0)
		vht_cap = _TRUE;
	#endif
	 /* Add the protocol name */
	iwe->cmd = SIOCGIWNAME;
	if ((rtw_is_cckratesonly_included((u8*)&pnetwork->network.SupportedRates)) == _TRUE)
	{
		if(ht_cap == _TRUE)
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11bn");
		else
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11b");
	}
	else if ((rtw_is_cckrates_included((u8*)&pnetwork->network.SupportedRates)) == _TRUE)
	{
		if(ht_cap == _TRUE)
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11bgn");
		else
			snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11bg");
	}
	else
	{
		if(pnetwork->network.Configuration.DSConfig > 14)
		{
			#ifdef CONFIG_80211AC_VHT
			if(vht_cap == _TRUE){
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11AC");
			}
			else
			#endif
			{
				if(ht_cap == _TRUE)
					snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11an");
				else
					snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11a");
			}
		}
		else
		{
			if(ht_cap == _TRUE)
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11gn");
			else
				snprintf(iwe->u.name, IFNAMSIZ, "IEEE 802.11g");
		}
	}
	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_CHAR_LEN);
	return start;
 }

 static inline char * iwe_stream_rate_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
	u32 ht_ielen = 0, vht_ielen = 0;
	char *p;
	u16 max_rate=0, rate, ht_cap=_FALSE, vht_cap = _FALSE;
	u32 i = 0;
	u8 bw_40MHz=0, short_GI=0, bw_160MHz=0, vht_highest_rate = 0;
	u16 mcs_rate=0, vht_data_rate=0;
	char custom[MAX_CUSTOM_LEN]={0};
	u8 ie_offset = (pnetwork->network.Reserved[0] == 2? 0:12);// Probe Request

	//parsing HT_CAP_IE
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], _HT_CAPABILITY_IE_, &ht_ielen, pnetwork->network.IELength-ie_offset);
	if(p && ht_ielen>0)
	{
		struct rtw_ieee80211_ht_cap *pht_capie;
		ht_cap = _TRUE;
		pht_capie = (struct rtw_ieee80211_ht_cap *)(p+2);
		_rtw_memcpy(&mcs_rate , pht_capie->supp_mcs_set, 2);
		bw_40MHz = (pht_capie->cap_info&IEEE80211_HT_CAP_SUP_WIDTH) ? 1:0;
		short_GI = (pht_capie->cap_info&(IEEE80211_HT_CAP_SGI_20|IEEE80211_HT_CAP_SGI_40)) ? 1:0;
	}

#ifdef CONFIG_80211AC_VHT
	//parsing VHT_CAP_IE
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], EID_VHTCapability, &vht_ielen, pnetwork->network.IELength-ie_offset);
	if(p && vht_ielen>0)
	{
		u8	mcs_map[2];

		vht_cap = _TRUE;
		bw_160MHz = GET_VHT_CAPABILITY_ELE_CHL_WIDTH(p+2);
		if(bw_160MHz)
			short_GI = GET_VHT_CAPABILITY_ELE_SHORT_GI160M(p+2);
		else
			short_GI = GET_VHT_CAPABILITY_ELE_SHORT_GI80M(p+2);

		_rtw_memcpy(mcs_map, GET_VHT_CAPABILITY_ELE_TX_MCS(p+2), 2);

		vht_highest_rate = rtw_get_vht_highest_rate(mcs_map);
		vht_data_rate = rtw_vht_mcs_to_data_rate(CHANNEL_WIDTH_80, short_GI, vht_highest_rate);
	}
#endif

	/*Add basic and extended rates */
	p = custom;
	p += snprintf(p, MAX_CUSTOM_LEN - (p - custom), " Rates (Mb/s): ");
	while(pnetwork->network.SupportedRates[i]!=0)
	{
		rate = pnetwork->network.SupportedRates[i]&0x7F;
		if (rate > max_rate)
			max_rate = rate;
		p += snprintf(p, MAX_CUSTOM_LEN - (p - custom),
			      "%d%s ", rate >> 1, (rate & 1) ? ".5" : "");
		i++;
	}
#ifdef CONFIG_80211AC_VHT
	if(vht_cap == _TRUE) {
		max_rate = vht_data_rate;
	}
	else
#endif
	if(ht_cap == _TRUE)
	{
		if(mcs_rate&0x8000)//MCS15
		{
			max_rate = (bw_40MHz) ? ((short_GI)?300:270):((short_GI)?144:130);

		}
		else if(mcs_rate&0x0080)//MCS7
		{
			max_rate = (bw_40MHz) ? ((short_GI)?150:135):((short_GI)?72:65);
		}
		else//default MCS7
		{
			//DBG_871X("wx_get_scan, mcs_rate_bitmap=0x%x\n", mcs_rate);
			max_rate = (bw_40MHz) ? ((short_GI)?150:135):((short_GI)?72:65);
		}

		max_rate = max_rate*2;//Mbps/2;
	}

	iwe->cmd = SIOCGIWRATE;
	iwe->u.bitrate.fixed = iwe->u.bitrate.disabled = 0;
	iwe->u.bitrate.value = max_rate * 500000;
	start =iwe_stream_add_event(info, start, stop, iwe, IW_EV_PARAM_LEN);
	return start ;
}

static inline char * iwe_stream_wpa_wpa2_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
	int buf_size = MAX_WPA_IE_LEN*2;
	//u8 pbuf[buf_size]={0};
	u8 *pbuf = rtw_zmalloc(buf_size);

	u8 wpa_ie[255]={0},rsn_ie[255]={0};
	u16 i, wpa_len=0,rsn_len=0;
	u8 *p;
	sint out_len=0;


	if(pbuf){
		p=pbuf;

		//parsing WPA/WPA2 IE
		if (pnetwork->network.Reserved[0] != 2) // Probe Request
		{
			out_len=rtw_get_sec_ie(pnetwork->network.IEs ,pnetwork->network.IELength,rsn_ie,&rsn_len,wpa_ie,&wpa_len);
			RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: ssid=%s\n",pnetwork->network.Ssid.Ssid));
			RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: wpa_len=%d rsn_len=%d\n",wpa_len,rsn_len));

			if (wpa_len > 0){

				_rtw_memset(pbuf, 0, buf_size);
				p += sprintf(p, "wpa_ie=");
				for (i = 0; i < wpa_len; i++) {
					p += sprintf(p, "%02x", wpa_ie[i]);
				}

				if (wpa_len > 100) {
					printk("-----------------Len %d----------------\n", wpa_len);
					for (i = 0; i < wpa_len; i++) {
						printk("%02x ", wpa_ie[i]);
					}
					printk("\n");
					printk("-----------------Len %d----------------\n", wpa_len);
				}

				_rtw_memset(iwe, 0, sizeof(*iwe));
				iwe->cmd = IWEVCUSTOM;
				iwe->u.data.length = strlen(pbuf);
				start = iwe_stream_add_point(info, start, stop, iwe,pbuf);

				_rtw_memset(iwe, 0, sizeof(*iwe));
				iwe->cmd =IWEVGENIE;
				iwe->u.data.length = wpa_len;
				start = iwe_stream_add_point(info, start, stop, iwe, wpa_ie);
			}
			if (rsn_len > 0){

				_rtw_memset(pbuf, 0, buf_size);
				p += sprintf(p, "rsn_ie=");
				for (i = 0; i < rsn_len; i++) {
					p += sprintf(p, "%02x", rsn_ie[i]);
				}
				_rtw_memset(iwe, 0, sizeof(*iwe));
				iwe->cmd = IWEVCUSTOM;
				iwe->u.data.length = strlen(pbuf);
				start = iwe_stream_add_point(info, start, stop, iwe,pbuf);

				_rtw_memset(iwe, 0, sizeof(*iwe));
				iwe->cmd =IWEVGENIE;
				iwe->u.data.length = rsn_len;
				start = iwe_stream_add_point(info, start, stop, iwe, rsn_ie);
			}
		}

		rtw_mfree(pbuf, buf_size);
	}
	return start;
}

static inline char * iwe_stream_wps_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
 	//parsing WPS IE
	uint cnt = 0,total_ielen;
	u8 *wpsie_ptr=NULL;
	uint wps_ielen = 0;
	u8 ie_offset = (pnetwork->network.Reserved[0] == 2? 0:12);

	u8 *ie_ptr = pnetwork->network.IEs + ie_offset;
	total_ielen= pnetwork->network.IELength - ie_offset;

	if (pnetwork->network.Reserved[0] == 2) // Probe Request
	{
		ie_ptr = pnetwork->network.IEs;
		total_ielen = pnetwork->network.IELength;
	}
	else     // Beacon or Probe Respones
	{
		ie_ptr = pnetwork->network.IEs + _FIXED_IE_LENGTH_;
		total_ielen = pnetwork->network.IELength - _FIXED_IE_LENGTH_;
	}
	while(cnt < total_ielen)
	{
		if(rtw_is_wps_ie(&ie_ptr[cnt], &wps_ielen) && (wps_ielen>2))
		{
			wpsie_ptr = &ie_ptr[cnt];
			iwe->cmd =IWEVGENIE;
			iwe->u.data.length = (u16)wps_ielen;
			start = iwe_stream_add_point(info, start, stop,iwe, wpsie_ptr);
		}
		cnt+=ie_ptr[cnt+1]+2; //goto next
	}
	return start;
}

static inline char * iwe_stream_wapi_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
#ifdef CONFIG_WAPI_SUPPORT
	char *p;

	if (pnetwork->network.Reserved[0] != 2) // Probe Request
	{
		sint out_len_wapi=0;
		/* here use static for stack size */
		static u8 buf_wapi[MAX_WAPI_IE_LEN*2]={0};
		static u8 wapi_ie[MAX_WAPI_IE_LEN]={0};
		u16 wapi_len=0;
		u16  i;

		out_len_wapi=rtw_get_wapi_ie(pnetwork->network.IEs ,pnetwork->network.IELength,wapi_ie,&wapi_len);
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: ssid=%s\n",pnetwork->network.Ssid.Ssid));
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: wapi_len=%d \n",wapi_len));

		DBG_871X("rtw_wx_get_scan: %s ",pnetwork->network.Ssid.Ssid);
		DBG_871X("rtw_wx_get_scan: ssid = %d ",wapi_len);


		if (wapi_len > 0)
		{
			p=buf_wapi;
			//_rtw_memset(buf_wapi, 0, MAX_WAPI_IE_LEN*2);
			p += sprintf(p, "wapi_ie=");
			for (i = 0; i < wapi_len; i++) {
				p += sprintf(p, "%02x", wapi_ie[i]);
			}

			_rtw_memset(iwe, 0, sizeof(*iwe));
			iwe->cmd = IWEVCUSTOM;
			iwe->u.data.length = strlen(buf_wapi);
			start = iwe_stream_add_point(info, start, stop, iwe,buf_wapi);

			_rtw_memset(iwe, 0, sizeof(*iwe));
			iwe->cmd =IWEVGENIE;
			iwe->u.data.length = wapi_len;
			start = iwe_stream_add_point(info, start, stop, iwe, wapi_ie);
		}
	}
#endif//#ifdef CONFIG_WAPI_SUPPORT
	return start;
}

static inline char *  iwe_stream_rssi_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
	u8 ss, sq;
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	/* Add quality statistics */
	iwe->cmd = IWEVQUAL;
	iwe->u.qual.updated = IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED
	#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)
		| IW_QUAL_NOISE_UPDATED
	#else
		| IW_QUAL_NOISE_INVALID
	#endif
	#ifdef CONFIG_SIGNAL_DISPLAY_DBM
		| IW_QUAL_DBM
	#endif
	;

	if ( check_fwstate(pmlmepriv, _FW_LINKED)== _TRUE &&
		is_same_network(&pmlmepriv->cur_network.network, &pnetwork->network, 0)){
		ss = padapter->recvpriv.signal_strength;
		sq = padapter->recvpriv.signal_qual;
	} else {
		ss = pnetwork->network.PhyInfo.SignalStrength;
		sq = pnetwork->network.PhyInfo.SignalQuality;
	}


	#ifdef CONFIG_SIGNAL_DISPLAY_DBM
	iwe->u.qual.level = (u8) translate_percentage_to_dbm(ss); /* dbm */
	#else
	#ifdef CONFIG_SIGNAL_SCALE_MAPPING
	iwe->u.qual.level = (u8)ss; /* % */
	#else
	{
		/* Do signal scale mapping when using percentage as the unit of signal strength, since the scale mapping is skipped in odm */

		HAL_DATA_TYPE *pHal = GET_HAL_DATA(padapter);

		iwe->u.qual.level = (u8)odm_SignalScaleMapping(&pHal->odmpriv, ss);
	}
	#endif
	#endif

	iwe->u.qual.qual = (u8)sq;   // signal quality

	#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)
	{
		s16 tmp_noise=0;
		rtw_hal_get_odm_var(padapter, HAL_ODM_NOISE_MONITOR,&(pnetwork->network.Configuration.DSConfig), &(tmp_noise));
		iwe->u.qual.noise = tmp_noise ;
	}
	#else
	iwe->u.qual.noise = 0; // noise level
	#endif

	//DBG_871X("iqual=%d, ilevel=%d, inoise=%d, iupdated=%d\n", iwe.u.qual.qual, iwe.u.qual.level , iwe.u.qual.noise, iwe.u.qual.updated);

	start = iwe_stream_add_event(info, start, stop, iwe, IW_EV_QUAL_LEN);
	return start;
}

static inline char *  iwe_stream_net_rsv_process(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop,struct iw_event *iwe)
{
	u8 buf[32] = {0};
	u8 * p,*pos;
	int len;
	p = buf;
	pos = pnetwork->network.Reserved;

	p += sprintf(p, "fm=%02X%02X", pos[1], pos[0]);
	_rtw_memset(iwe, 0, sizeof(*iwe));
	iwe->cmd = IWEVCUSTOM;
	iwe->u.data.length = strlen(buf);
	start = iwe_stream_add_point(info, start, stop,iwe, buf);
	return start;
}

#if 1
static char *translate_scan(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop)
{
	struct iw_event iwe;
	u16 cap = 0;
	_rtw_memset(&iwe, 0, sizeof(iwe));

	if(_FALSE == search_p2p_wfd_ie(padapter,info,pnetwork,start,stop))
		return start;

	start = iwe_stream_mac_addr_proess(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_essid_proess(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_protocol_process(padapter,info,pnetwork,start,stop,&iwe);
	if (pnetwork->network.Reserved[0] == 2) // Probe Request
	{
		cap = 0;
	}
	else
	{
		_rtw_memcpy((u8 *)&cap, rtw_get_capability_from_ie(pnetwork->network.IEs), 2);
		cap = le16_to_cpu(cap);
	}

	start = iwe_stream_mode_process(padapter,info,pnetwork,start,stop,&iwe,cap);
	start = iwe_stream_chan_process(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_encryption_process(padapter,info,pnetwork,start,stop,&iwe,cap);
	start = iwe_stream_rate_process(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_wpa_wpa2_process(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_wps_process(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_wapi_process(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_rssi_process(padapter,info,pnetwork,start,stop,&iwe);
	start = iwe_stream_net_rsv_process(padapter,info,pnetwork,start,stop,&iwe);

	return start;
}
#else
static char *translate_scan(_adapter *padapter,
				struct iw_request_info* info, struct wlan_network *pnetwork,
				char *start, char *stop)
{
	struct iw_event iwe;
	u16 cap;
	u32 ht_ielen = 0, vht_ielen = 0;
	char custom[MAX_CUSTOM_LEN];
	char *p;
	u16 max_rate=0, rate, ht_cap=_FALSE, vht_cap = _FALSE;
	u32 i = 0;
	char	*current_val;
	long rssi;
	u8 bw_40MHz=0, short_GI=0, bw_160MHz=0, vht_highest_rate = 0;
	u16 mcs_rate=0, vht_data_rate=0;
	u8 ie_offset = (pnetwork->network.Reserved[0] == 2? 0:12);
	struct registry_priv *pregpriv = &padapter->registrypriv;

	if(_FALSE == search_p2p_wfd_ie(padapter,info,pnetwork,start,stop))
		return start;

	/*  AP MAC address  */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;

	_rtw_memcpy(iwe.u.ap_addr.sa_data, pnetwork->network.MacAddress, ETH_ALEN);
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_ADDR_LEN);

	/* Add the ESSID */
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.flags = 1;
	iwe.u.data.length = min((u16)pnetwork->network.Ssid.SsidLength, (u16)32);
	start = iwe_stream_add_point(info, start, stop, &iwe, pnetwork->network.Ssid.Ssid);

	//parsing HT_CAP_IE
	if (pnetwork->network.Reserved[0] == 2) // Probe Request
	{
		p = rtw_get_ie(&pnetwork->network.IEs[0], _HT_CAPABILITY_IE_, &ht_ielen, pnetwork->network.IELength);
	}
	else
	{
		p = rtw_get_ie(&pnetwork->network.IEs[12], _HT_CAPABILITY_IE_, &ht_ielen, pnetwork->network.IELength-12);
	}
	if(p && ht_ielen>0)
	{
		struct rtw_ieee80211_ht_cap *pht_capie;
		ht_cap = _TRUE;
		pht_capie = (struct rtw_ieee80211_ht_cap *)(p+2);
		_rtw_memcpy(&mcs_rate , pht_capie->supp_mcs_set, 2);
		bw_40MHz = (pht_capie->cap_info&IEEE80211_HT_CAP_SUP_WIDTH) ? 1:0;
		short_GI = (pht_capie->cap_info&(IEEE80211_HT_CAP_SGI_20|IEEE80211_HT_CAP_SGI_40)) ? 1:0;
	}

#ifdef CONFIG_80211AC_VHT
	//parsing VHT_CAP_IE
	p = rtw_get_ie(&pnetwork->network.IEs[ie_offset], EID_VHTCapability, &vht_ielen, pnetwork->network.IELength-ie_offset);
	if(p && vht_ielen>0)
	{
		u8	mcs_map[2];

		vht_cap = _TRUE;
		bw_160MHz = GET_VHT_CAPABILITY_ELE_CHL_WIDTH(p+2);
		if(bw_160MHz)
			short_GI = GET_VHT_CAPABILITY_ELE_SHORT_GI160M(p+2);
		else
			short_GI = GET_VHT_CAPABILITY_ELE_SHORT_GI80M(p+2);

		_rtw_memcpy(mcs_map, GET_VHT_CAPABILITY_ELE_TX_MCS(p+2), 2);

		vht_highest_rate = rtw_get_vht_highest_rate(mcs_map);
		vht_data_rate = rtw_vht_mcs_to_data_rate(CHANNEL_WIDTH_80, short_GI, vht_highest_rate);
	}
#endif

	/* Add the protocol name */
	iwe.cmd = SIOCGIWNAME;
	if ((rtw_is_cckratesonly_included((u8*)&pnetwork->network.SupportedRates)) == _TRUE)
	{
		if(ht_cap == _TRUE)
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11bn");
		else
		snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11b");
	}
	else if ((rtw_is_cckrates_included((u8*)&pnetwork->network.SupportedRates)) == _TRUE)
	{
		if(ht_cap == _TRUE)
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11bgn");
		else
			snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11bg");
	}
	else
	{
		if(pnetwork->network.Configuration.DSConfig > 14)
		{
			if(vht_cap == _TRUE)
				snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11AC");
			else if(ht_cap == _TRUE)
				snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11an");
			else
				snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11a");
		}
		else
		{
			if(ht_cap == _TRUE)
				snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11gn");
			else
				snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11g");
		}
	}

	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_CHAR_LEN);

	  /* Add mode */
	if (pnetwork->network.Reserved[0] == 2) // Probe Request
	{
		cap = 0;
	}
	else
	{
        iwe.cmd = SIOCGIWMODE;
		_rtw_memcpy((u8 *)&cap, rtw_get_capability_from_ie(pnetwork->network.IEs), 2);
		cap = le16_to_cpu(cap);
	}

	if(cap & (WLAN_CAPABILITY_IBSS |WLAN_CAPABILITY_BSS)){
		if (cap & WLAN_CAPABILITY_BSS)
			iwe.u.mode = IW_MODE_MASTER;
		else
			iwe.u.mode = IW_MODE_ADHOC;

		start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_UINT_LEN);
	}

	if(pnetwork->network.Configuration.DSConfig<1 /*|| pnetwork->network.Configuration.DSConfig>14*/)
		pnetwork->network.Configuration.DSConfig = 1;

	 /* Add frequency/channel */
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = rtw_ch2freq(pnetwork->network.Configuration.DSConfig) * 100000;
	iwe.u.freq.e = 1;
	iwe.u.freq.i = pnetwork->network.Configuration.DSConfig;
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_FREQ_LEN);

	/* Add encryption capability */
	iwe.cmd = SIOCGIWENCODE;
	if (cap & WLAN_CAPABILITY_PRIVACY)
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	iwe.u.data.length = 0;
	start = iwe_stream_add_point(info, start, stop, &iwe, pnetwork->network.Ssid.Ssid);

	/*Add basic and extended rates */
	max_rate = 0;
	p = custom;
	p += snprintf(p, MAX_CUSTOM_LEN - (p - custom), " Rates (Mb/s): ");
	while(pnetwork->network.SupportedRates[i]!=0)
	{
		rate = pnetwork->network.SupportedRates[i]&0x7F;
		if (rate > max_rate)
			max_rate = rate;
		p += snprintf(p, MAX_CUSTOM_LEN - (p - custom),
			      "%d%s ", rate >> 1, (rate & 1) ? ".5" : "");
		i++;
	}

	if(vht_cap == _TRUE) {
		max_rate = vht_data_rate;
	}
	else if(ht_cap == _TRUE)
	{
		if(mcs_rate&0x8000)//MCS15
		{
			max_rate = (bw_40MHz) ? ((short_GI)?300:270):((short_GI)?144:130);

		}
		else if(mcs_rate&0x0080)//MCS7
		{
			max_rate = (bw_40MHz) ? ((short_GI)?150:135):((short_GI)?72:65);
		}
		else//default MCS7
		{
			//DBG_871X("wx_get_scan, mcs_rate_bitmap=0x%x\n", mcs_rate);
			max_rate = (bw_40MHz) ? ((short_GI)?150:135):((short_GI)?72:65);
		}

		max_rate = max_rate*2;//Mbps/2;
	}

	iwe.cmd = SIOCGIWRATE;
	iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = 0;
	iwe.u.bitrate.value = max_rate * 500000;
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_PARAM_LEN);

	//parsing WPA/WPA2 IE
	if (pnetwork->network.Reserved[0] != 2) // Probe Request
	{
		u8 buf[MAX_WPA_IE_LEN*2];
		u8 wpa_ie[255],rsn_ie[255];
		u16 wpa_len=0,rsn_len=0;
		u8 *p;
		sint out_len=0;
		out_len=rtw_get_sec_ie(pnetwork->network.IEs ,pnetwork->network.IELength,rsn_ie,&rsn_len,wpa_ie,&wpa_len);
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: ssid=%s\n",pnetwork->network.Ssid.Ssid));
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: wpa_len=%d rsn_len=%d\n",wpa_len,rsn_len));

		if (wpa_len > 0)
		{
			p=buf;
			_rtw_memset(buf, 0, MAX_WPA_IE_LEN*2);
			p += sprintf(p, "wpa_ie=");
			for (i = 0; i < wpa_len; i++) {
				p += sprintf(p, "%02x", wpa_ie[i]);
			}

			if (wpa_len > 100) {
				printk("-----------------Len %d----------------\n", wpa_len);
				for (i = 0; i < wpa_len; i++) {
					printk("%02x ", wpa_ie[i]);
				}
				printk("\n");
				printk("-----------------Len %d----------------\n", wpa_len);
			}

			_rtw_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVCUSTOM;
			iwe.u.data.length = strlen(buf);
			start = iwe_stream_add_point(info, start, stop, &iwe,buf);

			_rtw_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd =IWEVGENIE;
			iwe.u.data.length = wpa_len;
			start = iwe_stream_add_point(info, start, stop, &iwe, wpa_ie);
		}
		if (rsn_len > 0)
		{
			p = buf;
			_rtw_memset(buf, 0, MAX_WPA_IE_LEN*2);
			p += sprintf(p, "rsn_ie=");
			for (i = 0; i < rsn_len; i++) {
				p += sprintf(p, "%02x", rsn_ie[i]);
			}
			_rtw_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVCUSTOM;
			iwe.u.data.length = strlen(buf);
			start = iwe_stream_add_point(info, start, stop, &iwe,buf);

			_rtw_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd =IWEVGENIE;
			iwe.u.data.length = rsn_len;
			start = iwe_stream_add_point(info, start, stop, &iwe, rsn_ie);
		}
	}

	{ //parsing WPS IE
		uint cnt = 0,total_ielen;
		u8 *wpsie_ptr=NULL;
		uint wps_ielen = 0;

		u8 *ie_ptr = pnetwork->network.IEs + ie_offset;
		total_ielen= pnetwork->network.IELength - ie_offset;

		if (pnetwork->network.Reserved[0] == 2) // Probe Request
		{
			ie_ptr = pnetwork->network.IEs;
			total_ielen = pnetwork->network.IELength;
		}
		else     // Beacon or Probe Respones
		{
			ie_ptr = pnetwork->network.IEs + _FIXED_IE_LENGTH_;
			total_ielen = pnetwork->network.IELength - _FIXED_IE_LENGTH_;
		}

		while(cnt < total_ielen)
		{
			if(rtw_is_wps_ie(&ie_ptr[cnt], &wps_ielen) && (wps_ielen>2))
			{
				wpsie_ptr = &ie_ptr[cnt];
				iwe.cmd =IWEVGENIE;
				iwe.u.data.length = (u16)wps_ielen;
				start = iwe_stream_add_point(info, start, stop, &iwe, wpsie_ptr);
			}
			cnt+=ie_ptr[cnt+1]+2; //goto next
		}
	}

#ifdef CONFIG_WAPI_SUPPORT
	if (pnetwork->network.Reserved[0] != 2) // Probe Request
	{
		sint out_len_wapi=0;
		/* here use static for stack size */
		static u8 buf_wapi[MAX_WAPI_IE_LEN*2];
		static u8 wapi_ie[MAX_WAPI_IE_LEN];
		u16 wapi_len=0;
		u16  i;

		_rtw_memset(buf_wapi, 0, MAX_WAPI_IE_LEN);
		_rtw_memset(wapi_ie, 0, MAX_WAPI_IE_LEN);

		out_len_wapi=rtw_get_wapi_ie(pnetwork->network.IEs ,pnetwork->network.IELength,wapi_ie,&wapi_len);
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: ssid=%s\n",pnetwork->network.Ssid.Ssid));
		RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan: wapi_len=%d \n",wapi_len));

		DBG_871X("rtw_wx_get_scan: %s ",pnetwork->network.Ssid.Ssid);
		DBG_871X("rtw_wx_get_scan: ssid = %d ",wapi_len);


		if (wapi_len > 0)
		{
			p=buf_wapi;
			_rtw_memset(buf_wapi, 0, MAX_WAPI_IE_LEN*2);
			p += sprintf(p, "wapi_ie=");
			for (i = 0; i < wapi_len; i++) {
				p += sprintf(p, "%02x", wapi_ie[i]);
			}

			_rtw_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVCUSTOM;
			iwe.u.data.length = strlen(buf_wapi);
			start = iwe_stream_add_point(info, start, stop, &iwe,buf_wapi);

			_rtw_memset(&iwe, 0, sizeof(iwe));
			iwe.cmd =IWEVGENIE;
			iwe.u.data.length = wapi_len;
			start = iwe_stream_add_point(info, start, stop, &iwe, wapi_ie);
		}
	}
#endif

{
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	u8 ss, sq;

	/* Add quality statistics */
	iwe.cmd = IWEVQUAL;
	iwe.u.qual.updated = IW_QUAL_QUAL_UPDATED | IW_QUAL_LEVEL_UPDATED
	#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)
		| IW_QUAL_NOISE_UPDATED
	#else
		| IW_QUAL_NOISE_INVALID
	#endif
	#ifdef CONFIG_SIGNAL_DISPLAY_DBM
		| IW_QUAL_DBM
	#endif
	;

	if ( check_fwstate(pmlmepriv, _FW_LINKED)== _TRUE &&
		is_same_network(&pmlmepriv->cur_network.network, &pnetwork->network, 0)){
		ss = padapter->recvpriv.signal_strength;
		sq = padapter->recvpriv.signal_qual;
	} else {
		ss = pnetwork->network.PhyInfo.SignalStrength;
		sq = pnetwork->network.PhyInfo.SignalQuality;
	}


	#ifdef CONFIG_SIGNAL_DISPLAY_DBM
	iwe.u.qual.level = (u8) translate_percentage_to_dbm(ss); /* dbm */
	#else
	#ifdef CONFIG_SIGNAL_SCALE_MAPPING
	iwe.u.qual.level = (u8)ss; /* % */
	#else
	{
		/* Do signal scale mapping when using percentage as the unit of signal strength, since the scale mapping is skipped in odm */

		HAL_DATA_TYPE *pHal = GET_HAL_DATA(padapter);

		iwe.u.qual.level = (u8)odm_SignalScaleMapping(&pHal->odmpriv, ss);
	}
	#endif
	#endif

	iwe.u.qual.qual = (u8)sq;   // signal quality

	#if defined(CONFIG_SIGNAL_DISPLAY_DBM) && defined(CONFIG_BACKGROUND_NOISE_MONITOR)
	{
		s16 tmp_noise=0;
		rtw_hal_get_odm_var(padapter, HAL_ODM_NOISE_MONITOR,&(pnetwork->network.Configuration.DSConfig), &(tmp_noise));
		iwe.u.qual.noise = tmp_noise ;
	}
	#else
	iwe.u.qual.noise = 0; // noise level
	#endif

	//DBG_871X("iqual=%d, ilevel=%d, inoise=%d, iupdated=%d\n", iwe.u.qual.qual, iwe.u.qual.level , iwe.u.qual.noise, iwe.u.qual.updated);

	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_QUAL_LEN);
}

	{
		u8 buf[MAX_WPA_IE_LEN];
		u8 * p,*pos;
		int len;
		p = buf;
		pos = pnetwork->network.Reserved;
		_rtw_memset(buf, 0, MAX_WPA_IE_LEN);
		p += sprintf(p, "fm=%02X%02X", pos[1], pos[0]);
		_rtw_memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = strlen(buf);
		start = iwe_stream_add_point(info, start, stop, &iwe, buf);
	}

	return start;
}
#endif

static int wpa_set_auth_algs(struct net_device *dev, u32 value)
{
	_adapter *padapter = (_adapter *) rtw_netdev_priv(dev);
	int ret = 0;

	if ((value & AUTH_ALG_SHARED_KEY)&&(value & AUTH_ALG_OPEN_SYSTEM))
	{
		DBG_871X("wpa_set_auth_algs, AUTH_ALG_SHARED_KEY and  AUTH_ALG_OPEN_SYSTEM [value:0x%x]\n",value);
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
		padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeAutoSwitch;
		padapter->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
	}
	else if (value & AUTH_ALG_SHARED_KEY)
	{
		DBG_871X("wpa_set_auth_algs, AUTH_ALG_SHARED_KEY  [value:0x%x]\n",value);
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;

		padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeShared;
		padapter->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Shared;
	}
	else if(value & AUTH_ALG_OPEN_SYSTEM)
	{
		DBG_871X("wpa_set_auth_algs, AUTH_ALG_OPEN_SYSTEM\n");
		//padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
		if(padapter->securitypriv.ndisauthtype < Ndis802_11AuthModeWPAPSK)
		{
			padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeOpen;
 			padapter->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open;
		}

	}
	else if(value & AUTH_ALG_LEAP)
	{
		DBG_871X("wpa_set_auth_algs, AUTH_ALG_LEAP\n");
	}
	else
	{
		DBG_871X("wpa_set_auth_algs, error!\n");
		ret = -EINVAL;
	}

	return ret;

}

static int wpa_set_encryption(struct net_device *dev, struct ieee_param *param, u32 param_len)
{
	int ret = 0;
	u32 wep_key_idx, wep_key_len,wep_total_len;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv 	*pmlmepriv = &padapter->mlmepriv;
	struct security_priv *psecuritypriv = &padapter->securitypriv;
#ifdef CONFIG_P2P
	struct wifidirect_info* pwdinfo = &padapter->wdinfo;
#endif //CONFIG_P2P

_func_enter_;

	param->u.crypt.err = 0;
	param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

	if (param_len < (u32) ((u8 *) param->u.crypt.key - (u8 *) param) + param->u.crypt.key_len)
	{
		ret =  -EINVAL;
		goto exit;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{

		if (param->u.crypt.idx >= WEP_KEYS
#ifdef CONFIG_IEEE80211W
			&& param->u.crypt.idx > BIP_MAX_KEYID
#endif //CONFIG_IEEE80211W
			)
		{
			ret = -EINVAL;
			goto exit;
		}
	}
	else
	{
#ifdef CONFIG_WAPI_SUPPORT
		if (strcmp(param->u.crypt.alg, "SMS4"))
#endif
		{
			ret = -EINVAL;
			goto exit;
		}
	}

	if (strcmp(param->u.crypt.alg, "WEP") == 0)
	{
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("wpa_set_encryption, crypt.alg = WEP\n"));
		DBG_871X("wpa_set_encryption, crypt.alg = WEP\n");

		wep_key_idx = param->u.crypt.idx;
		wep_key_len = param->u.crypt.key_len;

		if ((wep_key_idx > WEP_KEYS) || (wep_key_len <= 0))
		{
			ret = -EINVAL;
			goto exit;
		}

		if (psecuritypriv->bWepDefaultKeyIdxSet == 0) {
			/* wep default key has not been set, so use this key index as default key.*/

			wep_key_len = wep_key_len <= 5 ? 5 : 13;

			psecuritypriv->ndisencryptstatus = Ndis802_11Encryption1Enabled;
			psecuritypriv->dot11PrivacyAlgrthm = _WEP40_;
			psecuritypriv->dot118021XGrpPrivacy = _WEP40_;

			if (wep_key_len == 13)
			{
				psecuritypriv->dot11PrivacyAlgrthm = _WEP104_;
				psecuritypriv->dot118021XGrpPrivacy = _WEP104_;
			}

			psecuritypriv->dot11PrivacyKeyIndex = wep_key_idx;
		}

		_rtw_memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), param->u.crypt.key, wep_key_len);

		psecuritypriv->dot11DefKeylen[wep_key_idx] = wep_key_len;

		psecuritypriv->key_mask |= BIT(wep_key_idx);

		goto exit;
	}

	if(padapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) // 802_1x
	{
		struct sta_info * psta,*pbcmc_sta;
		struct sta_priv * pstapriv = &padapter->stapriv;

		if (check_fwstate(pmlmepriv, WIFI_STATION_STATE | WIFI_MP_STATE) == _TRUE) //sta mode
		{
			psta = rtw_get_stainfo(pstapriv, get_bssid(pmlmepriv));
			if (psta == NULL) {
				//DEBUG_ERR( ("Set wpa_set_encryption: Obtain Sta_info fail \n"));
			}
			else
			{
				//Jeff: don't disable ieee8021x_blocked while clearing key
				if (strcmp(param->u.crypt.alg, "none") != 0)
					psta->ieee8021x_blocked = _FALSE;

				if((padapter->securitypriv.ndisencryptstatus == Ndis802_11Encryption2Enabled)||
						(padapter->securitypriv.ndisencryptstatus ==  Ndis802_11Encryption3Enabled))
				{
					psta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;
				}

				if(param->u.crypt.set_tx ==1)//pairwise key
				{
					_rtw_memcpy(psta->dot118021x_UncstKey.skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

					if(strcmp(param->u.crypt.alg, "TKIP") == 0)//set mic key
					{
						//DEBUG_ERR(("\nset key length :param->u.crypt.key_len=%d\n", param->u.crypt.key_len));
						_rtw_memcpy(psta->dot11tkiptxmickey.skey, &(param->u.crypt.key[16]), 8);
						_rtw_memcpy(psta->dot11tkiprxmickey.skey, &(param->u.crypt.key[24]), 8);

						padapter->securitypriv.busetkipkey=_FALSE;
						//_set_timer(&padapter->securitypriv.tkip_timer, 50);
					}

					//DEBUG_ERR((" param->u.crypt.key_len=%d\n",param->u.crypt.key_len));
					DBG_871X(" ~~~~set sta key:unicastkey\n");

					rtw_setstakey_cmd(padapter, psta, UNICAST_KEY, _TRUE);

					psta->bpairwise_key_installed = _TRUE;

				}
				else//group key
				{
					if(strcmp(param->u.crypt.alg, "TKIP") == 0 || strcmp(param->u.crypt.alg, "CCMP") == 0)
					{
						_rtw_memcpy(padapter->securitypriv.dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key,(param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));
						//only TKIP group key need to install this
						if(param->u.crypt.key_len > 16)
						{
							_rtw_memcpy(padapter->securitypriv.dot118021XGrptxmickey[param->u.crypt.idx].skey,&(param->u.crypt.key[16]),8);
							_rtw_memcpy(padapter->securitypriv.dot118021XGrprxmickey[param->u.crypt.idx].skey,&(param->u.crypt.key[24]),8);
						}
						padapter->securitypriv.binstallGrpkey = _TRUE;
						//DEBUG_ERR((" param->u.crypt.key_len=%d\n", param->u.crypt.key_len));
						DBG_871X(" ~~~~set sta key:groupkey\n");

						padapter->securitypriv.dot118021XGrpKeyid = param->u.crypt.idx;

						rtw_set_key(padapter,&padapter->securitypriv,param->u.crypt.idx, 1, _TRUE);
					}
#ifdef CONFIG_IEEE80211W
					else if(strcmp(param->u.crypt.alg, "BIP") == 0)
					{
						int no;
						//printk("BIP key_len=%d , index=%d @@@@@@@@@@@@@@@@@@\n", param->u.crypt.key_len, param->u.crypt.idx);
						//save the IGTK key, length 16 bytes
						_rtw_memcpy(padapter->securitypriv.dot11wBIPKey[param->u.crypt.idx].skey,  param->u.crypt.key,(param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));
						/*printk("IGTK key below:\n");
						for(no=0;no<16;no++)
							printk(" %02x ", padapter->securitypriv.dot11wBIPKey[param->u.crypt.idx].skey[no]);
						printk("\n");*/
						padapter->securitypriv.dot11wBIPKeyid = param->u.crypt.idx;
						padapter->securitypriv.binstallBIPkey = _TRUE;
						DBG_871X(" ~~~~set sta key:IGKT\n");
					}
#endif //CONFIG_IEEE80211W

#ifdef CONFIG_P2P
					if(rtw_p2p_chk_state(pwdinfo, P2P_STATE_PROVISIONING_ING))
					{
						rtw_p2p_set_state(pwdinfo, P2P_STATE_PROVISIONING_DONE);
					}
#endif //CONFIG_P2P

				}
			}

			pbcmc_sta=rtw_get_bcmc_stainfo(padapter);
			if(pbcmc_sta==NULL)
			{
				//DEBUG_ERR( ("Set OID_802_11_ADD_KEY: bcmc stainfo is null \n"));
			}
			else
			{
				//Jeff: don't disable ieee8021x_blocked while clearing key
				if (strcmp(param->u.crypt.alg, "none") != 0)
					pbcmc_sta->ieee8021x_blocked = _FALSE;

				if((padapter->securitypriv.ndisencryptstatus == Ndis802_11Encryption2Enabled)||
						(padapter->securitypriv.ndisencryptstatus ==  Ndis802_11Encryption3Enabled))
				{
					pbcmc_sta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;
				}
			}
		}
		else if(check_fwstate(pmlmepriv, WIFI_ADHOC_STATE)) //adhoc mode
		{
		}
	}

#ifdef CONFIG_WAPI_SUPPORT
	if (strcmp(param->u.crypt.alg, "SMS4") == 0)
	{
		PRT_WAPI_T			pWapiInfo = &padapter->wapiInfo;
		PRT_WAPI_STA_INFO	pWapiSta;
		u8					WapiASUEPNInitialValueSrc[16] = {0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C} ;
		u8					WapiAEPNInitialValueSrc[16] = {0x37,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C} ;
		u8 					WapiAEMultiCastPNInitialValueSrc[16] = {0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C,0x36,0x5C} ;

		if(param->u.crypt.set_tx == 1)
		{
			list_for_each_entry(pWapiSta, &pWapiInfo->wapiSTAUsedList, list) {
				if(_rtw_memcmp(pWapiSta->PeerMacAddr,param->sta_addr,6))
				{
					_rtw_memcpy(pWapiSta->lastTxUnicastPN,WapiASUEPNInitialValueSrc,16);

					pWapiSta->wapiUsk.bSet = true;
					_rtw_memcpy(pWapiSta->wapiUsk.dataKey,param->u.crypt.key,16);
					_rtw_memcpy(pWapiSta->wapiUsk.micKey,param->u.crypt.key+16,16);
					pWapiSta->wapiUsk.keyId = param->u.crypt.idx ;
					pWapiSta->wapiUsk.bTxEnable = true;

					_rtw_memcpy(pWapiSta->lastRxUnicastPNBEQueue,WapiAEPNInitialValueSrc,16);
					_rtw_memcpy(pWapiSta->lastRxUnicastPNBKQueue,WapiAEPNInitialValueSrc,16);
					_rtw_memcpy(pWapiSta->lastRxUnicastPNVIQueue,WapiAEPNInitialValueSrc,16);
					_rtw_memcpy(pWapiSta->lastRxUnicastPNVOQueue,WapiAEPNInitialValueSrc,16);
					_rtw_memcpy(pWapiSta->lastRxUnicastPN,WapiAEPNInitialValueSrc,16);
					pWapiSta->wapiUskUpdate.bTxEnable = false;
					pWapiSta->wapiUskUpdate.bSet = false;

					if (psecuritypriv->sw_encrypt== false || psecuritypriv->sw_decrypt == false)
					{
						//set unicast key for ASUE
						rtw_wapi_set_key(padapter, &pWapiSta->wapiUsk, pWapiSta, false, false);
					}
				}
			}
		}
		else
		{
			list_for_each_entry(pWapiSta, &pWapiInfo->wapiSTAUsedList, list) {
				if(_rtw_memcmp(pWapiSta->PeerMacAddr,get_bssid(pmlmepriv),6))
				{
					pWapiSta->wapiMsk.bSet = true;
					_rtw_memcpy(pWapiSta->wapiMsk.dataKey,param->u.crypt.key,16);
					_rtw_memcpy(pWapiSta->wapiMsk.micKey,param->u.crypt.key+16,16);
					pWapiSta->wapiMsk.keyId = param->u.crypt.idx ;
					pWapiSta->wapiMsk.bTxEnable = false;
					if(!pWapiSta->bSetkeyOk)
						pWapiSta->bSetkeyOk = true;
					pWapiSta->bAuthenticateInProgress = false;

					_rtw_memcpy(pWapiSta->lastRxMulticastPN, WapiAEMultiCastPNInitialValueSrc, 16);

					if (psecuritypriv->sw_decrypt == false)
					{
						//set rx broadcast key for ASUE
						rtw_wapi_set_key(padapter, &pWapiSta->wapiMsk, pWapiSta, true, false);
					}
				}

			}
		}
	}
#endif

exit:

_func_exit_;

	return ret;
}

static int rtw_set_wpa_ie(_adapter *padapter, char *pie, unsigned short ielen)
{
	u8 *buf=NULL, *pos=NULL;
	u32 left;
	int group_cipher = 0, pairwise_cipher = 0;
	int ret = 0;
	u8 null_addr[]= {0,0,0,0,0,0};
#ifdef CONFIG_P2P
	struct wifidirect_info* pwdinfo = &padapter->wdinfo;
#endif //CONFIG_P2P

	if((ielen > MAX_WPA_IE_LEN) || (pie == NULL)){
		_clr_fwstate_(&padapter->mlmepriv, WIFI_UNDER_WPS);
		if(pie == NULL)
			return ret;
		else
			return -EINVAL;
	}

	if(ielen)
	{
		buf = rtw_zmalloc(ielen);
		if (buf == NULL){
			ret =  -ENOMEM;
			goto exit;
		}

		_rtw_memcpy(buf, pie , ielen);

		//dump
		{
			int i;
			DBG_871X("\n wpa_ie(length:%d):\n", ielen);
			for(i=0;i<ielen;i=i+8)
				DBG_871X("0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x \n",buf[i],buf[i+1],buf[i+2],buf[i+3],buf[i+4],buf[i+5],buf[i+6],buf[i+7]);
		}

		pos = buf;
		if(ielen < RSN_HEADER_LEN){
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("Ie len too short %d\n", ielen));
			ret  = -1;
			goto exit;
		}

#if 0
		pos += RSN_HEADER_LEN;
		left  = ielen - RSN_HEADER_LEN;

		if (left >= RSN_SELECTOR_LEN){
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
		else if (left > 0){
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("Ie length mismatch, %u too much \n", left));
			ret =-1;
			goto exit;
		}
#endif

		if(rtw_parse_wpa_ie(buf, ielen, &group_cipher, &pairwise_cipher, NULL) == _SUCCESS)
		{
			padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_8021X;
			padapter->securitypriv.ndisauthtype=Ndis802_11AuthModeWPAPSK;
			_rtw_memcpy(padapter->securitypriv.supplicant_ie, &buf[0], ielen);
		}

		if(rtw_parse_wpa2_ie(buf, ielen, &group_cipher, &pairwise_cipher, NULL) == _SUCCESS)
		{
			padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_8021X;
			padapter->securitypriv.ndisauthtype=Ndis802_11AuthModeWPA2PSK;
			_rtw_memcpy(padapter->securitypriv.supplicant_ie, &buf[0], ielen);
		}

		if (group_cipher == 0)
		{
			group_cipher = WPA_CIPHER_NONE;
		}
		if (pairwise_cipher == 0)
		{
			pairwise_cipher = WPA_CIPHER_NONE;
		}

		switch(group_cipher)
		{
			case WPA_CIPHER_NONE:
				padapter->securitypriv.dot118021XGrpPrivacy=_NO_PRIVACY_;
				padapter->securitypriv.ndisencryptstatus=Ndis802_11EncryptionDisabled;
				break;
			case WPA_CIPHER_WEP40:
				padapter->securitypriv.dot118021XGrpPrivacy=_WEP40_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
			case WPA_CIPHER_TKIP:
				padapter->securitypriv.dot118021XGrpPrivacy=_TKIP_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				break;
			case WPA_CIPHER_CCMP:
				padapter->securitypriv.dot118021XGrpPrivacy=_AES_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				break;
			case WPA_CIPHER_WEP104:
				padapter->securitypriv.dot118021XGrpPrivacy=_WEP104_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
		}

		switch(pairwise_cipher)
		{
			case WPA_CIPHER_NONE:
				padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
				padapter->securitypriv.ndisencryptstatus=Ndis802_11EncryptionDisabled;
				break;
			case WPA_CIPHER_WEP40:
				padapter->securitypriv.dot11PrivacyAlgrthm=_WEP40_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
			case WPA_CIPHER_TKIP:
				padapter->securitypriv.dot11PrivacyAlgrthm=_TKIP_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				break;
			case WPA_CIPHER_CCMP:
				padapter->securitypriv.dot11PrivacyAlgrthm=_AES_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				break;
			case WPA_CIPHER_WEP104:
				padapter->securitypriv.dot11PrivacyAlgrthm=_WEP104_;
				padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;
				break;
		}

		_clr_fwstate_(&padapter->mlmepriv, WIFI_UNDER_WPS);
		{//set wps_ie
			u16 cnt = 0;
			u8 eid, wps_oui[4]={0x0,0x50,0xf2,0x04};

			while( cnt < ielen )
			{
				eid = buf[cnt];

				if((eid==_VENDOR_SPECIFIC_IE_)&&(_rtw_memcmp(&buf[cnt+2], wps_oui, 4)==_TRUE))
				{
					DBG_871X("SET WPS_IE\n");

					padapter->securitypriv.wps_ie_len = ((buf[cnt+1]+2) < MAX_WPS_IE_LEN) ? (buf[cnt+1]+2):MAX_WPS_IE_LEN;

					_rtw_memcpy(padapter->securitypriv.wps_ie, &buf[cnt], padapter->securitypriv.wps_ie_len);

					set_fwstate(&padapter->mlmepriv, WIFI_UNDER_WPS);

#ifdef CONFIG_P2P
					if(rtw_p2p_chk_state(pwdinfo, P2P_STATE_GONEGO_OK))
					{
						rtw_p2p_set_state(pwdinfo, P2P_STATE_PROVISIONING_ING);
					}
#endif //CONFIG_P2P
					cnt += buf[cnt+1]+2;

					break;
				} else {
					cnt += buf[cnt+1]+2; //goto next
				}
			}
		}
	}

	//TKIP and AES disallow multicast packets until installing group key
        if(padapter->securitypriv.dot11PrivacyAlgrthm == _TKIP_
                || padapter->securitypriv.dot11PrivacyAlgrthm == _TKIP_WTMIC_
                || padapter->securitypriv.dot11PrivacyAlgrthm == _AES_)
                //WPS open need to enable multicast
                //|| check_fwstate(&padapter->mlmepriv, WIFI_UNDER_WPS) == _TRUE)
                rtw_hal_set_hwreg(padapter, HW_VAR_OFF_RCR_AM, null_addr);

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
		 ("rtw_set_wpa_ie: pairwise_cipher=0x%08x padapter->securitypriv.ndisencryptstatus=%d padapter->securitypriv.ndisauthtype=%d\n",
		  pairwise_cipher, padapter->securitypriv.ndisencryptstatus, padapter->securitypriv.ndisauthtype));

exit:

	if (buf) rtw_mfree(buf, ielen);

	return ret;
}

static int rtw_wx_get_name(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	u16 cap;
	u32 ht_ielen = 0;
	char *p;
	u8 ht_cap=_FALSE, vht_cap=_FALSE;
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	WLAN_BSSID_EX  *pcur_bss = &pmlmepriv->cur_network.network;
	NDIS_802_11_RATES_EX* prates = NULL;

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("cmd_code=%x\n", info->cmd));

	_func_enter_;

	if (check_fwstate(pmlmepriv, _FW_LINKED|WIFI_ADHOC_MASTER_STATE) == _TRUE)
	{
		//parsing HT_CAP_IE
		p = rtw_get_ie(&pcur_bss->IEs[12], _HT_CAPABILITY_IE_, &ht_ielen, pcur_bss->IELength-12);
		if(p && ht_ielen>0)
		{
			ht_cap = _TRUE;
		}

#ifdef CONFIG_80211AC_VHT
		if(pmlmepriv->vhtpriv.vht_option == _TRUE)
			vht_cap = _TRUE;
#endif

		prates = &pcur_bss->SupportedRates;

		if (rtw_is_cckratesonly_included((u8*)prates) == _TRUE)
		{
			if(ht_cap == _TRUE)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11b");
		}
		else if ((rtw_is_cckrates_included((u8*)prates)) == _TRUE)
		{
			if(ht_cap == _TRUE)
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bgn");
			else
				snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11bg");
		}
		else
		{
			if(pcur_bss->Configuration.DSConfig > 14)
			{
			#ifdef CONFIG_80211AC_VHT
				if(vht_cap == _TRUE){
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11AC");
				}
				else
			#endif
				{
					if(ht_cap == _TRUE)
						snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11an");
					else
						snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11a");
				}
			}
			else
			{
				if(ht_cap == _TRUE)
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11gn");
				else
					snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11g");
			}
		}
	}
	else
	{
		//prates = &padapter->registrypriv.dev_network.SupportedRates;
		//snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11g");
		snprintf(wrqu->name, IFNAMSIZ, "unassociated");
	}

	_func_exit_;

	return 0;
}

static int rtw_wx_set_freq(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{

	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	struct wlan_network  *cur_network = &(pmlmepriv->cur_network);
	int exp = 1, freq = 0, div = 0;

	_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_, _drv_notice_, ("+rtw_wx_set_freq\n"));

	if (wrqu->freq.m <= 1000) {
		if (wrqu->freq.flags == IW_FREQ_AUTO) {
			padapter->mlmeextpriv.cur_channel = 1;
			DBG_871X("%s: channel is auto, set to channel 1\n", __func__);
		} else {
			padapter->mlmeextpriv.cur_channel = wrqu->freq.m;
			DBG_871X("%s: set to channel %d\n", __func__, padapter->mlmeextpriv.cur_channel);
		}
	} else {
		while (wrqu->freq.e) {
			exp *= 10;
			wrqu->freq.e--;
		}

		freq = wrqu->freq.m;
		while (!(freq%10)) {
			freq /= 10;
			exp *= 10;
		}

		/* freq unit is MHz here */
		div = 1000000/exp;

		if (div)
			freq /= div;
		else {
			div = exp/1000000;
			freq *= div;
		}

		/* If freq is invalid, rtw_freq2ch() will return channel 1 */
		padapter->mlmeextpriv.cur_channel = rtw_freq2ch(freq);
		DBG_871X("%s: set to channel %d\n", __func__, padapter->mlmeextpriv.cur_channel);
	}

	_func_exit_;

	return 0;
}

static int rtw_wx_get_freq(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	WLAN_BSSID_EX  *pcur_bss = &pmlmepriv->cur_network.network;

	if(check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE)
	{
		//wrqu->freq.m = ieee80211_wlan_frequencies[pcur_bss->Configuration.DSConfig-1] * 100000;
		wrqu->freq.m = rtw_ch2freq(pcur_bss->Configuration.DSConfig) * 100000;
		wrqu->freq.e = 1;
		wrqu->freq.i = pcur_bss->Configuration.DSConfig;

	}
	else{
		wrqu->freq.m = rtw_ch2freq(padapter->mlmeextpriv.cur_channel) * 100000;
		wrqu->freq.e = 1;
		wrqu->freq.i = padapter->mlmeextpriv.cur_channel;
	}

	return 0;
}

static int rtw_wx_set_mode(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	NDIS_802_11_NETWORK_INFRASTRUCTURE networkType ;
	int ret = 0;

	_func_enter_;

	if(_FAIL == rtw_pwr_wakeup(padapter)) {
		ret= -EPERM;
		goto exit;
	}

	if (!rtw_is_hw_init_completed(padapter)) {
		ret = -EPERM;
		goto exit;
	}

	/* initial default type */
	dev->type = ARPHRD_ETHER;

	switch(wrqu->mode)
	{
		case IW_MODE_MONITOR:
			networkType = Ndis802_11Monitor;
#if 0
			dev->type = ARPHRD_IEEE80211; /* IEEE 802.11 : 801 */
#endif
			dev->type = ARPHRD_IEEE80211_RADIOTAP; /* IEEE 802.11 + radiotap header : 803 */
			DBG_871X("set_mode = IW_MODE_MONITOR\n");
			break;

		case IW_MODE_AUTO:
			networkType = Ndis802_11AutoUnknown;
			DBG_871X("set_mode = IW_MODE_AUTO\n");
			break;
		case IW_MODE_ADHOC:
			networkType = Ndis802_11IBSS;
			DBG_871X("set_mode = IW_MODE_ADHOC\n");
			break;
		case IW_MODE_MASTER:
			networkType = Ndis802_11APMode;
			DBG_871X("set_mode = IW_MODE_MASTER\n");
                        //rtw_setopmode_cmd(padapter, networkType,_TRUE);
			break;
		case IW_MODE_INFRA:
			networkType = Ndis802_11Infrastructure;
			DBG_871X("set_mode = IW_MODE_INFRA\n");
			break;

		default :
			ret = -EINVAL;;
			RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("\n Mode: %s is not supported  \n", iw_operation_mode[wrqu->mode]));
			goto exit;
	}

/*
	if(Ndis802_11APMode == networkType)
	{
		rtw_setopmode_cmd(padapter, networkType,_TRUE);
	}
	else
	{
		rtw_setopmode_cmd(padapter, Ndis802_11AutoUnknown,_TRUE);
	}
*/

	if (rtw_set_802_11_infrastructure_mode(padapter, networkType) ==_FALSE){

		ret = -EPERM;
		goto exit;

	}

	rtw_setopmode_cmd(padapter, networkType,_TRUE);

exit:

	_func_exit_;

	return ret;

}

static int rtw_wx_get_mode(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *b)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,(" rtw_wx_get_mode \n"));

	_func_enter_;

	if (check_fwstate(pmlmepriv, WIFI_STATION_STATE) == _TRUE)
	{
		wrqu->mode = IW_MODE_INFRA;
	}
	else if  ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) == _TRUE) ||
		       (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE))

	{
		wrqu->mode = IW_MODE_ADHOC;
	}
	else if(check_fwstate(pmlmepriv, WIFI_AP_STATE) == _TRUE)
	{
		wrqu->mode = IW_MODE_MASTER;
	}
	else
	{
		wrqu->mode = IW_MODE_AUTO;
	}

	_func_exit_;

	return 0;

}


static int rtw_wx_set_pmkid(struct net_device *dev,
	                     struct iw_request_info *a,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	u8          j,blInserted = _FALSE;
	int         intReturn = _FALSE;
	struct mlme_priv  *pmlmepriv = &padapter->mlmepriv;
	struct security_priv *psecuritypriv = &padapter->securitypriv;
        struct iw_pmksa*  pPMK = ( struct iw_pmksa* ) extra;
        u8     strZeroMacAddress[ ETH_ALEN ] = { 0x00 };
        u8     strIssueBssid[ ETH_ALEN ] = { 0x00 };

/*
        struct iw_pmksa
        {
            __u32   cmd;
            struct sockaddr bssid;
            __u8    pmkid[IW_PMKID_LEN];   //IW_PMKID_LEN=16
        }
        There are the BSSID information in the bssid.sa_data array.
        If cmd is IW_PMKSA_FLUSH, it means the wpa_suppplicant wants to clear all the PMKID information.
        If cmd is IW_PMKSA_ADD, it means the wpa_supplicant wants to add a PMKID/BSSID to driver.
        If cmd is IW_PMKSA_REMOVE, it means the wpa_supplicant wants to remove a PMKID/BSSID from driver.
        */

	_rtw_memcpy( strIssueBssid, pPMK->bssid.sa_data, ETH_ALEN);
        if ( pPMK->cmd == IW_PMKSA_ADD )
        {
                DBG_871X( "[rtw_wx_set_pmkid] IW_PMKSA_ADD!\n" );
                if ( _rtw_memcmp( strIssueBssid, strZeroMacAddress, ETH_ALEN ) == _TRUE )
                {
                    return( intReturn );
                }
                else
                {
                    intReturn = _TRUE;
                }
		blInserted = _FALSE;

		//overwrite PMKID
		for(j=0 ; j<NUM_PMKID_CACHE; j++)
		{
			if( _rtw_memcmp( psecuritypriv->PMKIDList[j].Bssid, strIssueBssid, ETH_ALEN) ==_TRUE )
			{ // BSSID is matched, the same AP => rewrite with new PMKID.

                                DBG_871X( "[rtw_wx_set_pmkid] BSSID exists in the PMKList.\n" );

				_rtw_memcpy( psecuritypriv->PMKIDList[j].PMKID, pPMK->pmkid, IW_PMKID_LEN);
                                psecuritypriv->PMKIDList[ j ].bUsed = _TRUE;
				psecuritypriv->PMKIDIndex = j+1;
				blInserted = _TRUE;
				break;
			}
	        }

	        if(!blInserted)
                {
		    // Find a new entry
                    DBG_871X( "[rtw_wx_set_pmkid] Use the new entry index = %d for this PMKID.\n",
                            psecuritypriv->PMKIDIndex );

	            _rtw_memcpy(psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].Bssid, strIssueBssid, ETH_ALEN);
		    _rtw_memcpy(psecuritypriv->PMKIDList[psecuritypriv->PMKIDIndex].PMKID, pPMK->pmkid, IW_PMKID_LEN);

                    psecuritypriv->PMKIDList[ psecuritypriv->PMKIDIndex ].bUsed = _TRUE;
		    psecuritypriv->PMKIDIndex++ ;
		    if(psecuritypriv->PMKIDIndex==16)
                    {
		        psecuritypriv->PMKIDIndex =0;
                    }
		}
        }
        else if ( pPMK->cmd == IW_PMKSA_REMOVE )
        {
                DBG_871X( "[rtw_wx_set_pmkid] IW_PMKSA_REMOVE!\n" );
                intReturn = _TRUE;
		for(j=0 ; j<NUM_PMKID_CACHE; j++)
		{
			if( _rtw_memcmp( psecuritypriv->PMKIDList[j].Bssid, strIssueBssid, ETH_ALEN) ==_TRUE )
			{ // BSSID is matched, the same AP => Remove this PMKID information and reset it.
                                _rtw_memset( psecuritypriv->PMKIDList[ j ].Bssid, 0x00, ETH_ALEN );
                                psecuritypriv->PMKIDList[ j ].bUsed = _FALSE;
				break;
			}
	        }
        }
        else if ( pPMK->cmd == IW_PMKSA_FLUSH )
        {
            DBG_871X( "[rtw_wx_set_pmkid] IW_PMKSA_FLUSH!\n" );
            _rtw_memset( &psecuritypriv->PMKIDList[ 0 ], 0x00, sizeof( RT_PMKID_LIST ) * NUM_PMKID_CACHE );
            psecuritypriv->PMKIDIndex = 0;
            intReturn = _TRUE;
        }
    return( intReturn );
}

static int rtw_wx_get_sens(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	{
		wrqu->sens.value = 0;
		wrqu->sens.fixed = 0;	/* no auto select */
		wrqu->sens.disabled = 1;
	}
	return 0;
}

static int rtw_wx_get_range(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;

	u16 val;
	int i;

	_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_range. cmd_code=%x\n", info->cmd));

	wrqu->data.length = sizeof(*range);
	_rtw_memset(range, 0, sizeof(*range));

	/* Let's try to keep this struct in the same order as in
	 * linux/include/wireless.h
	 */

	/* TODO: See what values we can set, and remove the ones we can't
	 * set, or fill them with some default data.
	 */

	/* ~5 Mb/s real (802.11b) */
	range->throughput = 5 * 1000 * 1000;

	// TODO: Not used in 802.11b?
//	range->min_nwid;	/* Minimal NWID we are able to set */
	// TODO: Not used in 802.11b?
//	range->max_nwid;	/* Maximal NWID we are able to set */

        /* Old Frequency (backward compat - moved lower ) */
//	range->old_num_channels;
//	range->old_num_frequency;
//	range->old_freq[6]; /* Filler to keep "version" at the same offset */

	/* signal level threshold range */

	/* Quality of link & SNR stuff */
	/* Quality range (link, level, noise)
	 * If the quality is absolute, it will be in the range [0 ; max_qual],
	 * if the quality is dBm, it will be in the range [max_qual ; 0].
	 * Don't forget that we use 8 bit arithmetics...
	 *
	 * If percentage range is 0~100
	 * Signal strength dbm range logical is -100 ~ 0
	 * but usually value is -90 ~ -20
	 * When CONFIG_SIGNAL_SCALE_MAPPING is defined, dbm range is -95 ~ -45
	 */
	range->max_qual.qual = 100;
#ifdef CONFIG_SIGNAL_DISPLAY_DBM
	range->max_qual.level = (u8)-100;
	range->max_qual.noise = (u8)-100;
	range->max_qual.updated = IW_QUAL_ALL_UPDATED; /* Updated all three */
	range->max_qual.updated |= IW_QUAL_DBM;
#else /* !CONFIG_SIGNAL_DISPLAY_DBM */
	//percent values between 0 and 100.
	range->max_qual.level = 100;
	range->max_qual.noise = 100;
	range->max_qual.updated = IW_QUAL_ALL_UPDATED; /* Updated all three */
#endif /* !CONFIG_SIGNAL_DISPLAY_DBM */

	/* This should contain the average/typical values of the quality
	 * indicator. This should be the threshold between a "good" and
	 * a "bad" link (example : monitor going from green to orange).
	 * Currently, user space apps like quality monitors don't have any
	 * way to calibrate the measurement. With this, they can split
	 * the range between 0 and max_qual in different quality level
	 * (using a geometric subdivision centered on the average).
	 * I expect that people doing the user space apps will feedback
	 * us on which value we need to put in each driver... */
	range->avg_qual.qual = 92; /* > 8% missed beacons is 'bad' */
#ifdef CONFIG_SIGNAL_DISPLAY_DBM
	/* TODO: Find real 'good' to 'bad' threshold value for RSSI */
	range->avg_qual.level = (u8)-70;
	range->avg_qual.noise = 0;
	range->avg_qual.updated = IW_QUAL_ALL_UPDATED; /* Updated all three */
	range->avg_qual.updated |= IW_QUAL_DBM;
#else /* !CONFIG_SIGNAL_DISPLAY_DBM */
	/* TODO: Find real 'good' to 'bad' threshol value for RSSI */
	range->avg_qual.level = 30;
	range->avg_qual.noise = 100;
	range->avg_qual.updated = IW_QUAL_ALL_UPDATED; /* Updated all three */
#endif /* !CONFIG_SIGNAL_DISPLAY_DBM */

	range->num_bitrates = RATE_COUNT;

	for (i = 0; i < RATE_COUNT && i < IW_MAX_BITRATES; i++) {
		range->bitrate[i] = rtw_rates[i];
	}

	range->min_frag = MIN_FRAG_THRESHOLD;
	range->max_frag = MAX_FRAG_THRESHOLD;

	range->pm_capa = 0;

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 16;

//	range->retry_capa;	/* What retry options are supported */
//	range->retry_flags;	/* How to decode max/min retry limit */
//	range->r_time_flags;	/* How to decode max/min retry life */
//	range->min_retry;	/* Minimal number of retries */
//	range->max_retry;	/* Maximal number of retries */
//	range->min_r_time;	/* Minimal retry lifetime */
//	range->max_r_time;	/* Maximal retry lifetime */

	for (i = 0, val = 0; i < MAX_CHANNEL_NUM; i++) {

		// Include only legal frequencies for some countries
		if(pmlmeext->channel_set[i].ChannelNum != 0)
		{
			range->freq[val].i = pmlmeext->channel_set[i].ChannelNum;
			range->freq[val].m = rtw_ch2freq(pmlmeext->channel_set[i].ChannelNum) * 100000;
			range->freq[val].e = 1;
			val++;
		}

		if (val == IW_MAX_FREQUENCIES)
			break;
	}

	range->num_channels = val;
	range->num_frequency = val;

// Commented by Albert 2009/10/13
// The following code will proivde the security capability to network manager.
// If the driver doesn't provide this capability to network manager,
// the WPA/WPA2 routers can't be choosen in the network manager.

/*
#define IW_SCAN_CAPA_NONE		0x00
#define IW_SCAN_CAPA_ESSID		0x01
#define IW_SCAN_CAPA_BSSID		0x02
#define IW_SCAN_CAPA_CHANNEL	0x04
#define IW_SCAN_CAPA_MODE		0x08
#define IW_SCAN_CAPA_RATE		0x10
#define IW_SCAN_CAPA_TYPE		0x20
#define IW_SCAN_CAPA_TIME		0x40
*/

#if WIRELESS_EXT > 17
	range->enc_capa = IW_ENC_CAPA_WPA|IW_ENC_CAPA_WPA2|
			  IW_ENC_CAPA_CIPHER_TKIP|IW_ENC_CAPA_CIPHER_CCMP;
#endif

#ifdef IW_SCAN_CAPA_ESSID //WIRELESS_EXT > 21
	range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_TYPE |IW_SCAN_CAPA_BSSID|
					IW_SCAN_CAPA_CHANNEL|IW_SCAN_CAPA_MODE|IW_SCAN_CAPA_RATE;
#endif


	_func_exit_;

	return 0;

}

//set bssid flow
//s1. rtw_set_802_11_infrastructure_mode()
//s2. rtw_set_802_11_authentication_mode()
//s3. set_802_11_encryption_mode()
//s4. rtw_set_802_11_bssid()
static int rtw_wx_set_wap(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *awrq,
			 char *extra)
{
	_irqL	irqL;
	uint ret = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct sockaddr *temp = (struct sockaddr *)awrq;
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	_list	*phead;
	u8 *dst_bssid, *src_bssid;
	_queue	*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	NDIS_802_11_AUTHENTICATION_MODE	authmode;

	_func_enter_;
/*
#ifdef CONFIG_CONCURRENT_MODE
	if(padapter->iface_type > PRIMARY_IFACE)
	{
		ret = -EINVAL;
		goto exit;
	}
#endif
*/

#ifdef CONFIG_CONCURRENT_MODE
	if (check_buddy_fwstate(padapter, _FW_UNDER_SURVEY|_FW_UNDER_LINKING) == _TRUE)
	{
		DBG_871X("set bssid, but buddy_intf is under scanning or linking\n");

		ret = -EINVAL;

		goto exit;
	}
#endif

	rtw_ps_deny(padapter, PS_DENY_JOIN);
	if(_FAIL == rtw_pwr_wakeup(padapter))
	{
		ret= -1;
		goto exit;
	}

	if(!padapter->bup){
		ret = -1;
		goto exit;
	}


	if (temp->sa_family != ARPHRD_ETHER){
		ret = -EINVAL;
		goto exit;
	}

	authmode = padapter->securitypriv.ndisauthtype;
	_enter_critical_bh(&queue->lock, &irqL);
       phead = get_list_head(queue);
       pmlmepriv->pscanned = get_next(phead);

	while (1)
	 {

		if ((rtw_end_of_queue_search(phead, pmlmepriv->pscanned)) == _TRUE)
		{
#if 0
			ret = -EINVAL;
			goto exit;

			if(check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE)
			{
	            		rtw_set_802_11_bssid(padapter, temp->sa_data);
	    			goto exit;
			}
			else
			{
				ret = -EINVAL;
				goto exit;
			}
#endif

			break;
		}

		pnetwork = LIST_CONTAINOR(pmlmepriv->pscanned, struct wlan_network, list);

		pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

		dst_bssid = pnetwork->network.MacAddress;

		src_bssid = temp->sa_data;

		if ((_rtw_memcmp(dst_bssid, src_bssid, ETH_ALEN)) == _TRUE)
		{
			if(!rtw_set_802_11_infrastructure_mode(padapter, pnetwork->network.InfrastructureMode))
			{
				ret = -1;
				_exit_critical_bh(&queue->lock, &irqL);
				goto exit;
			}

				break;
		}

	}
	_exit_critical_bh(&queue->lock, &irqL);

	rtw_set_802_11_authentication_mode(padapter, authmode);
	//set_802_11_encryption_mode(padapter, padapter->securitypriv.ndisencryptstatus);
	if (rtw_set_802_11_bssid(padapter, temp->sa_data) == _FALSE) {
		ret = -1;
		goto exit;
	}

exit:

	rtw_ps_deny_cancel(padapter, PS_DENY_JOIN);

	_func_exit_;

	return ret;
}

static int rtw_wx_get_wap(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{

	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	WLAN_BSSID_EX  *pcur_bss = &pmlmepriv->cur_network.network;

	wrqu->ap_addr.sa_family = ARPHRD_ETHER;

	_rtw_memset(wrqu->ap_addr.sa_data, 0, ETH_ALEN);

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_wap\n"));

	_func_enter_;

	if  ( ((check_fwstate(pmlmepriv, _FW_LINKED)) == _TRUE) ||
			((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE)) == _TRUE) ||
			((check_fwstate(pmlmepriv, WIFI_AP_STATE)) == _TRUE) )
	{

		_rtw_memcpy(wrqu->ap_addr.sa_data, pcur_bss->MacAddress, ETH_ALEN);
	}
	else
	{
	 	_rtw_memset(wrqu->ap_addr.sa_data, 0, ETH_ALEN);
	}

	_func_exit_;

	return 0;

}

static int rtw_wx_set_mlme(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
#if 0
/* SIOCSIWMLME data */
struct	iw_mlme
{
	__u16		cmd; /* IW_MLME_* */
	__u16		reason_code;
	struct sockaddr	addr;
};
#endif

	int ret=0;
	u16 reason;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct iw_mlme *mlme = (struct iw_mlme *) extra;


	if(mlme==NULL)
		return -1;

	DBG_871X("%s\n", __FUNCTION__);

	reason = cpu_to_le16(mlme->reason_code);


	DBG_871X("%s, cmd=%d, reason=%d\n", __FUNCTION__, mlme->cmd, reason);


	switch (mlme->cmd)
	{
		case IW_MLME_DEAUTH:
				if(!rtw_set_802_11_disassociate(padapter))
					ret = -1;
				break;

		case IW_MLME_DISASSOC:
				if(!rtw_set_802_11_disassociate(padapter))
					ret = -1;

				break;

		default:
			return -EOPNOTSUPP;
	}

	return ret;
}

static int rtw_wx_set_scan(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *extra)
{
	u8 _status = _FALSE;
	int ret = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv= &padapter->mlmepriv;
	NDIS_802_11_SSID ssid[RTW_SSID_SCAN_AMOUNT];
	_irqL	irqL;
#ifdef CONFIG_P2P
	struct wifidirect_info *pwdinfo= &(padapter->wdinfo);
#endif //CONFIG_P2P
	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_set_scan\n"));

_func_enter_;

	#ifdef DBG_IOCTL
	DBG_871X("DBG_IOCTL %s:%d\n",__FUNCTION__, __LINE__);
	#endif
/*
#ifdef CONFIG_CONCURRENT_MODE
	if(padapter->iface_type > PRIMARY_IFACE)
	{
		ret = -1;
		goto exit;
	}
#endif
*/
#ifdef CONFIG_MP_INCLUDED
		if (padapter->registrypriv.mp_mode == 1)
		{
				DBG_871X(FUNC_ADPT_FMT ": MP mode block Scan request\n", FUNC_ADPT_ARG(padapter));
				ret = -1;
				goto exit;
		}
#ifdef CONFIG_CONCURRENT_MODE
		if (padapter->pbuddy_adapter) {
				if (padapter->pbuddy_adapter->registrypriv.mp_mode == 1)
				{
					DBG_871X(FUNC_ADPT_FMT ": MP mode block Scan request\n", FUNC_ADPT_ARG(padapter->pbuddy_adapter));
					ret = -1;
					goto exit;
				}
		}
#endif //CONFIG_CONCURRENT_MODE
#endif

	rtw_ps_deny(padapter, PS_DENY_SCAN);
	if(_FAIL == rtw_pwr_wakeup(padapter))
	{
		ret= -1;
		goto exit;
	}

	if (rtw_is_drv_stopped(padapter)) {
		DBG_871X("%s bDriverStopped=_TRUE\n", __func__);
		ret= -1;
		goto exit;
	}

	if(!padapter->bup){
		ret = -1;
		goto exit;
	}

	if (!rtw_is_hw_init_completed(padapter)) {
		ret = -1;
		goto exit;
	}

#ifndef CONFIG_DOSCAN_IN_BUSYTRAFFIC
	// When Busy Traffic, driver do not site survey. So driver return success.
	// wpa_supplicant will not issue SIOCSIWSCAN cmd again after scan timeout.
	// modify by thomas 2011-02-22.
	if (pmlmepriv->LinkDetectInfo.bBusyTraffic == _TRUE
#ifdef CONFIG_CONCURRENT_MODE
	|| rtw_get_buddy_bBusyTraffic(padapter) == _TRUE
#endif //CONFIG_CONCURRENT_MODE
	)
	{
		indicate_wx_scan_complete_event(padapter);
		goto exit;
	}
#endif

	if (check_fwstate(pmlmepriv, _FW_UNDER_SURVEY|_FW_UNDER_LINKING) == _TRUE)
	{
		indicate_wx_scan_complete_event(padapter);
		goto exit;
	}

#ifdef CONFIG_CONCURRENT_MODE
	if (check_buddy_fwstate(padapter,
		_FW_UNDER_SURVEY|_FW_UNDER_LINKING|WIFI_UNDER_WPS) == _TRUE)
	{
		indicate_wx_scan_complete_event(padapter);
		goto exit;
	}
#endif

#ifdef CONFIG_P2P
	if ( pwdinfo->p2p_state != P2P_STATE_NONE )
	{
		rtw_p2p_set_pre_state( pwdinfo, rtw_p2p_state( pwdinfo ) );
		rtw_p2p_set_state(pwdinfo, P2P_STATE_FIND_PHASE_SEARCH);
		rtw_p2p_findphase_ex_set(pwdinfo, P2P_FINDPHASE_EX_FULL);
		rtw_free_network_queue(padapter, _TRUE);
	}
#endif //CONFIG_P2P

	_rtw_memset(ssid, 0, sizeof(NDIS_802_11_SSID)*RTW_SSID_SCAN_AMOUNT);

#if WIRELESS_EXT >= 17
	if (wrqu->data.length == sizeof(struct iw_scan_req))
	{
		struct iw_scan_req *req = (struct iw_scan_req *)extra;

		if (wrqu->data.flags & IW_SCAN_THIS_ESSID)
		{
			int len = min((int)req->essid_len, IW_ESSID_MAX_SIZE);

			_rtw_memcpy(ssid[0].Ssid, req->essid, len);
			ssid[0].SsidLength = len;

			DBG_871X("IW_SCAN_THIS_ESSID, ssid=%s, len=%d\n", req->essid, req->essid_len);

			_enter_critical_bh(&pmlmepriv->lock, &irqL);

			_status = rtw_sitesurvey_cmd(padapter, ssid, 1, NULL, 0);

			_exit_critical_bh(&pmlmepriv->lock, &irqL);

		}
		else if (req->scan_type == IW_SCAN_TYPE_PASSIVE)
		{
			DBG_871X("rtw_wx_set_scan, req->scan_type == IW_SCAN_TYPE_PASSIVE\n");
		}

	}
	else
#endif

	if(	wrqu->data.length >= WEXT_CSCAN_HEADER_SIZE
		&& _rtw_memcmp(extra, WEXT_CSCAN_HEADER, WEXT_CSCAN_HEADER_SIZE) == _TRUE
	)
	{
		int len = wrqu->data.length -WEXT_CSCAN_HEADER_SIZE;
		char *pos = extra+WEXT_CSCAN_HEADER_SIZE;
		char section;
		char sec_len;
		int ssid_index = 0;

		//DBG_871X("%s COMBO_SCAN header is recognized\n", __FUNCTION__);

		while(len >= 1) {
			section = *(pos++); len-=1;

			switch(section) {
				case WEXT_CSCAN_SSID_SECTION:
					//DBG_871X("WEXT_CSCAN_SSID_SECTION\n");
					if(len < 1) {
						len = 0;
						break;
					}

					sec_len = *(pos++); len-=1;

					if(sec_len>0 && sec_len<=len) {
						ssid[ssid_index].SsidLength = sec_len;
						_rtw_memcpy(ssid[ssid_index].Ssid, pos, ssid[ssid_index].SsidLength);
						//DBG_871X("%s COMBO_SCAN with specific ssid:%s, %d\n", __FUNCTION__
						//	, ssid[ssid_index].Ssid, ssid[ssid_index].SsidLength);
						ssid_index++;
					}

					pos+=sec_len; len-=sec_len;
					break;


				case WEXT_CSCAN_CHANNEL_SECTION:
					//DBG_871X("WEXT_CSCAN_CHANNEL_SECTION\n");
					pos+=1; len-=1;
					break;
				case WEXT_CSCAN_ACTV_DWELL_SECTION:
					//DBG_871X("WEXT_CSCAN_ACTV_DWELL_SECTION\n");
					pos+=2; len-=2;
					break;
				case WEXT_CSCAN_PASV_DWELL_SECTION:
					//DBG_871X("WEXT_CSCAN_PASV_DWELL_SECTION\n");
					pos+=2; len-=2;
					break;
				case WEXT_CSCAN_HOME_DWELL_SECTION:
					//DBG_871X("WEXT_CSCAN_HOME_DWELL_SECTION\n");
					pos+=2; len-=2;
					break;
				case WEXT_CSCAN_TYPE_SECTION:
					//DBG_871X("WEXT_CSCAN_TYPE_SECTION\n");
					pos+=1; len-=1;
					break;
				#if 0
				case WEXT_CSCAN_NPROBE_SECTION:
					DBG_871X("WEXT_CSCAN_NPROBE_SECTION\n");
					break;
				#endif

				default:
					//DBG_871X("Unknown CSCAN section %c\n", section);
					len = 0; // stop parsing
			}
			//DBG_871X("len:%d\n", len);

		}

		//jeff: it has still some scan paramater to parse, we only do this now...
		_status = rtw_set_802_11_bssid_list_scan(padapter, ssid, RTW_SSID_SCAN_AMOUNT);

	} else

	{
		_status = rtw_set_802_11_bssid_list_scan(padapter, NULL, 0);
	}

	if(_status == _FALSE)
		ret = -1;

exit:

	rtw_ps_deny_cancel(padapter, PS_DENY_SCAN);

	#ifdef DBG_IOCTL
	DBG_871X("DBG_IOCTL %s:%d return %d\n",__FUNCTION__, __LINE__, ret);
	#endif

_func_exit_;

	return ret;
}

static int rtw_wx_get_scan(struct net_device *dev, struct iw_request_info *a,
			     union iwreq_data *wrqu, char *extra)
{
	_irqL	irqL;
	_list					*plist, *phead;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	_queue				*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	char *ev = extra;
	char *stop = ev + wrqu->data.length;
	u32 ret = 0;
	u32 cnt=0;
	u32 wait_for_surveydone;
	sint wait_status;
#ifdef CONFIG_CONCURRENT_MODE
	//PADAPTER pbuddy_adapter = padapter->pbuddy_adapter;
	//struct mlme_priv *pbuddy_mlmepriv = &(pbuddy_adapter->mlmepriv);
#endif
#ifdef CONFIG_P2P
	struct	wifidirect_info*	pwdinfo = &padapter->wdinfo;
#endif //CONFIG_P2P
	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_scan\n"));
	RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_, (" Start of Query SIOCGIWSCAN .\n"));

	_func_enter_;

	#ifdef DBG_IOCTL
	DBG_871X("DBG_IOCTL %s:%d\n",__FUNCTION__, __LINE__);
	#endif
/*
#ifdef CONFIG_CONCURRENT_MODE
	if(padapter->iface_type > PRIMARY_IFACE)
	{
		ret = -EINVAL;
		goto exit;
	}
#endif
*/
	if (adapter_to_pwrctl(padapter)->brfoffbyhw && rtw_is_drv_stopped(padapter)) {
		ret = -EINVAL;
		goto exit;
	}

#ifdef CONFIG_P2P
	if(!rtw_p2p_chk_state(pwdinfo, P2P_STATE_NONE))
	{
		wait_for_surveydone = 200;
	}
	else
	{
		//	P2P is disabled
		wait_for_surveydone = 100;
	}
#else
	{
		wait_for_surveydone = 100;
	}
#endif //CONFIG_P2P

#if 1 // Wireless Extension use EAGAIN to try
	wait_status = _FW_UNDER_SURVEY
#ifndef CONFIG_ANDROID
		| _FW_UNDER_LINKING
#endif
	;

	while (check_fwstate(pmlmepriv, wait_status) == _TRUE)
	{
		return -EAGAIN;
	}
#else
	wait_status = _FW_UNDER_SURVEY
		#ifndef CONFIG_ANDROID
		|_FW_UNDER_LINKING
		#endif
	;

 	while(check_fwstate(pmlmepriv, wait_status) == _TRUE)
	{
		rtw_msleep_os(30);
		cnt++;
		if(cnt > wait_for_surveydone )
			break;
	}
#endif
	_enter_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

	phead = get_list_head(queue);
	plist = get_next(phead);

	while(1)
	{
		if (rtw_end_of_queue_search(phead,plist)== _TRUE)
			break;

		if((stop - ev) < SCAN_ITEM_SIZE) {
			ret = -E2BIG;
			break;
		}

		pnetwork = LIST_CONTAINOR(plist, struct wlan_network, list);

		//report network only if the current channel set contains the channel to which this network belongs
		if(rtw_ch_set_search_ch(padapter->mlmeextpriv.channel_set, pnetwork->network.Configuration.DSConfig) >= 0
			&& rtw_mlme_band_check(padapter, pnetwork->network.Configuration.DSConfig) == _TRUE
			&& _TRUE == rtw_validate_ssid(&(pnetwork->network.Ssid))
		)
		{
			ev=translate_scan(padapter, a, pnetwork, ev, stop);
		}

		plist = get_next(plist);

	}

	_exit_critical_bh(&(pmlmepriv->scanned_queue.lock), &irqL);

       wrqu->data.length = ev-extra;
	wrqu->data.flags = 0;

exit:

	_func_exit_;

	#ifdef DBG_IOCTL
	DBG_871X("DBG_IOCTL %s:%d return %d\n",__FUNCTION__, __LINE__, ret);
	#endif

	return ret ;

}

//set ssid flow
//s1. rtw_set_802_11_infrastructure_mode()
//s2. set_802_11_authenticaion_mode()
//s3. set_802_11_encryption_mode()
//s4. rtw_set_802_11_ssid()
static int rtw_wx_set_essid(struct net_device *dev,
			      struct iw_request_info *a,
			      union iwreq_data *wrqu, char *extra)
{
	_irqL irqL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *queue = &pmlmepriv->scanned_queue;
	_list *phead;
	s8 status = _TRUE;
	struct wlan_network *pnetwork = NULL;
	NDIS_802_11_AUTHENTICATION_MODE authmode;
	NDIS_802_11_SSID ndis_ssid;
	u8 *dst_ssid, *src_ssid;

	uint ret = 0, len;

	_func_enter_;

	#ifdef DBG_IOCTL
	DBG_871X("DBG_IOCTL %s:%d\n",__FUNCTION__, __LINE__);
	#endif
	#ifdef CONFIG_WEXT_DONT_JOIN_BYSSID
	DBG_871X("%s: CONFIG_WEXT_DONT_JOIN_BYSSID be defined!! only allow bssid joining\n", __func__);
	return -EPERM;
	#endif
/*
#ifdef CONFIG_CONCURRENT_MODE
	if(padapter->iface_type > PRIMARY_IFACE)
	{
		ret = -EINVAL;
		goto exit;
	}
#endif
*/

#ifdef CONFIG_CONCURRENT_MODE
	if (check_buddy_fwstate(padapter, _FW_UNDER_SURVEY|_FW_UNDER_LINKING) == _TRUE)
	{
		DBG_871X("set ssid, but buddy_intf is under scanning or linking\n");

		ret = -EINVAL;

		goto exit;
	}
#endif

	RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
		 ("+rtw_wx_set_essid: fw_state=0x%08x\n", get_fwstate(pmlmepriv)));

	rtw_ps_deny(padapter, PS_DENY_JOIN);
	if(_FAIL == rtw_pwr_wakeup(padapter))
	{
		ret = -1;
		goto exit;
	}

	if(!padapter->bup){
		ret = -1;
		goto exit;
	}

#if WIRELESS_EXT <= 20
	if ((wrqu->essid.length-1) > IW_ESSID_MAX_SIZE){
#else
	if (wrqu->essid.length > IW_ESSID_MAX_SIZE){
#endif
		ret= -E2BIG;
		goto exit;
	}

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE)) {
		ret = -1;
		goto exit;
	}

	authmode = padapter->securitypriv.ndisauthtype;
	DBG_871X("=>%s\n",__FUNCTION__);
	if (wrqu->essid.flags && wrqu->essid.length)
	{
		// Commented by Albert 20100519
		// We got the codes in "set_info" function of iwconfig source code.
		//	=========================================
		//	wrq.u.essid.length = strlen(essid) + 1;
	  	//	if(we_kernel_version > 20)
		//		wrq.u.essid.length--;
		//	=========================================
		//	That means, if the WIRELESS_EXT less than or equal to 20, the correct ssid len should subtract 1.
#if WIRELESS_EXT <= 20
		len = ((wrqu->essid.length-1) < IW_ESSID_MAX_SIZE) ? (wrqu->essid.length-1) : IW_ESSID_MAX_SIZE;
#else
		len = (wrqu->essid.length < IW_ESSID_MAX_SIZE) ? wrqu->essid.length : IW_ESSID_MAX_SIZE;
#endif

		if( wrqu->essid.length != 33 )
			DBG_871X("ssid=%s, len=%d\n", extra, wrqu->essid.length);

		_rtw_memset(&ndis_ssid, 0, sizeof(NDIS_802_11_SSID));
		ndis_ssid.SsidLength = len;
		_rtw_memcpy(ndis_ssid.Ssid, extra, len);
		src_ssid = ndis_ssid.Ssid;

		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_, ("rtw_wx_set_essid: ssid=[%s]\n", src_ssid));
		_enter_critical_bh(&queue->lock, &irqL);
	       phead = get_list_head(queue);
              pmlmepriv->pscanned = get_next(phead);

		while (1)
		{
			if (rtw_end_of_queue_search(phead, pmlmepriv->pscanned) == _TRUE)
			{
#if 0
				if(check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE)
				{
	            			rtw_set_802_11_ssid(padapter, &ndis_ssid);

		    			goto exit;
				}
				else
				{
					RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("rtw_wx_set_ssid(): scanned_queue is empty\n"));
					ret = -EINVAL;
					goto exit;
				}
#endif
			        RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_warning_,
					 ("rtw_wx_set_essid: scan_q is empty, set ssid to check if scanning again!\n"));

				break;
			}

			pnetwork = LIST_CONTAINOR(pmlmepriv->pscanned, struct wlan_network, list);

			pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

			dst_ssid = pnetwork->network.Ssid.Ssid;

			RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
				 ("rtw_wx_set_essid: dst_ssid=%s\n",
				  pnetwork->network.Ssid.Ssid));

			if ((_rtw_memcmp(dst_ssid, src_ssid, ndis_ssid.SsidLength) == _TRUE) &&
				(pnetwork->network.Ssid.SsidLength==ndis_ssid.SsidLength))
			{
				RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
					 ("rtw_wx_set_essid: find match, set infra mode\n"));

				if(check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE)
				{
					if(pnetwork->network.InfrastructureMode != pmlmepriv->cur_network.network.InfrastructureMode)
						continue;
				}

				if (rtw_set_802_11_infrastructure_mode(padapter, pnetwork->network.InfrastructureMode) == _FALSE)
				{
					ret = -1;
					_exit_critical_bh(&queue->lock, &irqL);
					goto exit;
				}

				break;
			}
		}
		_exit_critical_bh(&queue->lock, &irqL);
		RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_info_,
			 ("set ssid: set_802_11_auth. mode=%d\n", authmode));
		rtw_set_802_11_authentication_mode(padapter, authmode);
		//set_802_11_encryption_mode(padapter, padapter->securitypriv.ndisencryptstatus);
		if (rtw_set_802_11_ssid(padapter, &ndis_ssid) == _FALSE) {
			ret = -1;
			goto exit;
		}
	}

exit:

	rtw_ps_deny_cancel(padapter, PS_DENY_JOIN);

	DBG_871X("<=%s, ret %d\n",__FUNCTION__, ret);

	#ifdef DBG_IOCTL
	DBG_871X("DBG_IOCTL %s:%d return %d\n",__FUNCTION__, __LINE__, ret);
	#endif

	_func_exit_;

	return ret;
}

static int rtw_wx_get_essid(struct net_device *dev,
			      struct iw_request_info *a,
			      union iwreq_data *wrqu, char *extra)
{
	u32 len,ret = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	WLAN_BSSID_EX  *pcur_bss = &pmlmepriv->cur_network.network;

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,("rtw_wx_get_essid\n"));

	_func_enter_;

	if ( (check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) ||
	      (check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) == _TRUE))
	{
		len = pcur_bss->Ssid.SsidLength;

		wrqu->essid.length = len;

		_rtw_memcpy(extra, pcur_bss->Ssid.Ssid, len);

		wrqu->essid.flags = 1;
	}
	else
	{
		ret = -1;
		goto exit;
	}

exit:

	_func_exit_;

	return ret;

}

static int rtw_wx_set_rate(struct net_device *dev,
			      struct iw_request_info *a,
			      union iwreq_data *wrqu, char *extra)
{
	int	i, ret = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	u8	datarates[NumRates];
	u32	target_rate = wrqu->bitrate.value;
	u32	fixed = wrqu->bitrate.fixed;
	u32	ratevalue = 0;
	 u8 mpdatarate[NumRates]={11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0xff};

_func_enter_;

	RT_TRACE(_module_rtl871x_mlme_c_,_drv_info_,(" rtw_wx_set_rate \n"));
	RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("target_rate = %d, fixed = %d\n",target_rate,fixed));

	if(target_rate == -1){
		ratevalue = 11;
		goto set_rate;
	}
	target_rate = target_rate/100000;

	switch(target_rate){
		case 10:
			ratevalue = 0;
			break;
		case 20:
			ratevalue = 1;
			break;
		case 55:
			ratevalue = 2;
			break;
		case 60:
			ratevalue = 3;
			break;
		case 90:
			ratevalue = 4;
			break;
		case 110:
			ratevalue = 5;
			break;
		case 120:
			ratevalue = 6;
			break;
		case 180:
			ratevalue = 7;
			break;
		case 240:
			ratevalue = 8;
			break;
		case 360:
			ratevalue = 9;
			break;
		case 480:
			ratevalue = 10;
			break;
		case 540:
			ratevalue = 11;
			break;
		default:
			ratevalue = 11;
			break;
	}

set_rate:

	for(i=0; i<NumRates; i++)
	{
		if(ratevalue==mpdatarate[i])
		{
			datarates[i] = mpdatarate[i];
			if(fixed == 0)
				break;
		}
		else{
			datarates[i] = 0xff;
		}

		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("datarate_inx=%d\n",datarates[i]));
	}

	if( rtw_setdatarate_cmd(padapter, datarates) !=_SUCCESS){
		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_err_,("rtw_wx_set_rate Fail!!!\n"));
		ret = -1;
	}

_func_exit_;

	return ret;
}

static int rtw_wx_get_rate(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	u16 max_rate = 0;

	max_rate = rtw_get_cur_max_rate((_adapter *)rtw_netdev_priv(dev));

	if(max_rate == 0)
		return -EPERM;

	wrqu->bitrate.fixed = 0;	/* no auto select */
	wrqu->bitrate.value = max_rate * 100000;

	return 0;
}

static int rtw_wx_set_rts(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	_func_enter_;

	if (wrqu->rts.disabled)
		padapter->registrypriv.rts_thresh = 2347;
	else {
		if (wrqu->rts.value < 0 ||
		    wrqu->rts.value > 2347)
			return -EINVAL;

		padapter->registrypriv.rts_thresh = wrqu->rts.value;
	}

	DBG_871X("%s, rts_thresh=%d\n", __func__, padapter->registrypriv.rts_thresh);

	_func_exit_;

	return 0;

}

static int rtw_wx_get_rts(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	_func_enter_;

	DBG_871X("%s, rts_thresh=%d\n", __func__, padapter->registrypriv.rts_thresh);

	wrqu->rts.value = padapter->registrypriv.rts_thresh;
	wrqu->rts.fixed = 0;	/* no auto select */
	//wrqu->rts.disabled = (wrqu->rts.value == DEFAULT_RTS_THRESHOLD);

	_func_exit_;

	return 0;
}

static int rtw_wx_set_frag(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	_func_enter_;

	if (wrqu->frag.disabled)
		padapter->xmitpriv.frag_len = MAX_FRAG_THRESHOLD;
	else {
		if (wrqu->frag.value < MIN_FRAG_THRESHOLD ||
		    wrqu->frag.value > MAX_FRAG_THRESHOLD)
			return -EINVAL;

		padapter->xmitpriv.frag_len = wrqu->frag.value & ~0x1;
	}

	DBG_871X("%s, frag_len=%d\n", __func__, padapter->xmitpriv.frag_len);

	_func_exit_;

	return 0;

}

static int rtw_wx_get_frag(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	_func_enter_;

	DBG_871X("%s, frag_len=%d\n", __func__, padapter->xmitpriv.frag_len);

	wrqu->frag.value = padapter->xmitpriv.frag_len;
	wrqu->frag.fixed = 0;	/* no auto select */
	//wrqu->frag.disabled = (wrqu->frag.value == DEFAULT_FRAG_THRESHOLD);

	_func_exit_;

	return 0;
}

static int rtw_wx_get_retry(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	//_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);


	wrqu->retry.value = 7;
	wrqu->retry.fixed = 0;	/* no auto select */
	wrqu->retry.disabled = 1;

	return 0;

}

#if 0
#define IW_ENCODE_INDEX		0x00FF	/* Token index (if needed) */
#define IW_ENCODE_FLAGS		0xFF00	/* Flags defined below */
#define IW_ENCODE_MODE		0xF000	/* Modes defined below */
#define IW_ENCODE_DISABLED	0x8000	/* Encoding disabled */
#define IW_ENCODE_ENABLED	0x0000	/* Encoding enabled */
#define IW_ENCODE_RESTRICTED	0x4000	/* Refuse non-encoded packets */
#define IW_ENCODE_OPEN		0x2000	/* Accept non-encoded packets */
#define IW_ENCODE_NOKEY		0x0800  /* Key is write only, so not present */
#define IW_ENCODE_TEMP		0x0400  /* Temporary key */
/*
iwconfig wlan0 key on -> flags = 0x6001 -> maybe it means auto
iwconfig wlan0 key off -> flags = 0x8800
iwconfig wlan0 key open -> flags = 0x2800
iwconfig wlan0 key open 1234567890 -> flags = 0x2000
iwconfig wlan0 key restricted -> flags = 0x4800
iwconfig wlan0 key open [3] 1234567890 -> flags = 0x2003
iwconfig wlan0 key restricted [2] 1234567890 -> flags = 0x4002
iwconfig wlan0 key open [3] -> flags = 0x2803
iwconfig wlan0 key restricted [2] -> flags = 0x4802
*/
#endif

static int rtw_wx_set_enc(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *keybuf)
{
	u32 key, ret = 0;
	u32 keyindex_provided;
	NDIS_802_11_WEP	 wep;
	NDIS_802_11_AUTHENTICATION_MODE authmode;

	struct iw_point *erq = &(wrqu->encoding);
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
	DBG_871X("+rtw_wx_set_enc, flags=0x%x\n", erq->flags);

	_rtw_memset(&wep, 0, sizeof(NDIS_802_11_WEP));

	key = erq->flags & IW_ENCODE_INDEX;

	_func_enter_;

	if (erq->flags & IW_ENCODE_DISABLED)
	{
		DBG_871X("EncryptionDisabled\n");
		padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
		padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
		padapter->securitypriv.dot118021XGrpPrivacy=_NO_PRIVACY_;
		padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_Open; //open system
  		authmode = Ndis802_11AuthModeOpen;
		padapter->securitypriv.ndisauthtype=authmode;

		goto exit;
	}

	if (key) {
		if (key > WEP_KEYS)
			return -EINVAL;
		key--;
		keyindex_provided = 1;
	}
	else
	{
		keyindex_provided = 0;
		key = padapter->securitypriv.dot11PrivacyKeyIndex;
		DBG_871X("rtw_wx_set_enc, key=%d\n", key);
	}

	//set authentication mode
	if(erq->flags & IW_ENCODE_OPEN)
	{
		DBG_871X("rtw_wx_set_enc():IW_ENCODE_OPEN\n");
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;//Ndis802_11EncryptionDisabled;

		padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_Open;

		padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
		padapter->securitypriv.dot118021XGrpPrivacy=_NO_PRIVACY_;
  		authmode = Ndis802_11AuthModeOpen;
		padapter->securitypriv.ndisauthtype=authmode;
	}
	else if(erq->flags & IW_ENCODE_RESTRICTED)
	{
		DBG_871X("rtw_wx_set_enc():IW_ENCODE_RESTRICTED\n");
		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;

		padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_Shared;

		padapter->securitypriv.dot11PrivacyAlgrthm=_WEP40_;
		padapter->securitypriv.dot118021XGrpPrivacy=_WEP40_;
		authmode = Ndis802_11AuthModeShared;
		padapter->securitypriv.ndisauthtype=authmode;
	}
	else
	{
		DBG_871X("rtw_wx_set_enc():erq->flags=0x%x\n", erq->flags);

		padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption1Enabled;//Ndis802_11EncryptionDisabled;
		padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_Open; //open system
		padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
		padapter->securitypriv.dot118021XGrpPrivacy=_NO_PRIVACY_;
  		authmode = Ndis802_11AuthModeOpen;
		padapter->securitypriv.ndisauthtype=authmode;
	}

	wep.KeyIndex = key;
	if (erq->length > 0)
	{
		wep.KeyLength = erq->length <= 5 ? 5 : 13;

		wep.Length = wep.KeyLength + FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial);
	}
	else
	{
		wep.KeyLength = 0 ;

		if(keyindex_provided == 1)// set key_id only, no given KeyMaterial(erq->length==0).
		{
			padapter->securitypriv.dot11PrivacyKeyIndex = key;

			DBG_871X("(keyindex_provided == 1), keyid=%d, key_len=%d\n", key, padapter->securitypriv.dot11DefKeylen[key]);

			switch(padapter->securitypriv.dot11DefKeylen[key])
			{
				case 5:
					padapter->securitypriv.dot11PrivacyAlgrthm=_WEP40_;
					break;
				case 13:
					padapter->securitypriv.dot11PrivacyAlgrthm=_WEP104_;
					break;
				default:
					padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
					break;
			}

			goto exit;

		}

	}

	wep.KeyIndex |= 0x80000000;

	_rtw_memcpy(wep.KeyMaterial, keybuf, wep.KeyLength);

	if (rtw_set_802_11_add_wep(padapter, &wep) == _FALSE) {
		if(rf_on == pwrpriv->rf_pwrstate )
			ret = -EOPNOTSUPP;
		goto exit;
	}

exit:

	_func_exit_;

	return ret;

}

static int rtw_wx_get_enc(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *keybuf)
{
	uint key, ret =0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct iw_point *erq = &(wrqu->encoding);
	struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);

	_func_enter_;

	if(check_fwstate(pmlmepriv, _FW_LINKED) != _TRUE)
	{
		 if(check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) != _TRUE)
		 {
		erq->length = 0;
		erq->flags |= IW_ENCODE_DISABLED;
		return 0;
	}
	}


	key = erq->flags & IW_ENCODE_INDEX;

	if (key) {
		if (key > WEP_KEYS)
			return -EINVAL;
		key--;
	} else
	{
		key = padapter->securitypriv.dot11PrivacyKeyIndex;
	}

	erq->flags = key + 1;

	//if(padapter->securitypriv.ndisauthtype == Ndis802_11AuthModeOpen)
	//{
	//      erq->flags |= IW_ENCODE_OPEN;
	//}

	switch(padapter->securitypriv.ndisencryptstatus)
	{
		case Ndis802_11EncryptionNotSupported:
		case Ndis802_11EncryptionDisabled:

		erq->length = 0;
		erq->flags |= IW_ENCODE_DISABLED;

		break;

		case Ndis802_11Encryption1Enabled:

		erq->length = padapter->securitypriv.dot11DefKeylen[key];

		if(erq->length)
		{
			_rtw_memcpy(keybuf, padapter->securitypriv.dot11DefKey[key].skey, padapter->securitypriv.dot11DefKeylen[key]);

		erq->flags |= IW_ENCODE_ENABLED;

			if(padapter->securitypriv.ndisauthtype == Ndis802_11AuthModeOpen)
			{
	     			erq->flags |= IW_ENCODE_OPEN;
			}
			else if(padapter->securitypriv.ndisauthtype == Ndis802_11AuthModeShared)
			{
		erq->flags |= IW_ENCODE_RESTRICTED;
			}
		}
		else
		{
			erq->length = 0;
			erq->flags |= IW_ENCODE_DISABLED;
		}

		break;

		case Ndis802_11Encryption2Enabled:
		case Ndis802_11Encryption3Enabled:

		erq->length = 16;
		erq->flags |= (IW_ENCODE_ENABLED | IW_ENCODE_OPEN | IW_ENCODE_NOKEY);

		break;

		default:
		erq->length = 0;
		erq->flags |= IW_ENCODE_DISABLED;

		break;

	}

	_func_exit_;

	return ret;

}

static int rtw_wx_get_power(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	//_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	wrqu->power.value = 0;
	wrqu->power.fixed = 0;	/* no auto select */
	wrqu->power.disabled = 1;

	return 0;

}

static int rtw_wx_set_gen_ie(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int ret;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

       ret = rtw_set_wpa_ie(padapter, extra, wrqu->data.length);

	return ret;
}

static int rtw_wx_set_auth(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct iw_param *param = (struct iw_param*)&(wrqu->param);
	struct mlme_priv 	*pmlmepriv = &padapter->mlmepriv;
	struct security_priv *psecuritypriv = &padapter->securitypriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u32 value = param->value;
	int ret = 0;

	switch (param->flags & IW_AUTH_INDEX) {

	case IW_AUTH_WPA_VERSION:
#ifdef CONFIG_WAPI_SUPPORT
#ifndef CONFIG_IOCTL_CFG80211
		 padapter->wapiInfo.bWapiEnable = false;
		 if(value == IW_AUTH_WAPI_VERSION_1)
		 {
			padapter->wapiInfo.bWapiEnable = true;
			psecuritypriv->dot11PrivacyAlgrthm = _SMS4_;
			psecuritypriv->dot118021XGrpPrivacy = _SMS4_;
			psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_WAPI;
			pmlmeinfo->auth_algo = psecuritypriv->dot11AuthAlgrthm;
			padapter->wapiInfo.extra_prefix_len = WAPI_EXT_LEN;
			padapter->wapiInfo.extra_postfix_len = SMS4_MIC_LEN;
		}
#endif
#endif
		break;
	case IW_AUTH_CIPHER_PAIRWISE:

		break;
	case IW_AUTH_CIPHER_GROUP:

		break;
	case IW_AUTH_KEY_MGMT:
#ifdef CONFIG_WAPI_SUPPORT
#ifndef CONFIG_IOCTL_CFG80211
		DBG_871X("rtw_wx_set_auth: IW_AUTH_KEY_MGMT case \n");
		if(value == IW_AUTH_KEY_MGMT_WAPI_PSK)
			padapter->wapiInfo.bWapiPSK = true;
		else
			padapter->wapiInfo.bWapiPSK = false;
		DBG_871X("rtw_wx_set_auth: IW_AUTH_KEY_MGMT bwapipsk %d \n",padapter->wapiInfo.bWapiPSK);
#endif
#endif
		/*
		 *  ??? does not use these parameters
		 */
		break;

	case IW_AUTH_TKIP_COUNTERMEASURES:
        {
	    if ( param->value )
            {  // wpa_supplicant is enabling the tkip countermeasure.
               padapter->securitypriv.btkip_countermeasure = _TRUE;
            }
            else
            {  // wpa_supplicant is disabling the tkip countermeasure.
               padapter->securitypriv.btkip_countermeasure = _FALSE;
            }
		break;
        }
	case IW_AUTH_DROP_UNENCRYPTED:
		{
			/* HACK:
			 *
			 * wpa_supplicant calls set_wpa_enabled when the driver
			 * is loaded and unloaded, regardless of if WPA is being
			 * used.  No other calls are made which can be used to
			 * determine if encryption will be used or not prior to
			 * association being expected.  If encryption is not being
			 * used, drop_unencrypted is set to false, else true -- we
			 * can use this to determine if the CAP_PRIVACY_ON bit should
			 * be set.
			 */

			if(padapter->securitypriv.ndisencryptstatus == Ndis802_11Encryption1Enabled)
			{
				break;//it means init value, or using wep, ndisencryptstatus = Ndis802_11Encryption1Enabled,
						// then it needn't reset it;
			}

			if(param->value){
				padapter->securitypriv.ndisencryptstatus = Ndis802_11EncryptionDisabled;
				padapter->securitypriv.dot11PrivacyAlgrthm=_NO_PRIVACY_;
				padapter->securitypriv.dot118021XGrpPrivacy=_NO_PRIVACY_;
				padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_Open; //open system
				padapter->securitypriv.ndisauthtype=Ndis802_11AuthModeOpen;
			}

			break;
		}

	case IW_AUTH_80211_AUTH_ALG:

		#if defined(CONFIG_ANDROID) || 1
		/*
		 *  It's the starting point of a link layer connection using wpa_supplicant
		*/
		if(check_fwstate(&padapter->mlmepriv, _FW_LINKED)) {
			LeaveAllPowerSaveMode(padapter);
			rtw_disassoc_cmd(padapter, 500, _FALSE);
			DBG_871X("%s...call rtw_indicate_disconnect\n ",__FUNCTION__);
			rtw_indicate_disconnect(padapter);
			rtw_free_assoc_resources(padapter, 1);
		}
		#endif


		ret = wpa_set_auth_algs(dev, (u32)param->value);

		break;

	case IW_AUTH_WPA_ENABLED:

		//if(param->value)
		//	padapter->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_8021X; //802.1x
		//else
		//	padapter->securitypriv.dot11AuthAlgrthm = dot11AuthAlgrthm_Open;//open system

		//_disassociate(priv);

		break;

	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
		//ieee->ieee802_1x = param->value;
		break;

	case IW_AUTH_PRIVACY_INVOKED:
		//ieee->privacy_invoked = param->value;
		break;

#ifdef CONFIG_WAPI_SUPPORT
#ifndef CONFIG_IOCTL_CFG80211
	case IW_AUTH_WAPI_ENABLED:
		break;
#endif
#endif

	default:
		return -EOPNOTSUPP;

	}

	return ret;

}

static int rtw_wx_set_enc_ext(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	char *alg_name;
	u32 param_len;
	struct ieee_param *param = NULL;
	struct iw_point *pencoding = &wrqu->encoding;
 	struct iw_encode_ext *pext = (struct iw_encode_ext *)extra;
	int ret=0;

	param_len = sizeof(struct ieee_param) + pext->key_len;
	param = (struct ieee_param *)rtw_malloc(param_len);
	if (param == NULL)
		return -1;

	_rtw_memset(param, 0, param_len);

	param->cmd = IEEE_CMD_SET_ENCRYPTION;
	_rtw_memset(param->sta_addr, 0xff, ETH_ALEN);


	switch (pext->alg) {
	case IW_ENCODE_ALG_NONE:
		//todo: remove key
		//remove = 1;
		alg_name = "none";
		break;
	case IW_ENCODE_ALG_WEP:
		alg_name = "WEP";
		break;
	case IW_ENCODE_ALG_TKIP:
		alg_name = "TKIP";
		break;
	case IW_ENCODE_ALG_CCMP:
		alg_name = "CCMP";
		break;
#ifdef CONFIG_IEEE80211W
	case IW_ENCODE_ALG_AES_CMAC:
		alg_name = "BIP";
		break;
#endif //CONFIG_IEEE80211W
#ifdef CONFIG_WAPI_SUPPORT
#ifndef CONFIG_IOCTL_CFG80211
	case IW_ENCODE_ALG_SM4:
		alg_name= "SMS4";
		_rtw_memcpy(param->sta_addr, pext->addr.sa_data, ETH_ALEN);
		DBG_871X("rtw_wx_set_enc_ext: SMS4 case \n");
		break;
#endif
#endif
	default:
		ret = -1;
		goto exit;
	}

	strncpy((char *)param->u.crypt.alg, alg_name, IEEE_CRYPT_ALG_NAME_LEN);

	if (pext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
	{
		param->u.crypt.set_tx = 1;
	}

	/* cliW: WEP does not have group key
	 * just not checking GROUP key setting
	 */
	if ((pext->alg != IW_ENCODE_ALG_WEP) &&
		((pext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
#ifdef CONFIG_IEEE80211W
		|| (pext->ext_flags & IW_ENCODE_ALG_AES_CMAC)
#endif //CONFIG_IEEE80211W
	))
	{
		param->u.crypt.set_tx = 0;
	}

	param->u.crypt.idx = (pencoding->flags&0x00FF) -1 ;

	if (pext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID)
	{
#ifdef CONFIG_WAPI_SUPPORT
#ifndef CONFIG_IOCTL_CFG80211
		if(pext->alg == IW_ENCODE_ALG_SM4)
			_rtw_memcpy(param->u.crypt.seq, pext->rx_seq, 16);
		else
#endif //CONFIG_IOCTL_CFG80211
#endif //CONFIG_WAPI_SUPPORT
		_rtw_memcpy(param->u.crypt.seq, pext->rx_seq, 8);
	}

	if(pext->key_len)
	{
		param->u.crypt.key_len = pext->key_len;
		//_rtw_memcpy(param + 1, pext + 1, pext->key_len);
		_rtw_memcpy(param->u.crypt.key, pext + 1, pext->key_len);
	}

	if (pencoding->flags & IW_ENCODE_DISABLED)
	{
		//todo: remove key
		//remove = 1;
	}

	ret =  wpa_set_encryption(dev, param, param_len);

exit:
	if(param)
	{
		rtw_mfree((u8*)param, param_len);
	}

	return ret;
}


static int rtw_wx_get_nick(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	//_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	 //struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	 //struct security_priv *psecuritypriv = &padapter->securitypriv;

	if(extra)
	{
		wrqu->data.length = 14;
		wrqu->data.flags = 1;
		_rtw_memcpy(extra, "<WIFI@REALTEK>", 14);
	}

	//dump debug info here
/*
	u32 dot11AuthAlgrthm;		// 802.11 auth, could be open, shared, and 8021x
	u32 dot11PrivacyAlgrthm;	// This specify the privacy for shared auth. algorithm.
	u32 dot118021XGrpPrivacy;	// This specify the privacy algthm. used for Grp key
	u32 ndisauthtype;
	u32 ndisencryptstatus;
*/

	//DBG_871X("auth_alg=0x%x, enc_alg=0x%x, auth_type=0x%x, enc_type=0x%x\n",
	//		psecuritypriv->dot11AuthAlgrthm, psecuritypriv->dot11PrivacyAlgrthm,
	//		psecuritypriv->ndisauthtype, psecuritypriv->ndisencryptstatus);

	//DBG_871X("enc_alg=0x%x\n", psecuritypriv->dot11PrivacyAlgrthm);
	//DBG_871X("auth_type=0x%x\n", psecuritypriv->ndisauthtype);
	//DBG_871X("enc_type=0x%x\n", psecuritypriv->ndisencryptstatus);

#if 0
	DBG_871X("dbg(0x210)=0x%x\n", rtw_read32(padapter, 0x210));
	DBG_871X("dbg(0x608)=0x%x\n", rtw_read32(padapter, 0x608));
	DBG_871X("dbg(0x280)=0x%x\n", rtw_read32(padapter, 0x280));
	DBG_871X("dbg(0x284)=0x%x\n", rtw_read32(padapter, 0x284));
	DBG_871X("dbg(0x288)=0x%x\n", rtw_read32(padapter, 0x288));

	DBG_871X("dbg(0x664)=0x%x\n", rtw_read32(padapter, 0x664));


	DBG_871X("\n");

	DBG_871X("dbg(0x430)=0x%x\n", rtw_read32(padapter, 0x430));
	DBG_871X("dbg(0x438)=0x%x\n", rtw_read32(padapter, 0x438));

	DBG_871X("dbg(0x440)=0x%x\n", rtw_read32(padapter, 0x440));

	DBG_871X("dbg(0x458)=0x%x\n", rtw_read32(padapter, 0x458));

	DBG_871X("dbg(0x484)=0x%x\n", rtw_read32(padapter, 0x484));
	DBG_871X("dbg(0x488)=0x%x\n", rtw_read32(padapter, 0x488));

	DBG_871X("dbg(0x444)=0x%x\n", rtw_read32(padapter, 0x444));
	DBG_871X("dbg(0x448)=0x%x\n", rtw_read32(padapter, 0x448));
	DBG_871X("dbg(0x44c)=0x%x\n", rtw_read32(padapter, 0x44c));
	DBG_871X("dbg(0x450)=0x%x\n", rtw_read32(padapter, 0x450));
#endif

	return 0;

}


static int dummy(struct net_device *dev, struct iw_request_info *a,
		 union iwreq_data *wrqu, char *b)
{
	//_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	//struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	//DBG_871X("cmd_code=%x, fwstate=0x%x\n", a->cmd, get_fwstate(pmlmepriv));

	return -1;

}

/*
typedef int (*iw_handler)(struct net_device *dev, struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra);
*/
/*
 *	For all data larger than 16 octets, we need to use a
 *	pointer to memory allocated in user space.
 */

extern int rtw_change_ifname(_adapter *padapter, const char *ifname);

#ifdef CONFIG_IOL
#include <rtw_iol.h>
#endif

#ifdef DBG_CMD_QUEUE
u8 dump_cmd_id=0;
#endif
static int rtw_dbg_port(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	_irqL irqL;
	int ret = 0;
	u8 major_cmd, minor_cmd;
	u16 arg;
	u32 extra_arg, *pdata, val32;
	struct sta_info *psta;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct security_priv *psecuritypriv = &padapter->securitypriv;
	struct wlan_network *cur_network = &(pmlmepriv->cur_network);
	struct sta_priv *pstapriv = &padapter->stapriv;


	pdata = (u32*)&wrqu->data;

	val32 = *pdata;
	arg = (u16)(val32&0x0000ffff);
	major_cmd = (u8)(val32>>24);
	minor_cmd = (u8)((val32>>16)&0x00ff);

	extra_arg = *(pdata+1);

	switch(major_cmd)
	{
		case 0x70://read_reg
			switch(minor_cmd)
			{
				case 1:
					DBG_871X("rtw_read8(0x%x)=0x%02x\n", arg, rtw_read8(padapter, arg));
					break;
				case 2:
					DBG_871X("rtw_read16(0x%x)=0x%04x\n", arg, rtw_read16(padapter, arg));
					break;
				case 4:
					DBG_871X("rtw_read32(0x%x)=0x%08x\n", arg, rtw_read32(padapter, arg));
					break;
			}
			break;
		case 0x71://write_reg
			switch(minor_cmd)
			{
				case 1:
					rtw_write8(padapter, arg, extra_arg);
					DBG_871X("rtw_write8(0x%x)=0x%02x\n", arg, rtw_read8(padapter, arg));
					break;
				case 2:
					rtw_write16(padapter, arg, extra_arg);
					DBG_871X("rtw_write16(0x%x)=0x%04x\n", arg, rtw_read16(padapter, arg));
					break;
				case 4:
					rtw_write32(padapter, arg, extra_arg);
					DBG_871X("rtw_write32(0x%x)=0x%08x\n", arg, rtw_read32(padapter, arg));
					break;
			}
			break;
		case 0x72://read_bb
			DBG_871X("read_bbreg(0x%x)=0x%x\n", arg, rtw_hal_read_bbreg(padapter, arg, 0xffffffff));
			break;
		case 0x73://write_bb
			rtw_hal_write_bbreg(padapter, arg, 0xffffffff, extra_arg);
			DBG_871X("write_bbreg(0x%x)=0x%x\n", arg, rtw_hal_read_bbreg(padapter, arg, 0xffffffff));
			break;
		case 0x74://read_rf
			DBG_871X("read RF_reg path(0x%02x),offset(0x%x),value(0x%08x)\n",minor_cmd,arg,rtw_hal_read_rfreg(padapter, minor_cmd, arg, 0xffffffff));
			break;
		case 0x75://write_rf
			rtw_hal_write_rfreg(padapter, minor_cmd, arg, 0xffffffff, extra_arg);
			DBG_871X("write RF_reg path(0x%02x),offset(0x%x),value(0x%08x)\n",minor_cmd,arg, rtw_hal_read_rfreg(padapter, minor_cmd, arg, 0xffffffff));
			break;

		case 0x76:
			switch(minor_cmd)
			{
				case 0x00: //normal mode,
					padapter->recvpriv.is_signal_dbg = 0;
					break;
				case 0x01: //dbg mode
					padapter->recvpriv.is_signal_dbg = 1;
					extra_arg = extra_arg>100?100:extra_arg;
					padapter->recvpriv.signal_strength_dbg=extra_arg;
					break;
			}
			break;
		case 0x78: //IOL test
			switch(minor_cmd)
			{
				#ifdef CONFIG_IOL
				case 0x04: //LLT table initialization test
				{
					u8 page_boundary = 0xf9;
					{
						struct xmit_frame	*xmit_frame;

						if((xmit_frame=rtw_IOL_accquire_xmit_frame(padapter)) == NULL) {
							ret = -ENOMEM;
							break;
						}

						rtw_IOL_append_LLT_cmd(xmit_frame, page_boundary);


						if(_SUCCESS != rtw_IOL_exec_cmds_sync(padapter, xmit_frame, 500,0) )
							ret = -EPERM;
					}
				}
					break;
				case 0x05: //blink LED test
				{
					u16 reg = 0x4c;
					u32 blink_num = 50;
					u32 blink_delay_ms = 200;
					int i;

					{
						struct xmit_frame	*xmit_frame;

						if((xmit_frame=rtw_IOL_accquire_xmit_frame(padapter)) == NULL) {
							ret = -ENOMEM;
							break;
						}

						for(i=0;i<blink_num;i++){
							#ifdef CONFIG_IOL_NEW_GENERATION
							rtw_IOL_append_WB_cmd(xmit_frame, reg, 0x00,0xff);
							rtw_IOL_append_DELAY_MS_cmd(xmit_frame, blink_delay_ms);
							rtw_IOL_append_WB_cmd(xmit_frame, reg, 0x08,0xff);
							rtw_IOL_append_DELAY_MS_cmd(xmit_frame, blink_delay_ms);
							#else
							rtw_IOL_append_WB_cmd(xmit_frame, reg, 0x00);
							rtw_IOL_append_DELAY_MS_cmd(xmit_frame, blink_delay_ms);
							rtw_IOL_append_WB_cmd(xmit_frame, reg, 0x08);
							rtw_IOL_append_DELAY_MS_cmd(xmit_frame, blink_delay_ms);
							#endif
						}
						if(_SUCCESS != rtw_IOL_exec_cmds_sync(padapter, xmit_frame, (blink_delay_ms*blink_num*2)+200,0) )
							ret = -EPERM;
					}
				}
					break;

				case 0x06: //continuous wirte byte test
				{
					u16 reg = arg;
					u16 start_value = 0;
					u32 write_num = extra_arg;
					int i;
					u8 final;

					{
						struct xmit_frame	*xmit_frame;

						if((xmit_frame=rtw_IOL_accquire_xmit_frame(padapter)) == NULL) {
							ret = -ENOMEM;
							break;
						}

						for(i=0;i<write_num;i++){
							#ifdef CONFIG_IOL_NEW_GENERATION
							rtw_IOL_append_WB_cmd(xmit_frame, reg, i+start_value,0xFF);
							#else
							rtw_IOL_append_WB_cmd(xmit_frame, reg, i+start_value);
							#endif
						}
						if(_SUCCESS != rtw_IOL_exec_cmds_sync(padapter, xmit_frame, 5000,0))
							ret = -EPERM;
					}

					if(start_value+write_num-1 == (final=rtw_read8(padapter, reg)) ) {
						DBG_871X("continuous IOL_CMD_WB_REG to 0x%x %u times Success, start:%u, final:%u\n", reg, write_num, start_value, final);
					} else {
						DBG_871X("continuous IOL_CMD_WB_REG to 0x%x %u times Fail, start:%u, final:%u\n", reg, write_num, start_value, final);
					}
				}
					break;

				case 0x07: //continuous wirte word test
				{
					u16 reg = arg;
					u16 start_value = 200;
					u32 write_num = extra_arg;

					int i;
					u16 final;

					{
						struct xmit_frame	*xmit_frame;

						if((xmit_frame=rtw_IOL_accquire_xmit_frame(padapter)) == NULL) {
							ret = -ENOMEM;
							break;
						}

						for(i=0;i<write_num;i++){
							#ifdef CONFIG_IOL_NEW_GENERATION
							rtw_IOL_append_WW_cmd(xmit_frame, reg, i+start_value,0xFFFF);
							#else
							rtw_IOL_append_WW_cmd(xmit_frame, reg, i+start_value);
							#endif
						}
						if(_SUCCESS !=rtw_IOL_exec_cmds_sync(padapter, xmit_frame, 5000,0))
							ret = -EPERM;
					}

					if(start_value+write_num-1 == (final=rtw_read16(padapter, reg)) ) {
						DBG_871X("continuous IOL_CMD_WW_REG to 0x%x %u times Success, start:%u, final:%u\n", reg, write_num, start_value, final);
					} else {
						DBG_871X("continuous IOL_CMD_WW_REG to 0x%x %u times Fail, start:%u, final:%u\n", reg, write_num, start_value, final);
					}
				}
					break;

				case 0x08: //continuous wirte dword test
				{
					u16 reg = arg;
					u32 start_value = 0x110000c7;
					u32 write_num = extra_arg;

					int i;
					u32 final;

					{
						struct xmit_frame	*xmit_frame;

						if((xmit_frame=rtw_IOL_accquire_xmit_frame(padapter)) == NULL) {
							ret = -ENOMEM;
							break;
						}

						for(i=0;i<write_num;i++){
							#ifdef CONFIG_IOL_NEW_GENERATION
							rtw_IOL_append_WD_cmd(xmit_frame, reg, i+start_value,0xFFFFFFFF);
							#else
							rtw_IOL_append_WD_cmd(xmit_frame, reg, i+start_value);
							#endif
						}
						if(_SUCCESS !=rtw_IOL_exec_cmds_sync(padapter, xmit_frame, 5000,0))
							ret = -EPERM;

					}

					if(start_value+write_num-1 == (final=rtw_read32(padapter, reg)) ) {
						DBG_871X("continuous IOL_CMD_WD_REG to 0x%x %u times Success, start:%u, final:%u\n", reg, write_num, start_value, final);
					} else {
						DBG_871X("continuous IOL_CMD_WD_REG to 0x%x %u times Fail, start:%u, final:%u\n", reg, write_num, start_value, final);
					}
				}
					break;
				#endif //CONFIG_IOL
			}
			break;
		case 0x79:
			{
				/*
				* dbg 0x79000000 [value], set RESP_TXAGC to + value, value:0~15
				* dbg 0x79010000 [value], set RESP_TXAGC to - value, value:0~15
				*/
				u8 value =  extra_arg & 0x0f;
				u8 sign = minor_cmd;
				u16 write_value = 0;

				DBG_871X("%s set RESP_TXAGC to %s %u\n", __func__, sign?"minus":"plus", value);

				if (sign)
					value = value | 0x10;

				write_value = value | (value << 5);
				rtw_write16(padapter, 0x6d9, write_value);
			}
			break;
		case 0x7a:
			receive_disconnect(padapter, pmlmeinfo->network.MacAddress
				, WLAN_REASON_EXPIRATION_CHK);
			break;
		case 0x7F:
			switch(minor_cmd)
			{
				case 0x0:
					DBG_871X("fwstate=0x%x\n", get_fwstate(pmlmepriv));
					break;
				case 0x01:
					DBG_871X("auth_alg=0x%x, enc_alg=0x%x, auth_type=0x%x, enc_type=0x%x\n",
						psecuritypriv->dot11AuthAlgrthm, psecuritypriv->dot11PrivacyAlgrthm,
						psecuritypriv->ndisauthtype, psecuritypriv->ndisencryptstatus);
					break;
				case 0x02:
					DBG_871X("pmlmeinfo->state=0x%x\n", pmlmeinfo->state);
					DBG_871X("DrvBcnEarly=%d\n", pmlmeext->DrvBcnEarly);
					DBG_871X("DrvBcnTimeOut=%d\n", pmlmeext->DrvBcnTimeOut);
					break;
				case 0x03:
					DBG_871X("qos_option=%d\n", pmlmepriv->qospriv.qos_option);
#ifdef CONFIG_80211N_HT
					DBG_871X("ht_option=%d\n", pmlmepriv->htpriv.ht_option);
#endif //CONFIG_80211N_HT
					break;
				case 0x04:
					DBG_871X("cur_ch=%d\n", pmlmeext->cur_channel);
					DBG_871X("cur_bw=%d\n", pmlmeext->cur_bwmode);
					DBG_871X("cur_ch_off=%d\n", pmlmeext->cur_ch_offset);

					DBG_871X("oper_ch=%d\n", rtw_get_oper_ch(padapter));
					DBG_871X("oper_bw=%d\n", rtw_get_oper_bw(padapter));
					DBG_871X("oper_ch_offet=%d\n", rtw_get_oper_choffset(padapter));

					break;
				case 0x05:
					psta = rtw_get_stainfo(pstapriv, cur_network->network.MacAddress);
					if(psta)
					{
						DBG_871X("SSID=%s\n", cur_network->network.Ssid.Ssid);
						DBG_871X("sta's macaddr:" MAC_FMT "\n", MAC_ARG(psta->hwaddr));
						DBG_871X("cur_channel=%d, cur_bwmode=%d, cur_ch_offset=%d\n", pmlmeext->cur_channel, pmlmeext->cur_bwmode, pmlmeext->cur_ch_offset);
						DBG_871X("rtsen=%d, cts2slef=%d\n", psta->rtsen, psta->cts2self);
						DBG_871X("state=0x%x, aid=%d, macid=%d, raid=%d\n", psta->state, psta->aid, psta->mac_id, psta->raid);
#ifdef CONFIG_80211N_HT
						DBG_871X("qos_en=%d, ht_en=%d, init_rate=%d\n", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
						DBG_871X("bwmode=%d, ch_offset=%d, sgi_20m=%d,sgi_40m=%d\n", psta->bw_mode, psta->htpriv.ch_offset, psta->htpriv.sgi_20m, psta->htpriv.sgi_40m);
						DBG_871X("ampdu_enable = %d\n", psta->htpriv.ampdu_enable);
						DBG_871X("agg_enable_bitmap=%x, candidate_tid_bitmap=%x\n", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);
#endif //CONFIG_80211N_HT

						sta_rx_reorder_ctl_dump(RTW_DBGDUMP, psta);
					}
					else
					{
						DBG_871X("can't get sta's macaddr, cur_network's macaddr:" MAC_FMT "\n", MAC_ARG(cur_network->network.MacAddress));
					}
					break;
				case 0x06:
					{
					}
					break;
				case 0x07:
					DBG_871X("bSurpriseRemoved=%s, bDriverStopped=%s\n"
						, rtw_is_surprise_removed(padapter)?"True":"False"
						, rtw_is_drv_stopped(padapter)?"True":"False");
					break;
				case 0x08:
					{
						struct xmit_priv *pxmitpriv = &padapter->xmitpriv;
						struct recv_priv  *precvpriv = &padapter->recvpriv;

						DBG_871X("free_xmitbuf_cnt=%d, free_xmitframe_cnt=%d"
							", free_xmit_extbuf_cnt=%d, free_xframe_ext_cnt=%d"
							", free_recvframe_cnt=%d\n",
							pxmitpriv->free_xmitbuf_cnt, pxmitpriv->free_xmitframe_cnt,
							pxmitpriv->free_xmit_extbuf_cnt, pxmitpriv->free_xframe_ext_cnt,
							precvpriv->free_recvframe_cnt);
						DBG_871X("rx_urb_pending_cn=%d\n", ATOMIC_READ(&(precvpriv->rx_pending_cnt)));
					}
					break;
				case 0x09:
					{
						int i;
						_list	*plist, *phead;

#ifdef CONFIG_AP_MODE
						DBG_871X("sta_dz_bitmap=0x%x, tim_bitmap=0x%x\n", pstapriv->sta_dz_bitmap, pstapriv->tim_bitmap);
#endif
						_enter_critical_bh(&pstapriv->sta_hash_lock, &irqL);

						for(i=0; i< NUM_STA; i++)
						{
							phead = &(pstapriv->sta_hash[i]);
							plist = get_next(phead);

							while ((rtw_end_of_queue_search(phead, plist)) == _FALSE)
							{
								psta = LIST_CONTAINOR(plist, struct sta_info, hash_list);

								plist = get_next(plist);

								if(extra_arg == psta->aid)
								{
									DBG_871X("sta's macaddr:" MAC_FMT "\n", MAC_ARG(psta->hwaddr));
									DBG_871X("rtsen=%d, cts2slef=%d\n", psta->rtsen, psta->cts2self);
									DBG_871X("state=0x%x, aid=%d, macid=%d, raid=%d\n", psta->state, psta->aid, psta->mac_id, psta->raid);
#ifdef CONFIG_80211N_HT
									DBG_871X("qos_en=%d, ht_en=%d, init_rate=%d\n", psta->qos_option, psta->htpriv.ht_option, psta->init_rate);
									DBG_871X("bwmode=%d, ch_offset=%d, sgi_20m=%d,sgi_40m=%d\n", psta->bw_mode, psta->htpriv.ch_offset, psta->htpriv.sgi_20m, psta->htpriv.sgi_40m);
									DBG_871X("ampdu_enable = %d\n", psta->htpriv.ampdu_enable);
									DBG_871X("agg_enable_bitmap=%x, candidate_tid_bitmap=%x\n", psta->htpriv.agg_enable_bitmap, psta->htpriv.candidate_tid_bitmap);
#endif //CONFIG_80211N_HT

#ifdef CONFIG_AP_MODE
									DBG_871X("capability=0x%x\n", psta->capability);
									DBG_871X("flags=0x%x\n", psta->flags);
									DBG_871X("wpa_psk=0x%x\n", psta->wpa_psk);
									DBG_871X("wpa2_group_cipher=0x%x\n", psta->wpa2_group_cipher);
									DBG_871X("wpa2_pairwise_cipher=0x%x\n", psta->wpa2_pairwise_cipher);
									DBG_871X("qos_info=0x%x\n", psta->qos_info);
#endif
									DBG_871X("dot118021XPrivacy=0x%x\n", psta->dot118021XPrivacy);

									sta_rx_reorder_ctl_dump(RTW_DBGDUMP, psta);
								}

							}
						}

						_exit_critical_bh(&pstapriv->sta_hash_lock, &irqL);

					}
					break;
				case 0x0a:
					{
						int max_mac_id = 0;
						max_mac_id = rtw_search_max_mac_id( padapter);
						printk("%s ==> max_mac_id = %d \n",__FUNCTION__,max_mac_id);
					}
					break;
				case 0x0b: //Enable=1, Disable=0 driver control vrtl_carrier_sense.
					{
						//u8 driver_vcs_en; //Enable=1, Disable=0 driver control vrtl_carrier_sense.
						//u8 driver_vcs_type;//force 0:disable VCS, 1:RTS-CTS, 2:CTS-to-self when vcs_en=1.

						if(arg == 0){
							DBG_871X("disable driver ctrl vcs\n");
							padapter->driver_vcs_en = 0;
						}
						else if(arg == 1){
							DBG_871X("enable driver ctrl vcs = %d\n", extra_arg);
							padapter->driver_vcs_en = 1;

							if(extra_arg>2)
								padapter->driver_vcs_type = 1;
							else
								padapter->driver_vcs_type = extra_arg;
						}
					}
					break;
				case 0x0c://dump rx/tx packet
					{
						if(arg == 0){
							DBG_871X("dump rx packet (%d)\n",extra_arg);
							//pHalData->bDumpRxPkt =extra_arg;
							rtw_hal_set_def_var(padapter, HAL_DEF_DBG_DUMP_RXPKT, &(extra_arg));
						}
						else if(arg==1){
							DBG_871X("dump tx packet (%d)\n",extra_arg);
							rtw_hal_set_def_var(padapter, HAL_DEF_DBG_DUMP_TXPKT, &(extra_arg));
						}
					}
					break;
				case 0x0e:
					{
						if(arg == 0){
							DBG_871X("disable driver ctrl rx_ampdu_factor\n");
							padapter->driver_rx_ampdu_factor = 0xFF;
						}
						else if(arg == 1){

							DBG_871X("enable driver ctrl rx_ampdu_factor = %d\n", extra_arg);

							if(extra_arg > 0x03)
								padapter->driver_rx_ampdu_factor = 0xFF;
							else
								padapter->driver_rx_ampdu_factor = extra_arg;
						}
					}
					break;
		#ifdef DBG_CONFIG_ERROR_DETECT
				case 0x0f:
						{
							if(extra_arg == 0){
								DBG_871X("###### silent reset test.......#####\n");
								rtw_hal_sreset_reset(padapter);
							} else {
								HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
								struct sreset_priv *psrtpriv = &pHalData->srestpriv;
								psrtpriv->dbg_trigger_point = extra_arg;
							}

						}
					break;
				case 0x15:
					{
						struct pwrctrl_priv *pwrpriv = adapter_to_pwrctl(padapter);
						DBG_871X("==>silent resete cnts:%d\n",pwrpriv->ips_enter_cnts);
					}
					break;

		#endif

				case 0x10:// driver version display
					dump_drv_version(RTW_DBGDUMP);
					break;
				case 0x11://dump linked status
					{
						int pre_mode;
						pre_mode=padapter->bLinkInfoDump;
						// linked_info_dump(padapter,extra_arg);
						 if(extra_arg==1 || (extra_arg==0 && pre_mode==1) ) //not consider pwr_saving 0:
						{
							padapter->bLinkInfoDump = extra_arg;

						}
						else if( (extra_arg==2 ) || (extra_arg==0 && pre_mode==2))//consider power_saving
						{
						//DBG_871X("linked_info_dump =%s \n", (padapter->bLinkInfoDump)?"enable":"disable")
							linked_info_dump(padapter,extra_arg);
						}



					}
					break;
#ifdef CONFIG_80211N_HT
				case 0x12: //set rx_stbc
				{
					struct registry_priv	*pregpriv = &padapter->registrypriv;
					// 0: disable, bit(0):enable 2.4g, bit(1):enable 5g, 0x3: enable both 2.4g and 5g
					//default is set to enable 2.4GHZ for IOT issue with bufflao's AP at 5GHZ
					if( pregpriv && (extra_arg == 0 || extra_arg == 1|| extra_arg == 2 || extra_arg == 3))
					{
						pregpriv->rx_stbc= extra_arg;
						DBG_871X("set rx_stbc=%d\n",pregpriv->rx_stbc);
					}
					else
						DBG_871X("get rx_stbc=%d\n",pregpriv->rx_stbc);

				}
				break;
				case 0x13: //set ampdu_enable
				{
					struct registry_priv	*pregpriv = &padapter->registrypriv;
					// 0: disable, 0x1:enable (but wifi_spec should be 0), 0x2: force enable (don't care wifi_spec)
					if( pregpriv && extra_arg < 3 )
					{
						pregpriv->ampdu_enable= extra_arg;
						DBG_871X("set ampdu_enable=%d\n",pregpriv->ampdu_enable);
					}
					else
						DBG_871X("get ampdu_enable=%d\n",pregpriv->ampdu_enable);

				}
				break;
#endif
				case 0x14: //get wifi_spec
				{
					struct registry_priv	*pregpriv = &padapter->registrypriv;
					DBG_871X("get wifi_spec=%d\n",pregpriv->wifi_spec);

				}
				break;
				case 0x16:
				{
					if(arg == 0xff){
						rtw_odm_dbg_comp_msg(RTW_DBGDUMP,padapter);
					}
					else{
						u64 dbg_comp = (u64)extra_arg;
						rtw_odm_dbg_comp_set(padapter, dbg_comp);
					}
				}
					break;
#ifdef DBG_FIXED_CHAN
				case 0x17:
					{
						struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
						printk("===>  Fixed channel to %d \n",extra_arg);
						pmlmeext->fixed_chan = extra_arg;

					}
					break;
#endif
#ifdef CONFIG_80211N_HT
				case 0x19:
					{
						struct registry_priv	*pregistrypriv = &padapter->registrypriv;
						// extra_arg :
						// BIT0: Enable VHT LDPC Rx, BIT1: Enable VHT LDPC Tx,
						// BIT4: Enable HT LDPC Rx, BIT5: Enable HT LDPC Tx
						if(arg == 0){
							DBG_871X("driver disable LDPC\n");
							pregistrypriv->ldpc_cap = 0x00;
						}
						else if(arg == 1){
							DBG_871X("driver set LDPC cap = 0x%x\n", extra_arg);
							pregistrypriv->ldpc_cap = (u8)(extra_arg&0x33);
						}
					}
                                        break;
				case 0x1a:
					{
						struct registry_priv	*pregistrypriv = &padapter->registrypriv;
						// extra_arg :
						// BIT0: Enable VHT STBC Rx, BIT1: Enable VHT STBC Tx,
						// BIT4: Enable HT STBC Rx, BIT5: Enable HT STBC Tx
						if(arg == 0){
							DBG_871X("driver disable STBC\n");
							pregistrypriv->stbc_cap = 0x00;
						}
						else if(arg == 1){
							DBG_871X("driver set STBC cap = 0x%x\n", extra_arg);
							pregistrypriv->stbc_cap = (u8)(extra_arg&0x33);
						}
					}
                                        break;
#endif //CONFIG_80211N_HT
				case 0x1b:
					{
						struct registry_priv	*pregistrypriv = &padapter->registrypriv;

						if(arg == 0){
							DBG_871X("disable driver ctrl max_rx_rate, reset to default_rate_set\n");
							init_mlme_default_rate_set(padapter);
#ifdef CONFIG_80211N_HT
							pregistrypriv->ht_enable = (u8)rtw_ht_enable;
#endif //CONFIG_80211N_HT
						}
						else if(arg == 1){

							int i;
							u8 max_rx_rate;

							DBG_871X("enable driver ctrl max_rx_rate = 0x%x\n", extra_arg);

							max_rx_rate = (u8)extra_arg;

							if(max_rx_rate < 0xc) // max_rx_rate < MSC0 -> B or G -> disable HT
							{
#ifdef CONFIG_80211N_HT
								pregistrypriv->ht_enable = 0;
#endif //CONFIG_80211N_HT
								for(i=0; i<NumRates; i++)
								{
									if(pmlmeext->datarate[i] > max_rx_rate)
										pmlmeext->datarate[i] = 0xff;
								}

							}
#ifdef CONFIG_80211N_HT
							else if(max_rx_rate < 0x1c) // mcs0~mcs15
							{
								u32 mcs_bitmap=0x0;

								for(i=0; i<((max_rx_rate+1)-0xc); i++)
									mcs_bitmap |= BIT(i);

								set_mcs_rate_by_mask(pmlmeext->default_supported_mcs_set, mcs_bitmap);
							}
#endif //CONFIG_80211N_HT
						}
					}
                                        break;
				case 0x1c: //enable/disable driver control AMPDU Density for peer sta's rx
					{
						if(arg == 0){
							DBG_871X("disable driver ctrl ampdu density\n");
							padapter->driver_ampdu_spacing = 0xFF;
						}
						else if(arg == 1){

							DBG_871X("enable driver ctrl ampdu density = %d\n", extra_arg);

							if(extra_arg > 0x07)
								padapter->driver_ampdu_spacing = 0xFF;
							else
								padapter->driver_ampdu_spacing = extra_arg;
						}
					}
					break;
#ifdef CONFIG_BACKGROUND_NOISE_MONITOR
				case 0x1e:
					{
						HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
						PDM_ODM_T pDM_Odm = &pHalData->odmpriv;
						u8 chan = rtw_get_oper_ch(padapter);
						DBG_871X("===========================================\n");
						ODM_InbandNoise_Monitor(pDM_Odm,_TRUE,0x1e,100);
						DBG_871X("channel(%d),noise_a = %d, noise_b = %d , noise_all:%d \n",
							chan,pDM_Odm->noise_level.noise[ODM_RF_PATH_A],
							pDM_Odm->noise_level.noise[ODM_RF_PATH_B],
							pDM_Odm->noise_level.noise_all);
						DBG_871X("===========================================\n");

					}
					break;
#endif
				case 0x23:
					{
						DBG_871X("turn %s the bNotifyChannelChange Variable\n",(extra_arg==1)?"on":"off");
						padapter->bNotifyChannelChange = extra_arg;
						break;
					}
				case 0x24:
					{
#ifdef CONFIG_P2P
						DBG_871X("turn %s the bShowGetP2PState Variable\n",(extra_arg==1)?"on":"off");
						padapter->bShowGetP2PState = extra_arg;
#endif // CONFIG_P2P
						break;
					}
#ifdef CONFIG_GPIO_API
		            case 0x25: //Get GPIO register
		                    {
			                    /*
			                    * dbg 0x7f250000 [gpio_num], Get gpio value, gpio_num:0~7
			                    */

			                    u8 value;
			                    DBG_871X("Read GPIO Value  extra_arg = %d\n",extra_arg);
			                    value = rtw_hal_get_gpio(padapter,extra_arg);
			                    DBG_871X("Read GPIO Value = %d\n",value);
			                    break;
		                    }
		            case 0x26: //Set GPIO direction
		                    {

			                    /* dbg 0x7f26000x [y], Set gpio direction,
			                    * x: gpio_num,4~7  y: indicate direction, 0~1
			                    */

			                    int value;
			                    DBG_871X("Set GPIO Direction! arg = %d ,extra_arg=%d\n",arg ,extra_arg);
			                    value = rtw_hal_config_gpio(padapter, arg, extra_arg);
			                    DBG_871X("Set GPIO Direction %s \n",(value==-1)?"Fail!!!":"Success");
			                    break;
					}
				case 0x27: //Set GPIO output direction value
					{
						/*
						* dbg 0x7f27000x [y], Set gpio output direction value,
 						* x: gpio_num,4~7  y: indicate direction, 0~1
						*/

						int value;
						DBG_871X("Set GPIO Value! arg = %d ,extra_arg=%d\n",arg ,extra_arg);
						value = rtw_hal_set_gpio_output_value(padapter,arg,extra_arg);
						DBG_871X("Set GPIO Value %s \n",(value==-1)?"Fail!!!":"Success");
						break;
					}
#endif
#ifdef DBG_CMD_QUEUE
				case 0x28:
					{
						dump_cmd_id = extra_arg;
						DBG_871X("dump_cmd_id:%d\n",dump_cmd_id);
					}
					break;
#endif //DBG_CMD_QUEUE
				case 0xaa:
					{
						if((extra_arg & 0x7F)> 0x3F) extra_arg = 0xFF;
						DBG_871X("chang data rate to :0x%02x\n",extra_arg);
						padapter->fix_rate = extra_arg;
					}
					break;
				case 0xdd://registers dump , 0 for mac reg,1 for bb reg, 2 for rf reg
					{
						if(extra_arg==0){
							mac_reg_dump(RTW_DBGDUMP, padapter);
						}
						else if(extra_arg==1){
							bb_reg_dump(RTW_DBGDUMP, padapter);
						}
						else if(extra_arg==2){
							rf_reg_dump(RTW_DBGDUMP, padapter);
						}
					}
					break;

				case 0xee:
					{
						DBG_871X(" === please control /proc  to trun on/off PHYDM func === \n");
					}
					break;

				case 0xfd:
					rtw_write8(padapter, 0xc50, arg);
					DBG_871X("wr(0xc50)=0x%x\n", rtw_read8(padapter, 0xc50));
					rtw_write8(padapter, 0xc58, arg);
					DBG_871X("wr(0xc58)=0x%x\n", rtw_read8(padapter, 0xc58));
					break;
				case 0xfe:
					DBG_871X("rd(0xc50)=0x%x\n", rtw_read8(padapter, 0xc50));
					DBG_871X("rd(0xc58)=0x%x\n", rtw_read8(padapter, 0xc58));
					break;
				case 0xff:
					{
						DBG_871X("dbg(0x210)=0x%x\n", rtw_read32(padapter, 0x210));
						DBG_871X("dbg(0x608)=0x%x\n", rtw_read32(padapter, 0x608));
						DBG_871X("dbg(0x280)=0x%x\n", rtw_read32(padapter, 0x280));
						DBG_871X("dbg(0x284)=0x%x\n", rtw_read32(padapter, 0x284));
						DBG_871X("dbg(0x288)=0x%x\n", rtw_read32(padapter, 0x288));

						DBG_871X("dbg(0x664)=0x%x\n", rtw_read32(padapter, 0x664));


						DBG_871X("\n");

						DBG_871X("dbg(0x430)=0x%x\n", rtw_read32(padapter, 0x430));
						DBG_871X("dbg(0x438)=0x%x\n", rtw_read32(padapter, 0x438));

						DBG_871X("dbg(0x440)=0x%x\n", rtw_read32(padapter, 0x440));

						DBG_871X("dbg(0x458)=0x%x\n", rtw_read32(padapter, 0x458));

						DBG_871X("dbg(0x484)=0x%x\n", rtw_read32(padapter, 0x484));
						DBG_871X("dbg(0x488)=0x%x\n", rtw_read32(padapter, 0x488));

						DBG_871X("dbg(0x444)=0x%x\n", rtw_read32(padapter, 0x444));
						DBG_871X("dbg(0x448)=0x%x\n", rtw_read32(padapter, 0x448));
						DBG_871X("dbg(0x44c)=0x%x\n", rtw_read32(padapter, 0x44c));
						DBG_871X("dbg(0x450)=0x%x\n", rtw_read32(padapter, 0x450));
					}
					break;
			}
			break;
		default:
			DBG_871X("error dbg cmd!\n");
			break;
	}


	return ret;

}

static int wpa_set_param(struct net_device *dev, u8 name, u32 value)
{
	uint ret=0;
	u32 flags;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	switch (name){
	case IEEE_PARAM_WPA_ENABLED:

		padapter->securitypriv.dot11AuthAlgrthm= dot11AuthAlgrthm_8021X; //802.1x

		//ret = ieee80211_wpa_enable(ieee, value);

		switch((value)&0xff)
		{
			case 1 : //WPA
			padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeWPAPSK; //WPA_PSK
			padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption2Enabled;
				break;
			case 2: //WPA2
			padapter->securitypriv.ndisauthtype = Ndis802_11AuthModeWPA2PSK; //WPA2_PSK
			padapter->securitypriv.ndisencryptstatus = Ndis802_11Encryption3Enabled;
				break;
		}

		RT_TRACE(_module_rtl871x_ioctl_os_c,_drv_info_,("wpa_set_param:padapter->securitypriv.ndisauthtype=%d\n", padapter->securitypriv.ndisauthtype));

		break;

	case IEEE_PARAM_TKIP_COUNTERMEASURES:
		//ieee->tkip_countermeasures=value;
		break;

	case IEEE_PARAM_DROP_UNENCRYPTED:
	{
		/* HACK:
		 *
		 * wpa_supplicant calls set_wpa_enabled when the driver
		 * is loaded and unloaded, regardless of if WPA is being
		 * used.  No other calls are made which can be used to
		 * determine if encryption will be used or not prior to
		 * association being expected.  If encryption is not being
		 * used, drop_unencrypted is set to false, else true -- we
		 * can use this to determine if the CAP_PRIVACY_ON bit should
		 * be set.
		 */

#if 0
		struct ieee80211_security sec = {
			.flags = SEC_ENABLED,
			.enabled = value,
		};
		ieee->drop_unencrypted = value;
		/* We only change SEC_LEVEL for open mode. Others
		 * are set by ipw_wpa_set_encryption.
		 */
		if (!value) {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_0;
		}
		else {
			sec.flags |= SEC_LEVEL;
			sec.level = SEC_LEVEL_1;
		}
		if (ieee->set_security)
			ieee->set_security(ieee->dev, &sec);
#endif
		break;

	}
	case IEEE_PARAM_PRIVACY_INVOKED:

		//ieee->privacy_invoked=value;

		break;

	case IEEE_PARAM_AUTH_ALGS:

		ret = wpa_set_auth_algs(dev, value);

		break;

	case IEEE_PARAM_IEEE_802_1X:

		//ieee->ieee802_1x=value;

		break;

	case IEEE_PARAM_WPAX_SELECT:

		// added for WPA2 mixed mode
		//DBG_871X(KERN_WARNING "------------------------>wpax value = %x\n", value);
		/*
		spin_lock_irqsave(&ieee->wpax_suitlist_lock,flags);
		ieee->wpax_type_set = 1;
		ieee->wpax_type_notify = value;
		spin_unlock_irqrestore(&ieee->wpax_suitlist_lock,flags);
		*/

		break;

	default:



		ret = -EOPNOTSUPP;


		break;

	}

	return ret;

}

static int wpa_mlme(struct net_device *dev, u32 command, u32 reason)
{
	int ret = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	switch (command)
	{
		case IEEE_MLME_STA_DEAUTH:

			if(!rtw_set_802_11_disassociate(padapter))
				ret = -1;

			break;

		case IEEE_MLME_STA_DISASSOC:

			if(!rtw_set_802_11_disassociate(padapter))
				ret = -1;

			break;

		default:
			ret = -EOPNOTSUPP;
			break;
	}

	return ret;

}

static int wpa_supplicant_ioctl(struct net_device *dev, struct iw_point *p)
{
	struct ieee_param *param;
	uint ret=0;

	//down(&ieee->wx_sem);

	if (p->length < sizeof(struct ieee_param) || !p->pointer){
		ret = -EINVAL;
		goto out;
	}

	param = (struct ieee_param *)rtw_malloc(p->length);
	if (param == NULL)
	{
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, p->pointer, p->length))
	{
		rtw_mfree((u8*)param, p->length);
		ret = -EFAULT;
		goto out;
	}

	switch (param->cmd) {

	case IEEE_CMD_SET_WPA_PARAM:
		ret = wpa_set_param(dev, param->u.wpa_param.name, param->u.wpa_param.value);
		break;

	case IEEE_CMD_SET_WPA_IE:
		//ret = wpa_set_wpa_ie(dev, param, p->length);
		ret =  rtw_set_wpa_ie((_adapter *)rtw_netdev_priv(dev), (char*)param->u.wpa_ie.data, (u16)param->u.wpa_ie.len);
		break;

	case IEEE_CMD_SET_ENCRYPTION:
		ret = wpa_set_encryption(dev, param, p->length);
		break;

	case IEEE_CMD_MLME:
		ret = wpa_mlme(dev, param->u.mlme.command, param->u.mlme.reason_code);
		break;

	default:
		DBG_871X("Unknown WPA supplicant request: %d\n", param->cmd);
		ret = -EOPNOTSUPP;
		break;

	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;

	rtw_mfree((u8 *)param, p->length);

out:

	//up(&ieee->wx_sem);

	return ret;

}

#ifdef CONFIG_AP_MODE
static int rtw_set_encryption(struct net_device *dev, struct ieee_param *param, u32 param_len)
{
	int ret = 0;
	u32 wep_key_idx, wep_key_len,wep_total_len;
	NDIS_802_11_WEP	 *pwep = NULL;
	struct sta_info *psta = NULL, *pbcmc_sta = NULL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv 	*pmlmepriv = &padapter->mlmepriv;
	struct security_priv* psecuritypriv=&(padapter->securitypriv);
	struct sta_priv *pstapriv = &padapter->stapriv;

	DBG_871X("%s\n", __FUNCTION__);

	param->u.crypt.err = 0;
	param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

	//sizeof(struct ieee_param) = 64 bytes;
	//if (param_len !=  (u32) ((u8 *) param->u.crypt.key - (u8 *) param) + param->u.crypt.key_len)
	if (param_len !=  sizeof(struct ieee_param) + param->u.crypt.key_len)
	{
		ret =  -EINVAL;
		goto exit;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		if (param->u.crypt.idx >= WEP_KEYS
#ifdef CONFIG_IEEE80211W
			&& param->u.crypt.idx > BIP_MAX_KEYID
#endif /* CONFIG_IEEE80211W */
			)
		{
			ret = -EINVAL;
			goto exit;
		}
	}
	else
	{
		psta = rtw_get_stainfo(pstapriv, param->sta_addr);
		if(!psta)
		{
			//ret = -EINVAL;
			DBG_871X("rtw_set_encryption(), sta has already been removed or never been added\n");
			goto exit;
		}
	}

	if (strcmp(param->u.crypt.alg, "none") == 0 && (psta==NULL))
	{
		//todo:clear default encryption keys

		psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_Open;
		psecuritypriv->ndisencryptstatus = Ndis802_11EncryptionDisabled;
		psecuritypriv->dot11PrivacyAlgrthm = _NO_PRIVACY_;
		psecuritypriv->dot118021XGrpPrivacy = _NO_PRIVACY_;

		DBG_871X("clear default encryption keys, keyid=%d\n", param->u.crypt.idx);

		goto exit;
	}


	if (strcmp(param->u.crypt.alg, "WEP") == 0 && (psta==NULL))
	{
		DBG_871X("r871x_set_encryption, crypt.alg = WEP\n");

		wep_key_idx = param->u.crypt.idx;
		wep_key_len = param->u.crypt.key_len;

		DBG_871X("r871x_set_encryption, wep_key_idx=%d, len=%d\n", wep_key_idx, wep_key_len);

		if((wep_key_idx >= WEP_KEYS) || (wep_key_len<=0))
		{
			ret = -EINVAL;
			goto exit;
		}


		if (wep_key_len > 0)
		{
			wep_key_len = wep_key_len <= 5 ? 5 : 13;
			wep_total_len = wep_key_len + FIELD_OFFSET(NDIS_802_11_WEP, KeyMaterial);
			pwep =(NDIS_802_11_WEP *)rtw_malloc(wep_total_len);
			if(pwep == NULL){
				DBG_871X(" r871x_set_encryption: pwep allocate fail !!!\n");
				goto exit;
			}

			_rtw_memset(pwep, 0, wep_total_len);

		 	pwep->KeyLength = wep_key_len;
			pwep->Length = wep_total_len;

		}

		pwep->KeyIndex = wep_key_idx;

		_rtw_memcpy(pwep->KeyMaterial,  param->u.crypt.key, pwep->KeyLength);

		if(param->u.crypt.set_tx)
		{
			DBG_871X("wep, set_tx=1\n");

			psecuritypriv->dot11AuthAlgrthm = dot11AuthAlgrthm_Auto;
			psecuritypriv->ndisencryptstatus = Ndis802_11Encryption1Enabled;
			psecuritypriv->dot11PrivacyAlgrthm=_WEP40_;
			psecuritypriv->dot118021XGrpPrivacy=_WEP40_;

			if(pwep->KeyLength==13)
			{
				psecuritypriv->dot11PrivacyAlgrthm=_WEP104_;
				psecuritypriv->dot118021XGrpPrivacy=_WEP104_;
			}


			psecuritypriv->dot11PrivacyKeyIndex = wep_key_idx;

			_rtw_memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), pwep->KeyMaterial, pwep->KeyLength);

			psecuritypriv->dot11DefKeylen[wep_key_idx]=pwep->KeyLength;

			rtw_ap_set_wep_key(padapter, pwep->KeyMaterial, pwep->KeyLength, wep_key_idx, 1);
		}
		else
		{
			DBG_871X("wep, set_tx=0\n");

			//don't update "psecuritypriv->dot11PrivacyAlgrthm" and
			//"psecuritypriv->dot11PrivacyKeyIndex=keyid", but can rtw_set_key to cam

		      _rtw_memcpy(&(psecuritypriv->dot11DefKey[wep_key_idx].skey[0]), pwep->KeyMaterial, pwep->KeyLength);

			psecuritypriv->dot11DefKeylen[wep_key_idx] = pwep->KeyLength;

			rtw_ap_set_wep_key(padapter, pwep->KeyMaterial, pwep->KeyLength, wep_key_idx, 0);
		}

		goto exit;

	}


	if(!psta && check_fwstate(pmlmepriv, WIFI_AP_STATE)) // //group key
	{
		if(param->u.crypt.set_tx ==1)
		{
			if(strcmp(param->u.crypt.alg, "WEP") == 0)
			{
				DBG_871X("%s, set group_key, WEP\n", __FUNCTION__);

				_rtw_memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

				psecuritypriv->dot118021XGrpPrivacy = _WEP40_;
				if(param->u.crypt.key_len==13)
				{
						psecuritypriv->dot118021XGrpPrivacy = _WEP104_;
				}

			}
			else if(strcmp(param->u.crypt.alg, "TKIP") == 0)
			{
				DBG_871X("%s, set group_key, TKIP\n", __FUNCTION__);

				psecuritypriv->dot118021XGrpPrivacy = _TKIP_;

				_rtw_memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

				//DEBUG_ERR("set key length :param->u.crypt.key_len=%d\n", param->u.crypt.key_len);
				//set mic key
				_rtw_memcpy(psecuritypriv->dot118021XGrptxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[16]), 8);
				_rtw_memcpy(psecuritypriv->dot118021XGrprxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[24]), 8);

				psecuritypriv->busetkipkey = _TRUE;

			}
			else if(strcmp(param->u.crypt.alg, "CCMP") == 0)
			{
				DBG_871X("%s, set group_key, CCMP\n", __FUNCTION__);

				psecuritypriv->dot118021XGrpPrivacy = _AES_;

				_rtw_memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));
			}
#ifdef CONFIG_IEEE80211W
			else if (strcmp(param->u.crypt.alg, "BIP") == 0) {
				int no;

				DBG_871X("BIP key_len=%d , index=%d\n", param->u.crypt.key_len, param->u.crypt.idx);
				/* save the IGTK key, length 16 bytes */
				_rtw_memcpy(padapter->securitypriv.dot11wBIPKey[param->u.crypt.idx].skey, param->u.crypt.key, (param->u.crypt.key_len > 16 ? 16:param->u.crypt.key_len));
				/* DBG_871X("IGTK key below:\n");
				for(no=0;no<16;no++)
					printk(" %02x ", padapter->securitypriv.dot11wBIPKey[param->u.crypt.idx].skey[no]);
				DBG_871X("\n"); */
				padapter->securitypriv.dot11wBIPKeyid = param->u.crypt.idx;
				padapter->securitypriv.binstallBIPkey = _TRUE;
				DBG_871X(" ~~~~set sta key:IGKT\n");
				goto exit;
			}
#endif /* CONFIG_IEEE80211W */
			else
			{
				DBG_871X("%s, set group_key, none\n", __FUNCTION__);

				psecuritypriv->dot118021XGrpPrivacy = _NO_PRIVACY_;
			}

			psecuritypriv->dot118021XGrpKeyid = param->u.crypt.idx;

			psecuritypriv->binstallGrpkey = _TRUE;

			psecuritypriv->dot11PrivacyAlgrthm = psecuritypriv->dot118021XGrpPrivacy;//!!!

			rtw_ap_set_group_key(padapter, param->u.crypt.key, psecuritypriv->dot118021XGrpPrivacy, param->u.crypt.idx);

			pbcmc_sta=rtw_get_bcmc_stainfo(padapter);
			if(pbcmc_sta)
			{
				pbcmc_sta->ieee8021x_blocked = _FALSE;
				pbcmc_sta->dot118021XPrivacy= psecuritypriv->dot118021XGrpPrivacy;//rx will use bmc_sta's dot118021XPrivacy
			}

		}

		goto exit;

	}

	if(psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X && psta) // psk/802_1x
	{
		if(check_fwstate(pmlmepriv, WIFI_AP_STATE))
		{
			if(param->u.crypt.set_tx ==1)
			{
				_rtw_memcpy(psta->dot118021x_UncstKey.skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

				if(strcmp(param->u.crypt.alg, "WEP") == 0)
				{
					DBG_871X("%s, set pairwise key, WEP\n", __FUNCTION__);

					psta->dot118021XPrivacy = _WEP40_;
					if(param->u.crypt.key_len==13)
					{
						psta->dot118021XPrivacy = _WEP104_;
					}
				}
				else if(strcmp(param->u.crypt.alg, "TKIP") == 0)
				{
					DBG_871X("%s, set pairwise key, TKIP\n", __FUNCTION__);

					psta->dot118021XPrivacy = _TKIP_;

					//DEBUG_ERR("set key length :param->u.crypt.key_len=%d\n", param->u.crypt.key_len);
					//set mic key
					_rtw_memcpy(psta->dot11tkiptxmickey.skey, &(param->u.crypt.key[16]), 8);
					_rtw_memcpy(psta->dot11tkiprxmickey.skey, &(param->u.crypt.key[24]), 8);

					psecuritypriv->busetkipkey = _TRUE;

				}
				else if(strcmp(param->u.crypt.alg, "CCMP") == 0)
				{

					DBG_871X("%s, set pairwise key, CCMP\n", __FUNCTION__);

					psta->dot118021XPrivacy = _AES_;
				}
				else
				{
					DBG_871X("%s, set pairwise key, none\n", __FUNCTION__);

					psta->dot118021XPrivacy = _NO_PRIVACY_;
				}

				rtw_ap_set_pairwise_key(padapter, psta);

				psta->ieee8021x_blocked = _FALSE;

				psta->bpairwise_key_installed = _TRUE;

			}
			else//group key???
			{
				if(strcmp(param->u.crypt.alg, "WEP") == 0)
				{
					_rtw_memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

					psecuritypriv->dot118021XGrpPrivacy = _WEP40_;
					if(param->u.crypt.key_len==13)
					{
						psecuritypriv->dot118021XGrpPrivacy = _WEP104_;
					}
				}
				else if(strcmp(param->u.crypt.alg, "TKIP") == 0)
				{
					psecuritypriv->dot118021XGrpPrivacy = _TKIP_;

					_rtw_memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));

					//DEBUG_ERR("set key length :param->u.crypt.key_len=%d\n", param->u.crypt.key_len);
					//set mic key
					_rtw_memcpy(psecuritypriv->dot118021XGrptxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[16]), 8);
					_rtw_memcpy(psecuritypriv->dot118021XGrprxmickey[param->u.crypt.idx].skey, &(param->u.crypt.key[24]), 8);

					psecuritypriv->busetkipkey = _TRUE;

				}
				else if(strcmp(param->u.crypt.alg, "CCMP") == 0)
				{
					psecuritypriv->dot118021XGrpPrivacy = _AES_;

					_rtw_memcpy(psecuritypriv->dot118021XGrpKey[param->u.crypt.idx].skey,  param->u.crypt.key, (param->u.crypt.key_len>16 ?16:param->u.crypt.key_len));
				}
				else
				{
					psecuritypriv->dot118021XGrpPrivacy = _NO_PRIVACY_;
				}

				psecuritypriv->dot118021XGrpKeyid = param->u.crypt.idx;

				psecuritypriv->binstallGrpkey = _TRUE;

				psecuritypriv->dot11PrivacyAlgrthm = psecuritypriv->dot118021XGrpPrivacy;//!!!

				rtw_ap_set_group_key(padapter, param->u.crypt.key, psecuritypriv->dot118021XGrpPrivacy, param->u.crypt.idx);

				pbcmc_sta=rtw_get_bcmc_stainfo(padapter);
				if(pbcmc_sta)
				{
					pbcmc_sta->ieee8021x_blocked = _FALSE;
					pbcmc_sta->dot118021XPrivacy= psecuritypriv->dot118021XGrpPrivacy;//rx will use bmc_sta's dot118021XPrivacy
				}

			}

		}

	}

exit:

	if(pwep)
	{
		rtw_mfree((u8 *)pwep, wep_total_len);
	}

	return ret;

}

static int rtw_set_beacon(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct sta_priv *pstapriv = &padapter->stapriv;
	unsigned char *pbuf = param->u.bcn_ie.buf;


	DBG_871X("%s, len=%d\n", __FUNCTION__, len);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	_rtw_memcpy(&pstapriv->max_num_sta, param->u.bcn_ie.reserved, 2);

	if((pstapriv->max_num_sta>NUM_STA) || (pstapriv->max_num_sta<=0))
		pstapriv->max_num_sta = NUM_STA;


	if(rtw_check_beacon_data(padapter, pbuf,  (len-12-2)) == _SUCCESS)// 12 = param header, 2:no packed
		ret = 0;
	else
		ret = -EINVAL;


	return ret;

}

static int rtw_hostapd_sta_flush(struct net_device *dev)
{
	//_irqL irqL;
	//_list	*phead, *plist;
	int ret=0;
	//struct sta_info *psta = NULL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	//struct sta_priv *pstapriv = &padapter->stapriv;

	DBG_871X("%s\n", __FUNCTION__);

	flush_all_cam_entry(padapter);	//clear CAM

	ret = rtw_sta_flush(padapter, _TRUE);

	return ret;

}

static int rtw_add_sta(struct net_device *dev, struct ieee_param *param)
{
	_irqL irqL;
	int ret=0;
	struct sta_info *psta = NULL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct sta_priv *pstapriv = &padapter->stapriv;

	DBG_871X("rtw_add_sta(aid=%d)=" MAC_FMT "\n", param->u.add_sta.aid, MAC_ARG(param->sta_addr));

	if(check_fwstate(pmlmepriv, (_FW_LINKED|WIFI_AP_STATE)) != _TRUE)
	{
		return -EINVAL;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		return -EINVAL;
	}

/*
	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if(psta)
	{
		DBG_871X("rtw_add_sta(), free has been added psta=%p\n", psta);
		//_enter_critical_bh(&(pstapriv->sta_hash_lock), &irqL);
		rtw_free_stainfo(padapter,  psta);
		//_exit_critical_bh(&(pstapriv->sta_hash_lock), &irqL);

		psta = NULL;
	}
*/
	//psta = rtw_alloc_stainfo(pstapriv, param->sta_addr);
	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if(psta)
	{
		int flags = param->u.add_sta.flags;

		//DBG_871X("rtw_add_sta(), init sta's variables, psta=%p\n", psta);

		psta->aid = param->u.add_sta.aid;//aid=1~2007

		_rtw_memcpy(psta->bssrateset, param->u.add_sta.tx_supp_rates, 16);


		//check wmm cap.
		if(WLAN_STA_WME&flags)
			psta->qos_option = 1;
		else
			psta->qos_option = 0;

		if(pmlmepriv->qospriv.qos_option == 0)
			psta->qos_option = 0;


#ifdef CONFIG_80211N_HT
		//chec 802.11n ht cap.
		if(WLAN_STA_HT&flags)
		{
			psta->htpriv.ht_option = _TRUE;
			psta->qos_option = 1;
			_rtw_memcpy((void*)&psta->htpriv.ht_cap, (void*)&param->u.add_sta.ht_cap, sizeof(struct rtw_ieee80211_ht_cap));
		}
		else
		{
			psta->htpriv.ht_option = _FALSE;
		}

		if(pmlmepriv->htpriv.ht_option == _FALSE)
			psta->htpriv.ht_option = _FALSE;
#endif


		update_sta_info_apmode(padapter, psta);


	}
	else
	{
		ret = -ENOMEM;
	}

	return ret;

}

static int rtw_del_sta(struct net_device *dev, struct ieee_param *param)
{
	_irqL irqL;
	int ret=0;
	struct sta_info *psta = NULL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct sta_priv *pstapriv = &padapter->stapriv;

	DBG_871X("rtw_del_sta=" MAC_FMT "\n", MAC_ARG(param->sta_addr));

	if(check_fwstate(pmlmepriv, (_FW_LINKED|WIFI_AP_STATE)) != _TRUE)
	{
		return -EINVAL;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		return -EINVAL;
	}

	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if(psta)
	{
		u8 updated=_FALSE;

		//DBG_871X("free psta=%p, aid=%d\n", psta, psta->aid);

		_enter_critical_bh(&pstapriv->asoc_list_lock, &irqL);
		if(rtw_is_list_empty(&psta->asoc_list)==_FALSE)
		{
			rtw_list_delete(&psta->asoc_list);
			pstapriv->asoc_list_cnt--;
			updated = ap_free_sta(padapter, psta, _TRUE, WLAN_REASON_DEAUTH_LEAVING, _TRUE);

		}
		_exit_critical_bh(&pstapriv->asoc_list_lock, &irqL);

		associated_clients_update(padapter, updated, STA_INFO_UPDATE_ALL);

		psta = NULL;

	}
	else
	{
		DBG_871X("rtw_del_sta(), sta has already been removed or never been added\n");

		//ret = -1;
	}


	return ret;

}

static int rtw_ioctl_get_sta_data(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	struct sta_info *psta = NULL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct sta_priv *pstapriv = &padapter->stapriv;
	struct ieee_param_ex *param_ex = (struct ieee_param_ex *)param;
	struct sta_data *psta_data = (struct sta_data *)param_ex->data;

	DBG_871X("rtw_ioctl_get_sta_info, sta_addr: " MAC_FMT "\n", MAC_ARG(param_ex->sta_addr));

	if(check_fwstate(pmlmepriv, (_FW_LINKED|WIFI_AP_STATE)) != _TRUE)
	{
		return -EINVAL;
	}

	if (param_ex->sta_addr[0] == 0xff && param_ex->sta_addr[1] == 0xff &&
	    param_ex->sta_addr[2] == 0xff && param_ex->sta_addr[3] == 0xff &&
	    param_ex->sta_addr[4] == 0xff && param_ex->sta_addr[5] == 0xff)
	{
		return -EINVAL;
	}

	psta = rtw_get_stainfo(pstapriv, param_ex->sta_addr);
	if(psta)
	{
#if 0
		struct {
			u16 aid;
			u16 capability;
			int flags;
			u32 sta_set;
			u8 tx_supp_rates[16];
			u32 tx_supp_rates_len;
			struct rtw_ieee80211_ht_cap ht_cap;
			u64	rx_pkts;
			u64	rx_bytes;
			u64	rx_drops;
			u64	tx_pkts;
			u64	tx_bytes;
			u64	tx_drops;
		} get_sta;
#endif
		psta_data->aid = (u16)psta->aid;
		psta_data->capability = psta->capability;
		psta_data->flags = psta->flags;

/*
		nonerp_set : BIT(0)
		no_short_slot_time_set : BIT(1)
		no_short_preamble_set : BIT(2)
		no_ht_gf_set : BIT(3)
		no_ht_set : BIT(4)
		ht_20mhz_set : BIT(5)
*/

		psta_data->sta_set =((psta->nonerp_set) |
							(psta->no_short_slot_time_set <<1) |
							(psta->no_short_preamble_set <<2) |
							(psta->no_ht_gf_set <<3) |
							(psta->no_ht_set <<4) |
							(psta->ht_20mhz_set <<5));

		psta_data->tx_supp_rates_len =  psta->bssratelen;
		_rtw_memcpy(psta_data->tx_supp_rates, psta->bssrateset, psta->bssratelen);
#ifdef CONFIG_80211N_HT
		_rtw_memcpy(&psta_data->ht_cap, &psta->htpriv.ht_cap, sizeof(struct rtw_ieee80211_ht_cap));
#endif //CONFIG_80211N_HT
		psta_data->rx_pkts = psta->sta_stats.rx_data_pkts;
		psta_data->rx_bytes = psta->sta_stats.rx_bytes;
		psta_data->rx_drops = psta->sta_stats.rx_drops;

		psta_data->tx_pkts = psta->sta_stats.tx_pkts;
		psta_data->tx_bytes = psta->sta_stats.tx_bytes;
		psta_data->tx_drops = psta->sta_stats.tx_drops;


	}
	else
	{
		ret = -1;
	}

	return ret;

}

static int rtw_get_sta_wpaie(struct net_device *dev, struct ieee_param *param)
{
	int ret=0;
	struct sta_info *psta = NULL;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct sta_priv *pstapriv = &padapter->stapriv;

	DBG_871X("rtw_get_sta_wpaie, sta_addr: " MAC_FMT "\n", MAC_ARG(param->sta_addr));

	if(check_fwstate(pmlmepriv, (_FW_LINKED|WIFI_AP_STATE)) != _TRUE)
	{
		return -EINVAL;
	}

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		return -EINVAL;
	}

	psta = rtw_get_stainfo(pstapriv, param->sta_addr);
	if(psta)
	{
		if((psta->wpa_ie[0] == WLAN_EID_RSN) || (psta->wpa_ie[0] == WLAN_EID_GENERIC))
		{
			int wpa_ie_len;
			int copy_len;

			wpa_ie_len = psta->wpa_ie[1];

			copy_len = ((wpa_ie_len+2) > sizeof(psta->wpa_ie)) ? (sizeof(psta->wpa_ie)):(wpa_ie_len+2);

			param->u.wpa_ie.len = copy_len;

			_rtw_memcpy(param->u.wpa_ie.reserved, psta->wpa_ie, copy_len);
		}
		else
		{
			//ret = -1;
			DBG_871X("sta's wpa_ie is NONE\n");
		}
	}
	else
	{
		ret = -1;
	}

	return ret;

}

static int rtw_set_wps_beacon(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	unsigned char wps_oui[4]={0x0,0x50,0xf2,0x04};
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct mlme_ext_priv	*pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	int ie_len;

	DBG_871X("%s, len=%d\n", __FUNCTION__, len);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	ie_len = len-12-2;// 12 = param header, 2:no packed


	if(pmlmepriv->wps_beacon_ie)
	{
		rtw_mfree(pmlmepriv->wps_beacon_ie, pmlmepriv->wps_beacon_ie_len);
		pmlmepriv->wps_beacon_ie = NULL;
	}

	if(ie_len>0)
	{
		pmlmepriv->wps_beacon_ie = rtw_malloc(ie_len);
		pmlmepriv->wps_beacon_ie_len = ie_len;
		if ( pmlmepriv->wps_beacon_ie == NULL) {
			DBG_871X("%s()-%d: rtw_malloc() ERROR!\n", __FUNCTION__, __LINE__);
			return -EINVAL;
		}

		_rtw_memcpy(pmlmepriv->wps_beacon_ie, param->u.bcn_ie.buf, ie_len);

		update_beacon(padapter, _VENDOR_SPECIFIC_IE_, wps_oui, _TRUE);

		pmlmeext->bstart_bss = _TRUE;

	}


	return ret;

}

static int rtw_set_wps_probe_resp(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	int ie_len;

	DBG_871X("%s, len=%d\n", __FUNCTION__, len);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	ie_len = len-12-2;// 12 = param header, 2:no packed


	if(pmlmepriv->wps_probe_resp_ie)
	{
		rtw_mfree(pmlmepriv->wps_probe_resp_ie, pmlmepriv->wps_probe_resp_ie_len);
		pmlmepriv->wps_probe_resp_ie = NULL;
	}

	if(ie_len>0)
	{
		pmlmepriv->wps_probe_resp_ie = rtw_malloc(ie_len);
		pmlmepriv->wps_probe_resp_ie_len = ie_len;
		if ( pmlmepriv->wps_probe_resp_ie == NULL) {
			DBG_871X("%s()-%d: rtw_malloc() ERROR!\n", __FUNCTION__, __LINE__);
			return -EINVAL;
		}
		_rtw_memcpy(pmlmepriv->wps_probe_resp_ie, param->u.bcn_ie.buf, ie_len);
	}


	return ret;

}

static int rtw_set_wps_assoc_resp(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	int ie_len;

	DBG_871X("%s, len=%d\n", __FUNCTION__, len);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	ie_len = len-12-2;// 12 = param header, 2:no packed


	if(pmlmepriv->wps_assoc_resp_ie)
	{
		rtw_mfree(pmlmepriv->wps_assoc_resp_ie, pmlmepriv->wps_assoc_resp_ie_len);
		pmlmepriv->wps_assoc_resp_ie = NULL;
	}

	if(ie_len>0)
	{
		pmlmepriv->wps_assoc_resp_ie = rtw_malloc(ie_len);
		pmlmepriv->wps_assoc_resp_ie_len = ie_len;
		if ( pmlmepriv->wps_assoc_resp_ie == NULL) {
			DBG_871X("%s()-%d: rtw_malloc() ERROR!\n", __FUNCTION__, __LINE__);
			return -EINVAL;
		}

		_rtw_memcpy(pmlmepriv->wps_assoc_resp_ie, param->u.bcn_ie.buf, ie_len);
	}


	return ret;

}

static int rtw_set_hidden_ssid(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *mlmepriv = &(adapter->mlmepriv);
	struct mlme_ext_priv	*mlmeext = &(adapter->mlmeextpriv);
	struct mlme_ext_info	*mlmeinfo = &(mlmeext->mlmext_info);
	int ie_len;
	u8 *ssid_ie;
	char ssid[NDIS_802_11_LENGTH_SSID + 1];
	sint ssid_len = 0;
	u8 ignore_broadcast_ssid;

	if(check_fwstate(mlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EPERM;

	if (param->u.bcn_ie.reserved[0] != 0xea)
		return -EINVAL;

	mlmeinfo->hidden_ssid_mode = ignore_broadcast_ssid = param->u.bcn_ie.reserved[1];

	ie_len = len-12-2;// 12 = param header, 2:no packed
	ssid_ie = rtw_get_ie(param->u.bcn_ie.buf,  WLAN_EID_SSID, &ssid_len, ie_len);

	if (ssid_ie && ssid_len > 0 && ssid_len <= NDIS_802_11_LENGTH_SSID) {
		WLAN_BSSID_EX *pbss_network = &mlmepriv->cur_network.network;
		WLAN_BSSID_EX *pbss_network_ext = &mlmeinfo->network;

		_rtw_memcpy(ssid, ssid_ie+2, ssid_len);
		ssid[ssid_len] = 0x0;

		if(0)
			DBG_871X(FUNC_ADPT_FMT" ssid:(%s,%d), from ie:(%s,%d), (%s,%d)\n", FUNC_ADPT_ARG(adapter),
				ssid, ssid_len,
				pbss_network->Ssid.Ssid, pbss_network->Ssid.SsidLength,
				pbss_network_ext->Ssid.Ssid, pbss_network_ext->Ssid.SsidLength);

		_rtw_memcpy(pbss_network->Ssid.Ssid, (void *)ssid, ssid_len);
		pbss_network->Ssid.SsidLength = ssid_len;
		_rtw_memcpy(pbss_network_ext->Ssid.Ssid, (void *)ssid, ssid_len);
		pbss_network_ext->Ssid.SsidLength = ssid_len;

		if(0)
			DBG_871X(FUNC_ADPT_FMT" after ssid:(%s,%d), (%s,%d)\n", FUNC_ADPT_ARG(adapter),
				pbss_network->Ssid.Ssid, pbss_network->Ssid.SsidLength,
				pbss_network_ext->Ssid.Ssid, pbss_network_ext->Ssid.SsidLength);
	}

	DBG_871X(FUNC_ADPT_FMT" ignore_broadcast_ssid:%d, %s,%d\n", FUNC_ADPT_ARG(adapter),
		ignore_broadcast_ssid, ssid, ssid_len);

	return ret;
}

static int rtw_ioctl_acl_remove_sta(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		return -EINVAL;
	}

	ret = rtw_acl_remove_sta(padapter, param->sta_addr);

	return ret;

}

static int rtw_ioctl_acl_add_sta(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff)
	{
		return -EINVAL;
	}

	ret = rtw_acl_add_sta(padapter, param->sta_addr);

	return ret;

}

static int rtw_ioctl_set_macaddr_acl(struct net_device *dev, struct ieee_param *param, int len)
{
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);

	if(check_fwstate(pmlmepriv, WIFI_AP_STATE) != _TRUE)
		return -EINVAL;

	rtw_set_macaddr_acl(padapter, param->u.mlme.command);

	return ret;
}

static int rtw_hostapd_ioctl(struct net_device *dev, struct iw_point *p)
{
	struct ieee_param *param;
	int ret=0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	//DBG_871X("%s\n", __FUNCTION__);

	/*
	* this function is expect to call in master mode, which allows no power saving
	* so, we just check hw_init_completed
	*/

	if (!rtw_is_hw_init_completed(padapter)) {
		ret = -EPERM;
		goto out;
	}


	//if (p->length < sizeof(struct ieee_param) || !p->pointer){
	if(!p->pointer){
		ret = -EINVAL;
		goto out;
	}

	param = (struct ieee_param *)rtw_malloc(p->length);
	if (param == NULL)
	{
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(param, p->pointer, p->length))
	{
		rtw_mfree((u8*)param, p->length);
		ret = -EFAULT;
		goto out;
	}

	//DBG_871X("%s, cmd=%d\n", __FUNCTION__, param->cmd);

	switch (param->cmd)
	{
		case RTL871X_HOSTAPD_FLUSH:

			ret = rtw_hostapd_sta_flush(dev);

			break;

		case RTL871X_HOSTAPD_ADD_STA:

			ret = rtw_add_sta(dev, param);

			break;

		case RTL871X_HOSTAPD_REMOVE_STA:

			ret = rtw_del_sta(dev, param);

			break;

		case RTL871X_HOSTAPD_SET_BEACON:

			ret = rtw_set_beacon(dev, param, p->length);

			break;

		case RTL871X_SET_ENCRYPTION:

			ret = rtw_set_encryption(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_GET_WPAIE_STA:

			ret = rtw_get_sta_wpaie(dev, param);

			break;

		case RTL871X_HOSTAPD_SET_WPS_BEACON:

			ret = rtw_set_wps_beacon(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_SET_WPS_PROBE_RESP:

			ret = rtw_set_wps_probe_resp(dev, param, p->length);

	 		break;

		case RTL871X_HOSTAPD_SET_WPS_ASSOC_RESP:

			ret = rtw_set_wps_assoc_resp(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_SET_HIDDEN_SSID:

			ret = rtw_set_hidden_ssid(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_GET_INFO_STA:

			ret = rtw_ioctl_get_sta_data(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_SET_MACADDR_ACL:

			ret = rtw_ioctl_set_macaddr_acl(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_ACL_ADD_STA:

			ret = rtw_ioctl_acl_add_sta(dev, param, p->length);

			break;

		case RTL871X_HOSTAPD_ACL_REMOVE_STA:

			ret = rtw_ioctl_acl_remove_sta(dev, param, p->length);

			break;

		default:
			DBG_871X("Unknown hostapd request: %d\n", param->cmd);
			ret = -EOPNOTSUPP;
			break;

	}

	if (ret == 0 && copy_to_user(p->pointer, param, p->length))
		ret = -EFAULT;


	rtw_mfree((u8 *)param, p->length);

out:

	return ret;

}
#endif

static int rtw_wx_set_priv(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *awrq,
				char *extra)
{

#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	char *ext_dbg;
#endif

	int ret = 0;
	int len = 0;
	char *ext;
	int i;

	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct iw_point *dwrq = (struct iw_point*)awrq;

	//RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_, ("+rtw_wx_set_priv\n"));
	if(dwrq->length == 0)
		return -EFAULT;

	len = dwrq->length;
	if (!(ext = rtw_vmalloc(len)))
		return -ENOMEM;

	if (copy_from_user(ext, dwrq->pointer, len)) {
		rtw_vmfree(ext, len);
		return -EFAULT;
	}


	//RT_TRACE(_module_rtl871x_ioctl_os_c, _drv_notice_,
	//	 ("rtw_wx_set_priv: %s req=%s\n",
	//	  dev->name, ext));

	#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	if (!(ext_dbg = rtw_vmalloc(len)))
	{
		rtw_vmfree(ext, len);
		return -ENOMEM;
	}

	_rtw_memcpy(ext_dbg, ext, len);
	#endif

	//added for wps2.0 @20110524
	if(dwrq->flags == 0x8766 && len > 8)
	{
		u32 cp_sz;
		struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
		u8 *probereq_wpsie = ext;
		int probereq_wpsie_len = len;
		u8 wps_oui[4]={0x0,0x50,0xf2,0x04};

		if((_VENDOR_SPECIFIC_IE_ == probereq_wpsie[0]) &&
			(_rtw_memcmp(&probereq_wpsie[2], wps_oui, 4) ==_TRUE))
		{
			cp_sz = probereq_wpsie_len>MAX_WPS_IE_LEN ? MAX_WPS_IE_LEN:probereq_wpsie_len;

			if(pmlmepriv->wps_probe_req_ie)
			{
				u32 free_len = pmlmepriv->wps_probe_req_ie_len;
				pmlmepriv->wps_probe_req_ie_len = 0;
				rtw_mfree(pmlmepriv->wps_probe_req_ie, free_len);
				pmlmepriv->wps_probe_req_ie = NULL;
			}

			pmlmepriv->wps_probe_req_ie = rtw_malloc(cp_sz);
			if ( pmlmepriv->wps_probe_req_ie == NULL) {
				printk("%s()-%d: rtw_malloc() ERROR!\n", __FUNCTION__, __LINE__);
				ret =  -EINVAL;
				goto FREE_EXT;

			}

			_rtw_memcpy(pmlmepriv->wps_probe_req_ie, probereq_wpsie, cp_sz);
			pmlmepriv->wps_probe_req_ie_len = cp_sz;

		}

		goto FREE_EXT;

	}

	if(	len >= WEXT_CSCAN_HEADER_SIZE
		&& _rtw_memcmp(ext, WEXT_CSCAN_HEADER, WEXT_CSCAN_HEADER_SIZE) == _TRUE
	){
		ret = rtw_wx_set_scan(dev, info, awrq, ext);
		goto FREE_EXT;
	}

#ifdef CONFIG_ANDROID
	//DBG_871X("rtw_wx_set_priv: %s req=%s\n", dev->name, ext);

	i = rtw_android_cmdstr_to_num(ext);

	switch(i) {
		case ANDROID_WIFI_CMD_START :
			indicate_wx_custom_event(padapter, "START");
			break;
		case ANDROID_WIFI_CMD_STOP :
			indicate_wx_custom_event(padapter, "STOP");
			break;
		case ANDROID_WIFI_CMD_RSSI :
			{
				struct	mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
				struct	wlan_network	*pcur_network = &pmlmepriv->cur_network;

				if(check_fwstate(pmlmepriv, _FW_LINKED) == _TRUE) {
					sprintf(ext, "%s rssi %d", pcur_network->network.Ssid.Ssid, padapter->recvpriv.rssi);
				} else {
					sprintf(ext, "OK");
				}
			}
			break;
		case ANDROID_WIFI_CMD_LINKSPEED :
			{
				u16 mbps = rtw_get_cur_max_rate(padapter)/10;
				sprintf(ext, "LINKSPEED %d", mbps);
			}
			break;
		case ANDROID_WIFI_CMD_MACADDR :
			sprintf(ext, "MACADDR = " MAC_FMT, MAC_ARG(dev->dev_addr));
			break;
		case ANDROID_WIFI_CMD_SCAN_ACTIVE :
			{
				//rtw_set_scan_mode(padapter, SCAN_ACTIVE);
				sprintf(ext, "OK");
			}
			break;
		case ANDROID_WIFI_CMD_SCAN_PASSIVE :
			{
				//rtw_set_scan_mode(padapter, SCAN_PASSIVE);
				sprintf(ext, "OK");
			}
			break;

		case ANDROID_WIFI_CMD_COUNTRY :
			{
				char country_code[10];
				sscanf(ext, "%*s %s", country_code);
				rtw_set_country(padapter, country_code);
				sprintf(ext, "OK");
			}
			break;
		default :
			#ifdef  CONFIG_DEBUG_RTW_WX_SET_PRIV
			DBG_871X("%s: %s unknowned req=%s\n", __FUNCTION__,
				dev->name, ext_dbg);
			#endif

			sprintf(ext, "OK");

	}

	if (copy_to_user(dwrq->pointer, ext, min(dwrq->length, (u16)(strlen(ext)+1)) ) )
		ret = -EFAULT;

	#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	DBG_871X("%s: %s req=%s rep=%s dwrq->length=%d, strlen(ext)+1=%d\n", __FUNCTION__,
		dev->name, ext_dbg ,ext, dwrq->length, (u16)(strlen(ext)+1));
	#endif
#endif //end of CONFIG_ANDROID


FREE_EXT:

	rtw_vmfree(ext, len);
	#ifdef CONFIG_DEBUG_RTW_WX_SET_PRIV
	rtw_vmfree(ext_dbg, len);
	#endif

	//DBG_871X("rtw_wx_set_priv: (SIOCSIWPRIV) %s ret=%d\n",
	//		dev->name, ret);

	return ret;

}

#ifdef CONFIG_APPEND_VENDOR_IE_ENABLE

int rtw_vendor_ie_get(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	int ret = 0, vendor_ie_num = 0 , j , len = 0 , cmdlen;
	char *pstring;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	struct iw_point *p;
	u8 *ptmp;
	u32 vendor_ie_mask = 0;

	p = &wrqu->data;
	cmdlen = p->length;
	if (0 == cmdlen)
		return -EINVAL;

	ptmp = (u8 *)rtw_malloc(cmdlen);
	if (NULL == ptmp)
		return -ENOMEM;

	if (copy_from_user(ptmp, p->pointer, cmdlen)) {
		ret = -EFAULT;
		goto exit;
	}
	ret = sscanf(ptmp , "%d", &vendor_ie_num);
	if (vendor_ie_num > WLAN_MAX_VENDOR_IE_NUM - 1) {
		ret = -EFAULT;
		goto exit;
	}
	vendor_ie_mask = pmlmepriv->vendor_ie_mask[vendor_ie_num];
	pstring = extra;
	pstring += sprintf(pstring , "\nVendor IE num %d , Mask:%x " , vendor_ie_num , vendor_ie_mask);

	if (vendor_ie_mask & WIFI_BEACON_VENDOR_IE_BIT)
		pstring += sprintf(pstring , "[Beacon]");
	if (vendor_ie_mask & WIFI_PROBEREQ_VENDOR_IE_BIT)
		pstring += sprintf(pstring , "[Probe Req]");
	if (vendor_ie_mask & WIFI_PROBERESP_VENDOR_IE_BIT)
		pstring += sprintf(pstring , "[Probe Resp]");
	if (vendor_ie_mask & WIFI_ASSOCREQ_VENDOR_IE_BIT)
		pstring += sprintf(pstring , "[Assoc Req]");
	if (vendor_ie_mask & WIFI_ASSOCRESP_VENDOR_IE_BIT)
		pstring += sprintf(pstring , "[Assoc Resp]");

	pstring += sprintf(pstring , "\nVendor IE:\n");
	for (j = 0 ; j < pmlmepriv->vendor_ielen[vendor_ie_num]  ; j++)
		pstring += sprintf(pstring , "%02x" , pmlmepriv->vendor_ie[vendor_ie_num][j]);

	wrqu->data.length = pstring - extra;

exit:
	rtw_mfree(ptmp, cmdlen);

	return 0;
}

int rtw_vendor_ie_set(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	int ret = 0, i , len = 0 , totoal_ie_len = 0 , total_ie_len_byte = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	u32 vendor_ie_mask = 0;
	u32 vendor_ie_num = 0;
	u32 id, elen;

	ret = sscanf(extra, "%d,%x,%*s", &vendor_ie_num , &vendor_ie_mask);
	if (strrchr(extra , ','))
		extra = strrchr(extra , ',') + 1;
	else
		return -EINVAL;
	totoal_ie_len = strlen(extra);
	DBG_871X("[%s] vendor_ie_num = %d , vendor_ie_mask = %x , vendor_ie = %s , len = %d\n", __func__ , vendor_ie_num , vendor_ie_mask , extra  , totoal_ie_len);

	if (vendor_ie_num  > WLAN_MAX_VENDOR_IE_NUM - 1) {
		DBG_871X("[%s] only support %d vendor ie\n", __func__ , WLAN_MAX_VENDOR_IE_NUM);
		return -EFAULT;
	}

	if (totoal_ie_len > WLAN_MAX_VENDOR_IE_LEN) {
		DBG_871X("[%s] Fail , not support ie length extend %d\n", __func__ , WLAN_MAX_VENDOR_IE_LEN);
		return -EFAULT;
	}

	if (vendor_ie_mask == 0) {
		DBG_871X("[%s] Clear vendor_ie_num %d group\n", __func__ , vendor_ie_num);
		goto _clear_path;
	}

	if (totoal_ie_len % 2 != 0) {
		DBG_871X("[%s]  Fail , IE length = %zu is odd\n" , __func__ , strlen(extra));
		return -EFAULT;
	}

	if (totoal_ie_len > 0) {
		for (i = 0  ; i < strlen(extra) ; i += 2) {
			pmlmepriv->vendor_ie[vendor_ie_num][len] = key_2char2num(extra[i] , extra[i + 1]);
			if (len == 0) {
				id = pmlmepriv->vendor_ie[vendor_ie_num][len];
				if (id != WLAN_EID_VENDOR_SPECIFIC) {
					DBG_871X("[%s] Fail , VENDOR SPECIFIC IE ID \"%x\" was not correct\n", __func__ , id);
					goto _clear_path;
				}
			} else if (len == 1) {
				total_ie_len_byte = (totoal_ie_len / 2) - 2;
				elen = pmlmepriv->vendor_ie[vendor_ie_num][len];
				if (elen != total_ie_len_byte) {
					DBG_871X("[%s] Fail , Input IE length = \"%d\"(hex:%x) bytes , not match input total IE context length \"%d\" bytes\n", __func__ , elen , elen ,
						 total_ie_len_byte);
					goto _clear_path;
				}
			}
			len++;
		}
		pmlmepriv->vendor_ielen[vendor_ie_num] = len;
	} else
		pmlmepriv->vendor_ielen[vendor_ie_num] = 0;



	if (vendor_ie_mask & WIFI_BEACON_VENDOR_IE_BIT)
		DBG_871X("[%s] Beacon append vendor ie\n", __func__);
	if (vendor_ie_mask & WIFI_PROBEREQ_VENDOR_IE_BIT)
		DBG_871X("[%s] Probe Req append vendor ie\n", __func__);
	if (vendor_ie_mask & WIFI_PROBERESP_VENDOR_IE_BIT)
		DBG_871X("[%s] Probe Resp  append vendor ie\n", __func__);
	if (vendor_ie_mask & WIFI_ASSOCREQ_VENDOR_IE_BIT)
		DBG_871X("[%s] Assoc Req append vendor ie\n", __func__);
	if (vendor_ie_mask & WIFI_ASSOCRESP_VENDOR_IE_BIT)
		DBG_871X("[%s] Assoc Resp append vendor ie\n", __func__);

	pmlmepriv->vendor_ie_mask[vendor_ie_num] = vendor_ie_mask;

	return ret;

_clear_path:
	_rtw_memset(pmlmepriv->vendor_ie[vendor_ie_num] , 0 , sizeof(u32) * WLAN_MAX_VENDOR_IE_LEN);
	pmlmepriv->vendor_ielen[vendor_ie_num] = 0;
	pmlmepriv->vendor_ie_mask[vendor_ie_num] = 0;
	return -EFAULT;
}
#endif

#ifdef CONFIG_MP_INCLUDED

#endif /*#if defined(CONFIG_MP_INCLUDED)*/





#ifdef CONFIG_INTEL_WIDI
static int rtw_widi_set(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	int ret = 0;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	process_intel_widi_cmd(padapter, extra);

	return ret;
}

static int rtw_widi_set_probe_request(struct net_device *dev,
                               struct iw_request_info *info,
                               union iwreq_data *wrqu, char *extra)
{
	int	ret = 0;
	u8	*pbuf = NULL;
	_adapter	*padapter = (_adapter *)rtw_netdev_priv(dev);

	pbuf = rtw_malloc(sizeof(l2_msg_t));
	if(pbuf)
	{
		if ( copy_from_user(pbuf, wrqu->data.pointer, wrqu->data.length) )
			ret = -EFAULT;
		//_rtw_memcpy(pbuf, wrqu->data.pointer, wrqu->data.length);

		if( wrqu->data.flags == 0 )
			intel_widi_wk_cmd(padapter, INTEL_WIDI_ISSUE_PROB_WK, pbuf, sizeof(l2_msg_t));
		else if( wrqu->data.flags == 1 )
			rtw_set_wfd_rds_sink_info( padapter, (l2_msg_t *)pbuf );
	}
	return ret;
}
#endif // CONFIG_INTEL_WIDI

#ifdef CONFIG_MAC_LOOPBACK_DRIVER

static s32 initLoopback(PADAPTER padapter)
{
	PLOOPBACKDATA ploopback;


	if (padapter->ploopback == NULL) {
		ploopback = (PLOOPBACKDATA)rtw_zmalloc(sizeof(LOOPBACKDATA));
		if (ploopback == NULL) return -ENOMEM;

		_rtw_init_sema(&ploopback->sema, 0);
		ploopback->bstop = _TRUE;
		ploopback->cnt = 0;
		ploopback->size = 300;
		_rtw_memset(ploopback->msg, 0, sizeof(ploopback->msg));

		padapter->ploopback = ploopback;
	}

	return 0;
}

static void freeLoopback(PADAPTER padapter)
{
	PLOOPBACKDATA ploopback;


	ploopback = padapter->ploopback;
	if (ploopback) {
		rtw_mfree((u8*)ploopback, sizeof(LOOPBACKDATA));
		padapter->ploopback = NULL;
	}
}

static s32 initpseudoadhoc(PADAPTER padapter)
{
	NDIS_802_11_NETWORK_INFRASTRUCTURE networkType;
	s32 err;

	networkType = Ndis802_11IBSS;
	err = rtw_set_802_11_infrastructure_mode(padapter, networkType);
	if (err == _FALSE) return _FAIL;

	err = rtw_setopmode_cmd(padapter, networkType,_TRUE);
	if (err == _FAIL) return _FAIL;

	return _SUCCESS;
}

static s32 createpseudoadhoc(PADAPTER padapter)
{
	NDIS_802_11_AUTHENTICATION_MODE authmode;
	struct mlme_priv *pmlmepriv;
	NDIS_802_11_SSID *passoc_ssid;
	WLAN_BSSID_EX *pdev_network;
	u8 *pibss;
	u8 ssid[] = "pseduo_ad-hoc";
	s32 err;
	_irqL irqL;


	pmlmepriv = &padapter->mlmepriv;

	authmode = Ndis802_11AuthModeOpen;
	err = rtw_set_802_11_authentication_mode(padapter, authmode);
	if (err == _FALSE) return _FAIL;

	passoc_ssid = &pmlmepriv->assoc_ssid;
	_rtw_memset(passoc_ssid, 0, sizeof(NDIS_802_11_SSID));
	passoc_ssid->SsidLength = sizeof(ssid) - 1;
	_rtw_memcpy(passoc_ssid->Ssid, ssid, passoc_ssid->SsidLength);

	pdev_network = &padapter->registrypriv.dev_network;
	pibss = padapter->registrypriv.dev_network.MacAddress;
	_rtw_memcpy(&pdev_network->Ssid, passoc_ssid, sizeof(NDIS_802_11_SSID));

	rtw_update_registrypriv_dev_network(padapter);
	rtw_generate_random_ibss(pibss);

	_enter_critical_bh(&pmlmepriv->lock, &irqL);
	pmlmepriv->fw_state = WIFI_ADHOC_MASTER_STATE;
	_exit_critical_bh(&pmlmepriv->lock, &irqL);

#if 0
	err = rtw_create_ibss_cmd(padapter, 0);
	if (err == _FAIL) return _FAIL;
#else
{
	struct wlan_network *pcur_network;
	struct sta_info *psta;

	//3  create a new psta
	pcur_network = &pmlmepriv->cur_network;

	//clear psta in the cur_network, if any
	psta = rtw_get_stainfo(&padapter->stapriv, pcur_network->network.MacAddress);
	if (psta) rtw_free_stainfo(padapter, psta);

	psta = rtw_alloc_stainfo(&padapter->stapriv, pibss);
	if (psta == NULL) return _FAIL;

	//3  join psudo AdHoc
	pcur_network->join_res = 1;
	pcur_network->aid = psta->aid = 1;
	_rtw_memcpy(&pcur_network->network, pdev_network, get_WLAN_BSSID_EX_sz(pdev_network));

	// set msr to WIFI_FW_ADHOC_STATE
#if 0
	Set_NETYPE0_MSR(padapter, WIFI_FW_ADHOC_STATE);
#else
	{
		u8 val8;

		val8 = rtw_read8(padapter, MSR);
		val8 &= 0xFC; // clear NETYPE0
		val8 |= WIFI_FW_ADHOC_STATE & 0x3;
		rtw_write8(padapter, MSR, val8);
	}
#endif
}
#endif

	return _SUCCESS;
}

static struct xmit_frame* createloopbackpkt(PADAPTER padapter, u32 size)
{
	struct xmit_priv *pxmitpriv;
	struct xmit_frame *pframe;
	struct xmit_buf *pxmitbuf;
	struct pkt_attrib *pattrib;
	struct tx_desc *desc;
	u8 *pkt_start, *pkt_end, *ptr;
	struct rtw_ieee80211_hdr *hdr;
	s32 bmcast;
	_irqL irqL;


	if ((TXDESC_SIZE + WLANHDR_OFFSET + size) > MAX_XMITBUF_SZ) return NULL;

	pxmitpriv = &padapter->xmitpriv;
	pframe = NULL;

	//2 1. allocate xmit frame
	pframe = rtw_alloc_xmitframe(pxmitpriv);
	if (pframe == NULL) return NULL;
	pframe->padapter = padapter;

	//2 2. allocate xmit buffer
	_enter_critical_bh(&pxmitpriv->lock, &irqL);
	pxmitbuf = rtw_alloc_xmitbuf(pxmitpriv);
	_exit_critical_bh(&pxmitpriv->lock, &irqL);
	if (pxmitbuf == NULL) {
		rtw_free_xmitframe(pxmitpriv, pframe);
		return NULL;
	}

	pframe->pxmitbuf = pxmitbuf;
	pframe->buf_addr = pxmitbuf->pbuf;
	pxmitbuf->priv_data = pframe;

	//2 3. update_attrib()
	pattrib = &pframe->attrib;

	// init xmitframe attribute
	_rtw_memset(pattrib, 0, sizeof(struct pkt_attrib));

	pattrib->ether_type = 0x8723;
	_rtw_memcpy(pattrib->src, adapter_mac_addr(padapter), ETH_ALEN);
	_rtw_memcpy(pattrib->ta, pattrib->src, ETH_ALEN);
	_rtw_memset(pattrib->dst, 0xFF, ETH_ALEN);
	_rtw_memcpy(pattrib->ra, pattrib->dst, ETH_ALEN);

//	pattrib->dhcp_pkt = 0;
//	pattrib->pktlen = 0;
	pattrib->ack_policy = 0;
//	pattrib->pkt_hdrlen = ETH_HLEN;
	pattrib->hdrlen = WLAN_HDR_A3_LEN;
	pattrib->subtype = WIFI_DATA;
	pattrib->priority = 0;
	pattrib->qsel = pattrib->priority;
//	do_queue_select(padapter, pattrib);
	pattrib->nr_frags = 1;
	pattrib->encrypt = 0;
	pattrib->bswenc = _FALSE;
	pattrib->qos_en = _FALSE;

	bmcast = IS_MCAST(pattrib->ra);
	if (bmcast) {
		pattrib->mac_id = 1;
		pattrib->psta = rtw_get_bcmc_stainfo(padapter);
	} else {
		pattrib->mac_id = 0;
		pattrib->psta = rtw_get_stainfo(&padapter->stapriv, get_bssid(&padapter->mlmepriv));
	}

	pattrib->pktlen = size;
	pattrib->last_txcmdsz = pattrib->hdrlen + pattrib->pktlen;

	//2 4. fill TX descriptor
	desc = (struct tx_desc*)pframe->buf_addr;
	_rtw_memset(desc, 0, TXDESC_SIZE);

	fill_default_txdesc(pframe, (u8*)desc);

	// Hw set sequence number
	((PTXDESC)desc)->hwseq_en = 0; // HWSEQ_EN, 0:disable, 1:enable
//	((PTXDESC)desc)->hwseq_sel = 0; // HWSEQ_SEL

	((PTXDESC)desc)->disdatafb = 1;

	// convert to little endian
	desc->txdw0 = cpu_to_le32(desc->txdw0);
	desc->txdw1 = cpu_to_le32(desc->txdw1);
	desc->txdw2 = cpu_to_le32(desc->txdw2);
	desc->txdw3 = cpu_to_le32(desc->txdw3);
	desc->txdw4 = cpu_to_le32(desc->txdw4);
	desc->txdw5 = cpu_to_le32(desc->txdw5);
	desc->txdw6 = cpu_to_le32(desc->txdw6);
	desc->txdw7 = cpu_to_le32(desc->txdw7);

	cal_txdesc_chksum(desc);

	//2 5. coalesce
	pkt_start = pframe->buf_addr + TXDESC_SIZE;
	pkt_end = pkt_start + pattrib->last_txcmdsz;

	//3 5.1. make wlan header, make_wlanhdr()
	hdr = (struct rtw_ieee80211_hdr *)pkt_start;
	SetFrameSubType(&hdr->frame_ctl, pattrib->subtype);
	_rtw_memcpy(hdr->addr1, pattrib->dst, ETH_ALEN); // DA
	_rtw_memcpy(hdr->addr2, pattrib->src, ETH_ALEN); // SA
	_rtw_memcpy(hdr->addr3, get_bssid(&padapter->mlmepriv), ETH_ALEN); // RA, BSSID

	//3 5.2. make payload
	ptr = pkt_start + pattrib->hdrlen;
	get_random_bytes(ptr, pkt_end - ptr);

	pxmitbuf->len = TXDESC_SIZE + pattrib->last_txcmdsz;
	pxmitbuf->ptail += pxmitbuf->len;

	return pframe;
}

static void freeloopbackpkt(PADAPTER padapter, struct xmit_frame *pframe)
{
	struct xmit_priv *pxmitpriv;
	struct xmit_buf *pxmitbuf;


	pxmitpriv = &padapter->xmitpriv;
	pxmitbuf = pframe->pxmitbuf;

	rtw_free_xmitframe(pxmitpriv, pframe);
	rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
}

static void printdata(u8 *pbuf, u32 len)
{
	u32 i, val;


	for (i = 0; (i+4) <= len; i+=4) {
		printk("%08X", *(u32*)(pbuf + i));
		if ((i+4) & 0x1F) printk(" ");
		else printk("\n");
	}

	if (i < len)
	{
#ifdef CONFIG_BIG_ENDIAN
		for (; i < len, i++)
			printk("%02X", pbuf+i);
#else // CONFIG_LITTLE_ENDIAN
#if 0
		val = 0;
		_rtw_memcpy(&val, pbuf + i, len - i);
		printk("%8X", val);
#else
		u8 str[9];
		u8 n;
		val = 0;
		n = len - i;
		_rtw_memcpy(&val, pbuf+i, n);
		sprintf(str, "%08X", val);
		n = (4 - n) * 2;
		printk("%8s", str+n);
#endif
#endif // CONFIG_LITTLE_ENDIAN
	}
	printk("\n");
}

static u8 pktcmp(PADAPTER padapter, u8 *txbuf, u32 txsz, u8 *rxbuf, u32 rxsz)
{
	PHAL_DATA_TYPE phal;
	struct recv_stat *prxstat;
	struct recv_stat report;
	PRXREPORT prxreport;
	u32 drvinfosize;
	u32 rxpktsize;
	u8 fcssize;
	u8 ret = _FALSE;

	prxstat = (struct recv_stat*)rxbuf;
	report.rxdw0 = le32_to_cpu(prxstat->rxdw0);
	report.rxdw1 = le32_to_cpu(prxstat->rxdw1);
	report.rxdw2 = le32_to_cpu(prxstat->rxdw2);
	report.rxdw3 = le32_to_cpu(prxstat->rxdw3);
	report.rxdw4 = le32_to_cpu(prxstat->rxdw4);
	report.rxdw5 = le32_to_cpu(prxstat->rxdw5);

	prxreport = (PRXREPORT)&report;
	drvinfosize = prxreport->drvinfosize << 3;
	rxpktsize = prxreport->pktlen;

	phal = GET_HAL_DATA(padapter);
	if (phal->ReceiveConfig & RCR_APPFCS) fcssize = IEEE80211_FCS_LEN;
	else fcssize = 0;

	if ((txsz - TXDESC_SIZE) != (rxpktsize - fcssize)) {
		DBG_8192C("%s: ERROR! size not match tx/rx=%d/%d !\n",
			__func__, txsz - TXDESC_SIZE, rxpktsize - fcssize);
		ret = _FALSE;
	} else {
		ret = _rtw_memcmp(txbuf + TXDESC_SIZE,\
						  rxbuf + RXDESC_SIZE + drvinfosize,\
						  txsz - TXDESC_SIZE);
		if (ret == _FALSE) {
			DBG_8192C("%s: ERROR! pkt content mismatch!\n", __func__);
		}
	}

	if (ret == _FALSE)
	{
		DBG_8192C("\n%s: TX PKT total=%d, desc=%d, content=%d\n",
			__func__, txsz, TXDESC_SIZE, txsz - TXDESC_SIZE);
		DBG_8192C("%s: TX DESC size=%d\n", __func__, TXDESC_SIZE);
		printdata(txbuf, TXDESC_SIZE);
		DBG_8192C("%s: TX content size=%d\n", __func__, txsz - TXDESC_SIZE);
		printdata(txbuf + TXDESC_SIZE, txsz - TXDESC_SIZE);

		DBG_8192C("\n%s: RX PKT read=%d offset=%d(%d,%d) content=%d\n",
			__func__, rxsz, RXDESC_SIZE + drvinfosize, RXDESC_SIZE, drvinfosize, rxpktsize);
		if (rxpktsize != 0)
		{
			DBG_8192C("%s: RX DESC size=%d\n", __func__, RXDESC_SIZE);
			printdata(rxbuf, RXDESC_SIZE);
			DBG_8192C("%s: RX drvinfo size=%d\n", __func__, drvinfosize);
			printdata(rxbuf + RXDESC_SIZE, drvinfosize);
			DBG_8192C("%s: RX content size=%d\n", __func__, rxpktsize);
			printdata(rxbuf + RXDESC_SIZE + drvinfosize, rxpktsize);
		} else {
			DBG_8192C("%s: RX data size=%d\n", __func__, rxsz);
			printdata(rxbuf, rxsz);
		}
	}

	return ret;
}

thread_return lbk_thread(thread_context context)
{
	s32 err;
	PADAPTER padapter;
	PLOOPBACKDATA ploopback;
	struct xmit_frame *pxmitframe;
	u32 cnt, ok, fail, headerlen;
	u32 pktsize;
	u32 ff_hwaddr;


	padapter = (PADAPTER)context;
	ploopback = padapter->ploopback;
	if (ploopback == NULL) return -1;
	cnt = 0;
	ok = 0;
	fail = 0;

	daemonize("%s", "RTW_LBK_THREAD");
	allow_signal(SIGTERM);

	do {
		if (ploopback->size == 0) {
			get_random_bytes(&pktsize, 4);
			pktsize = (pktsize % 1535) + 1; // 1~1535
		} else
			pktsize = ploopback->size;

		pxmitframe = createloopbackpkt(padapter, pktsize);
		if (pxmitframe == NULL) {
			sprintf(ploopback->msg, "loopback FAIL! 3. create Packet FAIL!");
			break;
		}

		ploopback->txsize = TXDESC_SIZE + pxmitframe->attrib.last_txcmdsz;
		_rtw_memcpy(ploopback->txbuf, pxmitframe->buf_addr, ploopback->txsize);
		ff_hwaddr = rtw_get_ff_hwaddr(pxmitframe);
		cnt++;
		DBG_8192C("%s: wirte port cnt=%d size=%d\n", __func__, cnt, ploopback->txsize);
		pxmitframe->pxmitbuf->pdata = ploopback->txbuf;
		rtw_write_port(padapter, ff_hwaddr, ploopback->txsize, (u8 *)pxmitframe->pxmitbuf);

		// wait for rx pkt
		_rtw_down_sema(&ploopback->sema);

		err = pktcmp(padapter, ploopback->txbuf, ploopback->txsize, ploopback->rxbuf, ploopback->rxsize);
		if (err == _TRUE)
			ok++;
		else
			fail++;

		ploopback->txsize = 0;
		_rtw_memset(ploopback->txbuf, 0, 0x8000);
		ploopback->rxsize = 0;
		_rtw_memset(ploopback->rxbuf, 0, 0x8000);

		freeloopbackpkt(padapter, pxmitframe);
		pxmitframe = NULL;

		if (signal_pending(current)) {
			flush_signals(current);
		}

		if ((ploopback->bstop == _TRUE) ||
			((ploopback->cnt != 0) && (ploopback->cnt == cnt)))
		{
			u32 ok_rate, fail_rate, all;
			all = cnt;
			ok_rate = (ok*100)/all;
			fail_rate = (fail*100)/all;
			sprintf(ploopback->msg,\
					"loopback result: ok=%d%%(%d/%d),error=%d%%(%d/%d)",\
					ok_rate, ok, all, fail_rate, fail, all);
			break;
		}
	} while (1);

	ploopback->bstop = _TRUE;

	thread_exit();
}

static void loopbackTest(PADAPTER padapter, u32 cnt, u32 size, u8* pmsg)
{
	PLOOPBACKDATA ploopback;
	u32 len;
	s32 err;


	ploopback = padapter->ploopback;

	if (ploopback)
	{
		if (ploopback->bstop == _FALSE) {
			ploopback->bstop = _TRUE;
			_rtw_up_sema(&ploopback->sema);
		}
		len = 0;
		do {
			len = strlen(ploopback->msg);
			if (len) break;
			rtw_msleep_os(1);
		} while (1);
		_rtw_memcpy(pmsg, ploopback->msg, len+1);
		freeLoopback(padapter);

		return;
	}

	// disable dynamic algorithm
	rtw_phydm_ability_backup(padapter);
	rtw_phydm_func_disable_all(padapter);

	// create pseudo ad-hoc connection
	err = initpseudoadhoc(padapter);
	if (err == _FAIL) {
		sprintf(pmsg, "loopback FAIL! 1.1 init ad-hoc FAIL!");
		return;
	}

	err = createpseudoadhoc(padapter);
	if (err == _FAIL) {
		sprintf(pmsg, "loopback FAIL! 1.2 create ad-hoc master FAIL!");
		return;
	}

	err = initLoopback(padapter);
	if (err) {
		sprintf(pmsg, "loopback FAIL! 2. init FAIL! error code=%d", err);
		return;
	}

	ploopback = padapter->ploopback;

	ploopback->bstop = _FALSE;
	ploopback->cnt = cnt;
	ploopback->size = size;
	ploopback->lbkthread = kthread_run(lbk_thread, padapter, "RTW_LBK_THREAD");
	if (IS_ERR(padapter->lbkthread))
	{
		freeLoopback(padapter);
		sprintf(pmsg, "loopback start FAIL! cnt=%d", cnt);
		return;
	}

	sprintf(pmsg, "loopback start! cnt=%d", cnt);
}
#endif // CONFIG_MAC_LOOPBACK_DRIVER

static int rtw_test(
	struct net_device *dev,
	struct iw_request_info *info,
	union iwreq_data *wrqu, char *extra)
{
	u32 len;
	u8 *pbuf, *pch;
	char *ptmp;
	u8 *delim = ",";
	PADAPTER padapter = rtw_netdev_priv(dev);


	DBG_871X("+%s\n", __func__);
	len = wrqu->data.length;

	pbuf = (u8*)rtw_zmalloc(len);
	if (pbuf == NULL) {
		DBG_871X("%s: no memory!\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(pbuf, wrqu->data.pointer, len)) {
		rtw_mfree(pbuf, len);
		DBG_871X("%s: copy from user fail!\n", __func__);
		return -EFAULT;
	}
	DBG_871X("%s: string=\"%s\"\n", __func__, pbuf);

	ptmp = (char*)pbuf;
	pch = strsep(&ptmp, delim);
	if ((pch == NULL) || (strlen(pch) == 0)) {
		rtw_mfree(pbuf, len);
		DBG_871X("%s: parameter error(level 1)!\n", __func__);
		return -EFAULT;
	}

#ifdef CONFIG_MAC_LOOPBACK_DRIVER
	if (strcmp(pch, "loopback") == 0)
	{
		s32 cnt = 0;
		u32 size = 64;

		pch = strsep(&ptmp, delim);
		if ((pch == NULL) || (strlen(pch) == 0)) {
			rtw_mfree(pbuf, len);
			DBG_871X("%s: parameter error(level 2)!\n", __func__);
			return -EFAULT;
		}

		sscanf(pch, "%d", &cnt);
		DBG_871X("%s: loopback cnt=%d\n", __func__, cnt);

		pch = strsep(&ptmp, delim);
		if ((pch == NULL) || (strlen(pch) == 0)) {
			rtw_mfree(pbuf, len);
			DBG_871X("%s: parameter error(level 2)!\n", __func__);
			return -EFAULT;
		}

		sscanf(pch, "%d", &size);
		DBG_871X("%s: loopback size=%d\n", __func__, size);

		loopbackTest(padapter, cnt, size, extra);
		wrqu->data.length = strlen(extra) + 1;

		goto free_buf;
	}
#endif


	if (strcmp(pch, "h2c") == 0) {
		u8 param[8];
		u8 count = 0;
		u32 tmp;
		u8 i;
		u32 pos;
		s32 ret;

		do {
			pch = strsep(&ptmp, delim);
			if ((pch == NULL) || (strlen(pch) == 0))
				break;

			sscanf(pch, "%x", &tmp);
			param[count++] = (u8)tmp;
		} while (count < 8);

		if (count == 0) {
			rtw_mfree(pbuf, len);
			DBG_871X("%s: parameter error(level 2)!\n", __func__);
			return -EFAULT;
		}

		ret = rtw_hal_fill_h2c_cmd(padapter, param[0], count-1, &param[1]);

		pos = sprintf(extra, "H2C ID=0x%02x content=", param[0]);
		for (i=1; i<count; i++) {
			pos += sprintf(extra+pos, "%02x,", param[i]);
		}
		extra[pos] = 0;
		pos--;
		pos += sprintf(extra+pos, " %s", ret==_FAIL?"FAIL":"OK");

		wrqu->data.length = strlen(extra) + 1;

		goto free_buf;
	}

free_buf:
	rtw_mfree(pbuf, len);
	return 0;
}

static iw_handler rtw_handlers[] =
{
	NULL,					/* SIOCSIWCOMMIT */
	rtw_wx_get_name,		/* SIOCGIWNAME */
	dummy,					/* SIOCSIWNWID */
	dummy,					/* SIOCGIWNWID */
	rtw_wx_set_freq,		/* SIOCSIWFREQ */
	rtw_wx_get_freq,		/* SIOCGIWFREQ */
	rtw_wx_set_mode,		/* SIOCSIWMODE */
	rtw_wx_get_mode,		/* SIOCGIWMODE */
	dummy,					/* SIOCSIWSENS */
	rtw_wx_get_sens,		/* SIOCGIWSENS */
	NULL,					/* SIOCSIWRANGE */
	rtw_wx_get_range,		/* SIOCGIWRANGE */
	rtw_wx_set_priv,		/* SIOCSIWPRIV */
	NULL,					/* SIOCGIWPRIV */
	NULL,					/* SIOCSIWSTATS */
	NULL,					/* SIOCGIWSTATS */
	dummy,					/* SIOCSIWSPY */
	dummy,					/* SIOCGIWSPY */
	NULL,					/* SIOCGIWTHRSPY */
	NULL,					/* SIOCWIWTHRSPY */
	rtw_wx_set_wap,		/* SIOCSIWAP */
	rtw_wx_get_wap,		/* SIOCGIWAP */
	rtw_wx_set_mlme,		/* request MLME operation; uses struct iw_mlme */
	dummy,					/* SIOCGIWAPLIST -- depricated */
	rtw_wx_set_scan,		/* SIOCSIWSCAN */
	rtw_wx_get_scan,		/* SIOCGIWSCAN */
	rtw_wx_set_essid,		/* SIOCSIWESSID */
	rtw_wx_get_essid,		/* SIOCGIWESSID */
	dummy,					/* SIOCSIWNICKN */
	rtw_wx_get_nick,		/* SIOCGIWNICKN */
	NULL,					/* -- hole -- */
	NULL,					/* -- hole -- */
	rtw_wx_set_rate,		/* SIOCSIWRATE */
	rtw_wx_get_rate,		/* SIOCGIWRATE */
	rtw_wx_set_rts,			/* SIOCSIWRTS */
	rtw_wx_get_rts,			/* SIOCGIWRTS */
	rtw_wx_set_frag,		/* SIOCSIWFRAG */
	rtw_wx_get_frag,		/* SIOCGIWFRAG */
	dummy,					/* SIOCSIWTXPOW */
	dummy,					/* SIOCGIWTXPOW */
	dummy,					/* SIOCSIWRETRY */
	rtw_wx_get_retry,		/* SIOCGIWRETRY */
	rtw_wx_set_enc,			/* SIOCSIWENCODE */
	rtw_wx_get_enc,			/* SIOCGIWENCODE */
	dummy,					/* SIOCSIWPOWER */
	rtw_wx_get_power,		/* SIOCGIWPOWER */
	NULL,					/*---hole---*/
	NULL,					/*---hole---*/
	rtw_wx_set_gen_ie,		/* SIOCSIWGENIE */
	NULL,					/* SIOCGWGENIE */
	rtw_wx_set_auth,		/* SIOCSIWAUTH */
	NULL,					/* SIOCGIWAUTH */
	rtw_wx_set_enc_ext,		/* SIOCSIWENCODEEXT */
	NULL,					/* SIOCGIWENCODEEXT */
	rtw_wx_set_pmkid,		/* SIOCSIWPMKSA */
	NULL,					/*---hole---*/
};


static const struct iw_priv_args rtw_private_args[] = {
	{
		SIOCIWFIRSTPRIV + 0x0,
		IW_PRIV_TYPE_CHAR | 0x7FF, 0, "write"
	},
	{
		SIOCIWFIRSTPRIV + 0x1,
		IW_PRIV_TYPE_CHAR | 0x7FF,
		IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | IFNAMSIZ, "read"
	},
	{
		SIOCIWFIRSTPRIV + 0x2, 0, 0, "driver_ext"
	},
	{
		SIOCIWFIRSTPRIV + 0x3, 0, 0, "mp_ioctl"
	},
	{
		SIOCIWFIRSTPRIV + 0x4,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "apinfo"
	},
	{
		SIOCIWFIRSTPRIV + 0x5,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setpid"
	},
	{
		SIOCIWFIRSTPRIV + 0x6,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_start"
	},
//for PLATFORM_MT53XX
	{
		SIOCIWFIRSTPRIV + 0x7,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "get_sensitivity"
	},
	{
		SIOCIWFIRSTPRIV + 0x8,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_prob_req_ie"
	},
	{
		SIOCIWFIRSTPRIV + 0x9,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wps_assoc_req_ie"
	},

//for RTK_DMP_PLATFORM
	{
		SIOCIWFIRSTPRIV + 0xA,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "channel_plan"
	},

	{
		SIOCIWFIRSTPRIV + 0xB,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "dbg"
	},
	{
		SIOCIWFIRSTPRIV + 0xC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "rfw"
	},
	{
		SIOCIWFIRSTPRIV + 0xD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | IFNAMSIZ, "rfr"
	},
#if 0
	{
		SIOCIWFIRSTPRIV + 0xE,0,0, "wowlan_ctrl"
	},
#endif
	{
		SIOCIWFIRSTPRIV + 0x10,
		IW_PRIV_TYPE_CHAR | 1024, 0, "p2p_set"
	},
	{
		SIOCIWFIRSTPRIV + 0x11,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK , "p2p_get"
	},
	{
		SIOCIWFIRSTPRIV + 0x12, 0, 0, "NULL"
	},
	{
		SIOCIWFIRSTPRIV + 0x13,
		IW_PRIV_TYPE_CHAR | 64, IW_PRIV_TYPE_CHAR | 64 , "p2p_get2"
	},
	{
		SIOCIWFIRSTPRIV + 0x14,
		IW_PRIV_TYPE_CHAR  | 64, 0, "tdls"
	},
	{
		SIOCIWFIRSTPRIV + 0x15,
		IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024 , "tdls_get"
	},
	{
		SIOCIWFIRSTPRIV + 0x16,
		IW_PRIV_TYPE_CHAR | 64, 0, "pm_set"
	},

	{SIOCIWFIRSTPRIV + 0x18, IW_PRIV_TYPE_CHAR | IFNAMSIZ , 0 , "rereg_nd_name"},
#ifdef CONFIG_MP_INCLUDED
	{SIOCIWFIRSTPRIV + 0x1A, IW_PRIV_TYPE_CHAR | 1024, 0,  "NULL"},
	{SIOCIWFIRSTPRIV + 0x1B, IW_PRIV_TYPE_CHAR | 128, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "NULL"},
#else
	{SIOCIWFIRSTPRIV + 0x1A, IW_PRIV_TYPE_CHAR | 1024, 0, "efuse_set"},
	{SIOCIWFIRSTPRIV + 0x1B, IW_PRIV_TYPE_CHAR | 128, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "efuse_get"},
#endif
	{
		SIOCIWFIRSTPRIV + 0x1D,
		IW_PRIV_TYPE_CHAR | 40, IW_PRIV_TYPE_CHAR | 0x7FF, "test"
	},

#ifdef CONFIG_INTEL_WIDI
	{
		SIOCIWFIRSTPRIV + 0x1E,
		IW_PRIV_TYPE_CHAR | 1024, 0, "widi_set"
	},
	{
		SIOCIWFIRSTPRIV + 0x1F,
		IW_PRIV_TYPE_CHAR | 128, 0, "widi_prob_req"
	},
#endif // CONFIG_INTEL_WIDI

#ifdef CONFIG_MP_INCLUDED
	{ SIOCIWFIRSTPRIV + 0x0E, IW_PRIV_TYPE_CHAR | 1024, 0 , ""},  //set
	{ SIOCIWFIRSTPRIV + 0x0F, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK , ""},//get
/* --- sub-ioctls definitions --- */
#ifdef CONFIG_APPEND_VENDOR_IE_ENABLE
	{ VENDOR_IE_SET, IW_PRIV_TYPE_CHAR | 1024 , 0 , "vendor_ie_set" },
	{ VENDOR_IE_GET, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "vendor_ie_get" },
#endif
		{ MP_START , IW_PRIV_TYPE_CHAR | 1024, 0, "mp_start" }, //set
		{ MP_PHYPARA, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_phypara" },//get
		{ MP_STOP , IW_PRIV_TYPE_CHAR | 1024, 0, "mp_stop" }, //set
		{ MP_CHANNEL , IW_PRIV_TYPE_CHAR | 1024 , IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_channel" },//get
		{ MP_BANDWIDTH , IW_PRIV_TYPE_CHAR | 1024, 0, "mp_bandwidth"}, //set
		{ MP_RATE , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_rate" },//get
		{ MP_RESET_STATS , IW_PRIV_TYPE_CHAR | 1024, 0, "mp_reset_stats"},
		{ MP_QUERY , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK , "mp_query"}, //get
		{ READ_REG , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "read_reg" },
		{ MP_RATE , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_rate" },
		{ READ_RF , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "read_rf" },
		{ MP_PSD , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_psd"},
		{ MP_DUMP, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_dump" },
		{ MP_TXPOWER , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_txpower"},
		{ MP_ANT_TX , IW_PRIV_TYPE_CHAR | 1024,  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_ant_tx"},
		{ MP_ANT_RX , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_ant_rx"},
		{ WRITE_REG , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "write_reg" },
		{ WRITE_RF , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "write_rf" },
		{ MP_CTX , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_ctx"},
		{ MP_ARX , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_arx"},
		{ MP_THER , IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_ther"},
		{ EFUSE_SET, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "efuse_set" },
		{ EFUSE_GET, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "efuse_get" },
		{ MP_PWRTRK , IW_PRIV_TYPE_CHAR | 1024, 0, "mp_pwrtrk"},
		{ MP_QueryDrvStats, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_drvquery" },
		{ MP_IOCTL, IW_PRIV_TYPE_CHAR | 1024, 0, "mp_ioctl"}, // mp_ioctl
		{ MP_SetRFPathSwh, IW_PRIV_TYPE_CHAR | 1024, 0, "mp_setrfpath" },
		{ MP_PwrCtlDM, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_pwrctldm" },
		{ MP_GET_TXPOWER_INX, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_get_txpower" },
		{ MP_GETVER, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_priv_ver" },
		{ MP_MON, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_mon" },
		{ EFUSE_MASK, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "efuse_mask" },
		{ EFUSE_FILE, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "efuse_file" },
		{ MP_TX, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_tx" },
		{ MP_RX, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "mp_rx" },
		{ CTA_TEST, IW_PRIV_TYPE_CHAR | 1024, 0, "cta_test"},
#endif
#ifdef CONFIG_SDIO_INDIRECT_ACCESS
		{ MP_SD_IREAD, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "sd_iread" },
		{ MP_SD_IWRITE, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "sd_iwrite" },
#endif
};

#if WIRELESS_EXT >= 17
static struct iw_statistics *rtw_get_wireless_stats(struct net_device *dev)
{
       _adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	   struct iw_statistics *piwstats=&padapter->iwstats;
	int tmp_level = 0;
	int tmp_qual = 0;
	int tmp_noise = 0;

	if (check_fwstate(&padapter->mlmepriv, _FW_LINKED) != _TRUE)
	{
		piwstats->qual.qual = 0;
		piwstats->qual.level = 0;
		piwstats->qual.noise = 0;
		//DBG_871X("No link  level:%d, qual:%d, noise:%d\n", tmp_level, tmp_qual, tmp_noise);
	}
	else{
		#ifdef CONFIG_SIGNAL_DISPLAY_DBM
		tmp_level = translate_percentage_to_dbm(padapter->recvpriv.signal_strength);
		#else
		#ifdef CONFIG_SIGNAL_SCALE_MAPPING
		tmp_level = padapter->recvpriv.signal_strength;
		#else
		{
			/* Do signal scale mapping when using percentage as the unit of signal strength, since the scale mapping is skipped in odm */

			HAL_DATA_TYPE *pHal = GET_HAL_DATA(padapter);

			tmp_level = (u8)odm_SignalScaleMapping(&pHal->odmpriv, padapter->recvpriv.signal_strength);
		}
		#endif
		#endif

		tmp_qual = padapter->recvpriv.signal_qual;
		rtw_get_noise(padapter);
		tmp_noise = padapter->recvpriv.noise;
		//DBG_871X("level:%d, qual:%d, noise:%d, rssi (%d)\n", tmp_level, tmp_qual, tmp_noise,padapter->recvpriv.rssi);

		piwstats->qual.level = tmp_level;
		piwstats->qual.qual = tmp_qual;
		piwstats->qual.noise = tmp_noise;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14))
	piwstats->qual.updated = IW_QUAL_ALL_UPDATED ;//|IW_QUAL_DBM;
#else
#ifdef RTK_DMP_PLATFORM
	//IW_QUAL_DBM= 0x8, if driver use this flag, wireless extension will show value of dbm.
	//remove this flag for show percentage 0~100
	piwstats->qual.updated = 0x07;
#else
	piwstats->qual.updated = 0x0f;
#endif
#endif

	#ifdef CONFIG_SIGNAL_DISPLAY_DBM
	piwstats->qual.updated = piwstats->qual.updated | IW_QUAL_DBM;
	#endif

	return &padapter->iwstats;
}
#endif

#ifdef CONFIG_WIRELESS_EXT
struct iw_handler_def rtw_handlers_def =
{
	.standard = rtw_handlers,
	.num_standard = sizeof(rtw_handlers) / sizeof(iw_handler),
#if WIRELESS_EXT >= 17
	.get_wireless_stats = rtw_get_wireless_stats,
#endif
};
#endif

// copy from net/wireless/wext.c start
/* ---------------------------------------------------------------- */
/*
 * Calculate size of private arguments
 */
static const char iw_priv_type_size[] = {
	0,                              /* IW_PRIV_TYPE_NONE */
	1,                              /* IW_PRIV_TYPE_BYTE */
	1,                              /* IW_PRIV_TYPE_CHAR */
	0,                              /* Not defined */
	sizeof(__u32),                  /* IW_PRIV_TYPE_INT */
	sizeof(struct iw_freq),         /* IW_PRIV_TYPE_FLOAT */
	sizeof(struct sockaddr),        /* IW_PRIV_TYPE_ADDR */
	0,                              /* Not defined */
};

static int get_priv_size(__u16 args)
{
	int num = args & IW_PRIV_SIZE_MASK;
	int type = (args & IW_PRIV_TYPE_MASK) >> 12;

	return num * iw_priv_type_size[type];
}
// copy from net/wireless/wext.c end


int rtw_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct iwreq *wrq = (struct iwreq *)rq;
	int ret=0;

	switch (cmd)
	{
		case RTL_IOCTL_WPA_SUPPLICANT:
			ret = wpa_supplicant_ioctl(dev, &wrq->u.data);
			break;
#ifdef CONFIG_AP_MODE
		case RTL_IOCTL_HOSTAPD:
			ret = rtw_hostapd_ioctl(dev, &wrq->u.data);
			break;
#ifdef CONFIG_WIRELESS_EXT
		case SIOCSIWMODE:
			ret = rtw_wx_set_mode(dev, NULL, &wrq->u, NULL);
			break;
#endif
#endif // CONFIG_AP_MODE
		default:
			ret = -EOPNOTSUPP;
			break;
	}

	return ret;
}


