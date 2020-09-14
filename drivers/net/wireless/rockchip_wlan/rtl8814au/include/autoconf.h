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
//***** temporarily flag *******
#define CONFIG_SINGLE_IMG
//#define CONFIG_DISABLE_ODM

//***** temporarily flag *******
/*
 * Public  General Config
 */
#define AUTOCONF_INCLUDED
#define RTL871X_MODULE_NAME "8814AU"
#define DRV_NAME "rtl8814au"




#define PLATFORM_LINUX	1


#define CONFIG_IOCTL_CFG80211 1

#ifdef CONFIG_IOCTL_CFG80211
	//#define RTW_USE_CFG80211_STA_EVENT /* Indecate new sta asoc through cfg80211_new_sta */
	#define CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER
	//#define CONFIG_DEBUG_CFG80211
	//#define CONFIG_DRV_ISSUE_PROV_REQ // IOT FOR S2
	#define CONFIG_SET_SCAN_DENY_TIMER
	/*#define SUPPLICANT_RTK_VERSION_LOWER_THAN_JB42*/ /* wpa_supplicant realtek version <= jb42 will be defined this */
#endif

/*
 * Internal  General Config
 */
//#define CONFIG_H2CLBK

#define CONFIG_EMBEDDED_FWIMG	1

//#define CONFIG_XMIT_ACK
#ifdef CONFIG_XMIT_ACK
	#define CONFIG_ACTIVE_KEEP_ALIVE_CHECK
#endif

#define CONFIG_80211N_HT	1

#ifdef CONFIG_80211N_HT
	#define CONFIG_80211AC_VHT 1
	#define CONFIG_BEAMFORMING

	#ifdef CONFIG_BEAMFORMING
		#define CONFIG_PHYDM_BEAMFORMING
		#ifdef CONFIG_PHYDM_BEAMFORMING
		#define BEAMFORMING_SUPPORT			1	/*for phydm beamforming*/
		#define SUPPORT_MU_BF					0
		#else
		#define BEAMFORMING_SUPPORT			0	/*for driver beamforming*/
		#endif
	#endif
#endif

#define CONFIG_RECV_REORDERING_CTRL	1

//#define CONFIG_TCP_CSUM_OFFLOAD_RX	1

#define CONFIG_RF_GAIN_OFFSET

#define CONFIG_DFS	1

//#ifndef CONFIG_MP_INCLUDED
	#define CONFIG_IPS	1
	#ifdef CONFIG_IPS
	//#define CONFIG_IPS_LEVEL_2	1 //enable this to set default IPS mode to IPS_LEVEL_2
	#define CONFIG_IPS_CHECK_IN_WD // Do IPS Check in WatchDog.
	#endif
	//#define SUPPORT_HW_RFOFF_DETECTED	1

	#define CONFIG_LPS	1

	#ifdef CONFIG_LPS_LCLK
	#define CONFIG_XMIT_THREAD_MODE
	#endif

	//befor link
	//#define CONFIG_ANTENNA_DIVERSITY

	//after link

	//#define CONFIG_CONCURRENT_MODE 1
	#ifdef CONFIG_CONCURRENT_MODE
		//#define CONFIG_HWPORT_SWAP				//Port0->Sec , Port1 -> Pri
		#define CONFIG_RUNTIME_PORT_SWITCH
		//#define DBG_RUNTIME_PORT_SWITCH
		#define CONFIG_SCAN_BACKOP
		//#ifdef CONFIG_RTL8812A
		//	#define CONFIG_TSF_RESET_OFFLOAD 1		// For 2 PORT TSF SYNC.
		//#endif
	#endif

//#else 	//#ifndef CONFIG_MP_INCLUDED

//#endif 	//#ifndef CONFIG_MP_INCLUDED

#define CONFIG_AP_MODE	1
#ifdef CONFIG_AP_MODE
	//#define CONFIG_INTERRUPT_BASED_TXBCN // Tx Beacon when driver BCN_OK ,BCN_ERR interrupt occurs
	#if defined(CONFIG_CONCURRENT_MODE) && defined(CONFIG_INTERRUPT_BASED_TXBCN)
		#undef CONFIG_INTERRUPT_BASED_TXBCN
	#endif
	#ifdef CONFIG_INTERRUPT_BASED_TXBCN
		//#define CONFIG_INTERRUPT_BASED_TXBCN_EARLY_INT
		#define CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
	#endif

	#define CONFIG_NATIVEAP_MLME
	#ifndef CONFIG_NATIVEAP_MLME
		#define CONFIG_HOSTAPD_MLME	1
	#endif
	#define CONFIG_FIND_BEST_CHANNEL	1
#endif

#define CONFIG_P2P	1
#ifdef CONFIG_P2P
	//The CONFIG_WFD is for supporting the Wi-Fi display
	#define CONFIG_WFD

	#define CONFIG_P2P_REMOVE_GROUP_INFO

	//#define CONFIG_DBG_P2P

	#define CONFIG_P2P_PS
	//#define CONFIG_P2P_IPS
	#define CONFIG_P2P_OP_CHK_SOCIAL_CH
	#define CONFIG_CFG80211_ONECHANNEL_UNDER_CONCURRENT  //replace CONFIG_P2P_CHK_INVITE_CH_LIST flag
	#define CONFIG_P2P_INVITE_IOT
#endif

#define CONFIG_SKB_COPY	1//for amsdu

#define CONFIG_LED
#ifdef CONFIG_LED
	#define CONFIG_SW_LED
	#ifdef CONFIG_SW_LED
		//#define CONFIG_LED_HANDLED_BY_CMD_THREAD
	#endif
#endif // CONFIG_LED

#define USB_INTERFERENCE_ISSUE // this should be checked in all usb interface
#define CONFIG_GLOBAL_UI_PID

#define CONFIG_LAYER2_ROAMING
#define CONFIG_LAYER2_ROAMING_RESUME
//#define CONFIG_ADAPTOR_INFO_CACHING_FILE // now just applied on 8192cu only, should make it general...
//#define CONFIG_RESUME_IN_WORKQUEUE
//#define CONFIG_SET_SCAN_DENY_TIMER
#define CONFIG_LONG_DELAY_ISSUE
#define CONFIG_NEW_SIGNAL_STAT_PROCESS
//#define CONFIG_SIGNAL_DISPLAY_DBM //display RX signal with dbm
#ifdef CONFIG_SIGNAL_DISPLAY_DBM
//#define CONFIG_BACKGROUND_NOISE_MONITOR
#endif
#define RTW_NOTCH_FILTER 0 /* 0:Disable, 1:Enable, */

#define CONFIG_TX_MCAST2UNI		/*Support IP multicast->unicast*/
//#define CONFIG_CHECK_AC_LIFETIME 1	// Check packet lifetime of 4 ACs.


/*
 * Interface  Related Config
 */

#ifndef CONFIG_MINIMAL_MEMORY_USAGE
	#define CONFIG_USB_TX_AGGREGATION	1
	#define CONFIG_USB_RX_AGGREGATION	1
#endif

//#define CONFIG_REDUCE_USB_TX_INT	1	// Trade-off: Improve performance, but may cause TX URBs blocked by USB Host/Bus driver on few platforms.
//#define CONFIG_EASY_REPLACEMENT	1

/*
 * CONFIG_USE_USB_BUFFER_ALLOC_XX uses Linux USB Buffer alloc API and is for Linux platform only now!
 */
//#define CONFIG_USE_USB_BUFFER_ALLOC_TX 1	// Trade-off: For TX path, improve stability on some platforms, but may cause performance degrade on other platforms.
//#define CONFIG_USE_USB_BUFFER_ALLOC_RX 1	// For RX path
#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX

#else
	#define CONFIG_PREALLOC_RECV_SKB
	#ifdef CONFIG_PREALLOC_RECV_SKB
		//#define CONFIG_FIX_NR_BULKIN_BUFFER /* only use PREALLOC_RECV_SKB buffer, don't alloc skb at runtime */
	#endif
#endif

/*
 * USB VENDOR REQ BUFFER ALLOCATION METHOD
 * if not set we'll use function local variable (stack memory)
 */
//#define CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
#define CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC

#define CONFIG_USB_VENDOR_REQ_MUTEX
#define CONFIG_VENDOR_REQ_RETRY

//#define CONFIG_USB_SUPPORT_ASYNC_VDN_REQ 1

#ifdef CONFIG_GPIO_WAKEUP
	#ifndef WAKEUP_GPIO_IDX
#define WAKEUP_GPIO_IDX	1	//WIFI Chip Side
	#endif // !WAKEUP_GPIO_IDX
#endif // CONFIG_GPIO_WAKEUP

/*
 * HAL  Related Config
 */
#define RTL8812A_RX_PACKET_INCLUDE_CRC	0

#define CONFIG_RX_PACKET_APPEND_FCS

//#define CONFIG_ONLY_ONE_OUT_EP_TO_LOW	0

#define CONFIG_OUT_EP_WIFI_MODE	0

#define ENABLE_USB_DROP_INCORRECT_OUT

#define CONFIG_ADHOC_WORKAROUND_SETTING	1

#define ENABLE_NEW_RFE_TYPE	0

#define DISABLE_BB_RF	0

#ifdef CONFIG_MP_INCLUDED
	#define MP_DRIVER 1
	#define CONFIG_MP_IWPRIV_SUPPORT	1
	//#undef CONFIG_USB_TX_AGGREGATION
	//#undef CONFIG_USB_RX_AGGREGATION
#else
	#define MP_DRIVER 0
#endif


/*
 * Platform  Related Config
 */

	#define BT_30_SUPPORT 0



#ifdef CONFIG_USB_TX_AGGREGATION
//#define 	CONFIG_TX_EARLY_MODE
#endif

#define	RTL8188E_EARLY_MODE_PKT_NUM_10	0

#define CONFIG_80211D

#define CONFIG_ATTEMPT_TO_FIX_AP_BEACON_ERROR

/*
 * Debug Related Config
 */
#define DBG	1

#define CONFIG_DEBUG /* DBG_871X, etc... */
//#define CONFIG_DEBUG_RTL871X /* RT_TRACE, RT_PRINT_DATA, _func_enter_, _func_exit_ */

#define DBG_CONFIG_ERROR_DETECT
//#define DBG_CONFIG_ERROR_DETECT_INT
//#define DBG_CONFIG_ERROR_RESET

//#define DBG_IO
//#define DBG_DELAY_OS
//#define DBG_MEM_ALLOC
//#define DBG_IOCTL

//#define DBG_TX
//#define DBG_XMIT_BUF
//#define DBG_XMIT_BUF_EXT
//#define DBG_TX_DROP_FRAME

//#define DBG_RX_DROP_FRAME
//#define DBG_RX_SEQ
//#define DBG_RX_SIGNAL_DISPLAY_PROCESSING
//#define DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED "jeff-ap"



//#define DBG_SHOW_MCUFWDL_BEFORE_51_ENABLE
//#define DBG_ROAMING_TEST

//#define DBG_HAL_INIT_PROFILING

//#define DBG_MEMORY_LEAK	1
//#define CONFIG_FW_C2H_DEBUG
/*#define	DBG_RX_DFRAME_RAW_DATA*/
