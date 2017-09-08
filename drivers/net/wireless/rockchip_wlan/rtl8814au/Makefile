EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
EXTRA_CFLAGS += -O1
#EXTRA_CFLAGS += -O3
#EXTRA_CFLAGS += -Wall
#EXTRA_CFLAGS += -Wextra
#EXTRA_CFLAGS += -Werror
#EXTRA_CFLAGS += -pedantic
#EXTRA_CFLAGS += -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes

EXTRA_CFLAGS += -Wno-unused-variable
EXTRA_CFLAGS += -Wno-unused-value
EXTRA_CFLAGS += -Wno-unused-label
EXTRA_CFLAGS += -Wno-unused-parameter
EXTRA_CFLAGS += -Wno-unused-function
EXTRA_CFLAGS += -Wno-unused
#EXTRA_CFLAGS += -Wno-uninitialized
EXTRA_CFLAGS += -Wno-error=date-time	# Fix compile error on gcc 4.9 and later

EXTRA_CFLAGS += -I$(src)/include
EXTRA_CFLAGS += -I$(src)/hal/phydm

EXTRA_LDFLAGS += --strip-debug

########################## WIFI IC ############################
CONFIG_RTL8814A = y
######################### Interface ###########################
########################## Features ###########################
CONFIG_MP_INCLUDED = y
CONFIG_POWER_SAVING = y
CONFIG_USB_AUTOSUSPEND = n
CONFIG_HW_PWRP_DETECTION = n
CONFIG_WIFI_TEST = n
CONFIG_INTEL_WIDI = n
CONFIG_WAPI_SUPPORT = n
CONFIG_EFUSE_CONFIG_FILE = n
CONFIG_EXT_CLK = n
CONFIG_TRAFFIC_PROTECT = y
CONFIG_LOAD_PHY_PARA_FROM_FILE = y
CONFIG_CALIBRATE_TX_POWER_BY_REGULATORY = n
CONFIG_CALIBRATE_TX_POWER_TO_MAX = n
CONFIG_RTW_ADAPTIVITY_EN = disable
CONFIG_RTW_ADAPTIVITY_MODE = normal
CONFIG_SIGNAL_SCALE_MAPPING = n
CONFIG_80211W = n
CONFIG_REDUCE_TX_CPU_LOADING = n
CONFIG_BR_EXT = y
CONFIG_WIFI_MONITOR = n
CONFIG_APPEND_VENDOR_IE_ENABLE = n
######################## Wake On Lan ##########################
CONFIG_GPIO_WAKEUP = n
CONFIG_WAKEUP_GPIO_IDX = default
CONFIG_HIGH_ACTIVE = n
CONFIG_PNO_SUPPORT = n
CONFIG_PNO_SET_DEBUG = n
######### Notify SDIO Host Keep Power During Syspend ##########
CONFIG_RTW_SDIO_PM_KEEP_POWER = y
###################### MP HW TX MODE FOR VHT #######################
CONFIG_MP_VHT_HW_TX_MODE = n
###################### Platform Related #######################
CONFIG_PLATFORM_I386_PC = y
###############################################################

CONFIG_DRVEXT_MODULE = n

export TopDIR ?= $(shell pwd)

########### COMMON  #################################
_OS_INTFS_FILES :=	os_dep/osdep_service.o \
			os_dep/linux/os_intfs.o \
			os_dep/linux/usb_intf.o \
			os_dep/linux/usb_ops_linux.o \
			os_dep/linux/ioctl_linux.o \
			os_dep/linux/xmit_linux.o \
			os_dep/linux/mlme_linux.o \
			os_dep/linux/recv_linux.o \
			os_dep/linux/ioctl_cfg80211.o \
			os_dep/linux/rtw_cfgvendor.o \
			os_dep/linux/wifi_regd.o \
			os_dep/linux/rtw_android.o \

ifeq ($(CONFIG_MP_INCLUDED), y)
_OS_INTFS_FILES += os_dep/linux/ioctl_mp.o
endif

_HAL_INTFS_FILES :=	hal/hal_intf.o \
			hal/hal_com.o \
			hal/hal_com_phycfg.o \
			hal/hal_phy.o \
			hal/hal_dm.o \
			hal/hal_btcoex.o \
			hal/hal_mp.o \
			hal/hal_hci/hal_usb.o \
			hal/led/hal_usb_led.o


_OUTSRC_FILES := hal/phydm/phydm_debug.o	\
		hal/phydm/phydm_antdiv.o\
		hal/phydm/phydm_antdect.o\
		hal/phydm/phydm_interface.o\
		hal/phydm/phydm_hwconfig.o\
		hal/phydm/phydm.o\
		hal/phydm/halphyrf_ce.o\
		hal/phydm/phydm_edcaturbocheck.o\
		hal/phydm/phydm_dig.o\
		hal/phydm/phydm_pathdiv.o\
		hal/phydm/phydm_rainfo.o\
		hal/phydm/phydm_dynamicbbpowersaving.o\
		hal/phydm/phydm_powertracking_ce.o\
		hal/phydm/phydm_dynamictxpower.o\
		hal/phydm/phydm_adaptivity.o\
		hal/phydm/phydm_cfotracking.o\
		hal/phydm/phydm_noisemonitor.o\
		hal/phydm/phydm_acs.o\
		hal/phydm/phydm_beamforming.o\
		hal/phydm/txbf/halcomtxbf.o\
		hal/phydm/txbf/haltxbfinterface.o


EXTRA_CFLAGS += -I$(src)/platform

########### HAL_RTL8814A #################################
ifeq ($(CONFIG_RTL8814A), y)
## ADD NEW VHT MP HW TX MODE ##
#EXTRA_CFLAGS += -DCONFIG_MP_VHT_HW_TX_MODE
#CONFIG_MP_VHT_HW_TX_MODE = y
##########################################
MODULE_NAME = rtl8814au

EXTRA_CFLAGS += -DCONFIG_RTL8814A

_HAL_INTFS_FILES +=  hal/HalPwrSeqCmd.o \
					hal/rtl8814a/Hal8814PwrSeq.o \
					hal/rtl8814a/rtl8814a_xmit.o\
					hal/rtl8814a/rtl8814a_sreset.o

_HAL_INTFS_FILES +=	hal/rtl8814a/rtl8814a_hal_init.o \
			hal/rtl8814a/rtl8814a_phycfg.o \
			hal/rtl8814a/rtl8814a_rf6052.o \
			hal/rtl8814a/rtl8814a_dm.o \
			hal/rtl8814a/rtl8814a_rxdesc.o \
			hal/rtl8814a/rtl8814a_cmd.o \


_HAL_INTFS_FILES +=	\
			hal/rtl8814a/usb/usb_halinit.o \
			hal/rtl8814a/usb/rtl8814au_led.o \
			hal/rtl8814a/usb/rtl8814au_xmit.o \
			hal/rtl8814a/usb/rtl8814au_recv.o

_HAL_INTFS_FILES += hal/rtl8814a/usb/usb_ops_linux.o

_HAL_INTFS_FILES +=hal/efuse/rtl8814a/HalEfuseMask8814A_USB.o

_OUTSRC_FILES += hal/phydm/rtl8814a/halhwimg8814a_bb.o\
								hal/phydm/rtl8814a/halhwimg8814a_mac.o\
								hal/phydm/rtl8814a/halhwimg8814a_rf.o\
								hal/phydm/rtl8814a/halhwimg8814a_fw.o\
								hal/phydm/rtl8814a/phydm_iqk_8814a.o\
								hal/phydm/rtl8814a/phydm_regconfig8814a.o\
								hal/phydm/rtl8814a/halphyrf_8814a_ce.o\
								hal/phydm/rtl8814a/phydm_rtl8814a.o\
								hal/phydm/txbf/haltxbf8814a.o

endif

########### END OF PATH  #################################


ifeq ($(CONFIG_USB_AUTOSUSPEND), y)
EXTRA_CFLAGS += -DCONFIG_USB_AUTOSUSPEND
endif

ifeq ($(CONFIG_MP_INCLUDED), y)
#MODULE_NAME := $(MODULE_NAME)_mp
EXTRA_CFLAGS += -DCONFIG_MP_INCLUDED
endif

ifeq ($(CONFIG_POWER_SAVING), y)
EXTRA_CFLAGS += -DCONFIG_POWER_SAVING
endif

ifeq ($(CONFIG_HW_PWRP_DETECTION), y)
EXTRA_CFLAGS += -DCONFIG_HW_PWRP_DETECTION
endif

ifeq ($(CONFIG_WIFI_TEST), y)
EXTRA_CFLAGS += -DCONFIG_WIFI_TEST
endif

ifeq ($(CONFIG_INTEL_WIDI), y)
EXTRA_CFLAGS += -DCONFIG_INTEL_WIDI
endif

ifeq ($(CONFIG_WAPI_SUPPORT), y)
EXTRA_CFLAGS += -DCONFIG_WAPI_SUPPORT
endif


ifeq ($(CONFIG_EFUSE_CONFIG_FILE), y)
EXTRA_CFLAGS += -DCONFIG_EFUSE_CONFIG_FILE

#EFUSE_MAP_PATH
USER_EFUSE_MAP_PATH ?=
ifneq ($(USER_EFUSE_MAP_PATH),)
EXTRA_CFLAGS += -DEFUSE_MAP_PATH=\"$(USER_EFUSE_MAP_PATH)\"
else ifeq ($(MODULE_NAME), 8189es)
EXTRA_CFLAGS += -DEFUSE_MAP_PATH=\"/system/etc/wifi/wifi_efuse_8189e.map\"
else ifeq ($(MODULE_NAME), 8723bs)
EXTRA_CFLAGS += -DEFUSE_MAP_PATH=\"/system/etc/wifi/wifi_efuse_8723bs.map\"
else
EXTRA_CFLAGS += -DEFUSE_MAP_PATH=\"/system/etc/wifi/wifi_efuse_$(MODULE_NAME).map\"
endif

#WIFIMAC_PATH
USER_WIFIMAC_PATH ?=
ifneq ($(USER_WIFIMAC_PATH),)
EXTRA_CFLAGS += -DWIFIMAC_PATH=\"$(USER_WIFIMAC_PATH)\"
else
EXTRA_CFLAGS += -DWIFIMAC_PATH=\"/data/wifimac.txt\"
endif

endif

ifeq ($(CONFIG_EXT_CLK), y)
EXTRA_CFLAGS += -DCONFIG_EXT_CLK
endif

ifeq ($(CONFIG_TRAFFIC_PROTECT), y)
EXTRA_CFLAGS += -DCONFIG_TRAFFIC_PROTECT
endif

ifeq ($(CONFIG_LOAD_PHY_PARA_FROM_FILE), y)
EXTRA_CFLAGS += -DCONFIG_LOAD_PHY_PARA_FROM_FILE
#EXTRA_CFLAGS += -DREALTEK_CONFIG_PATH=\"/lib/firmware/\"
EXTRA_CFLAGS += -DREALTEK_CONFIG_PATH=\"\"
endif

ifeq ($(CONFIG_CALIBRATE_TX_POWER_BY_REGULATORY), y)
EXTRA_CFLAGS += -DCONFIG_CALIBRATE_TX_POWER_BY_REGULATORY
endif

ifeq ($(CONFIG_CALIBRATE_TX_POWER_TO_MAX), y)
EXTRA_CFLAGS += -DCONFIG_CALIBRATE_TX_POWER_TO_MAX
endif

ifeq ($(CONFIG_RTW_ADAPTIVITY_EN), disable)
EXTRA_CFLAGS += -DCONFIG_RTW_ADAPTIVITY_EN=0
else ifeq ($(CONFIG_RTW_ADAPTIVITY_EN), enable)
EXTRA_CFLAGS += -DCONFIG_RTW_ADAPTIVITY_EN=1
endif

ifeq ($(CONFIG_RTW_ADAPTIVITY_MODE), normal)
EXTRA_CFLAGS += -DCONFIG_RTW_ADAPTIVITY_MODE=0
else ifeq ($(CONFIG_RTW_ADAPTIVITY_MODE), carrier_sense)
EXTRA_CFLAGS += -DCONFIG_RTW_ADAPTIVITY_MODE=1
endif

ifeq ($(CONFIG_SIGNAL_SCALE_MAPPING), y)
EXTRA_CFLAGS += -DCONFIG_SIGNAL_SCALE_MAPPING
endif

ifeq ($(CONFIG_80211W), y)
EXTRA_CFLAGS += -DCONFIG_IEEE80211W
endif

ifeq ($(CONFIG_PNO_SUPPORT), y)
EXTRA_CFLAGS += -DCONFIG_PNO_SUPPORT
ifeq ($(CONFIG_PNO_SET_DEBUG), y)
EXTRA_CFLAGS += -DCONFIG_PNO_SET_DEBUG
endif
endif

ifeq ($(CONFIG_GPIO_WAKEUP), y)
EXTRA_CFLAGS += -DCONFIG_GPIO_WAKEUP
ifeq ($(CONFIG_HIGH_ACTIVE), y)
EXTRA_CFLAGS += -DHIGH_ACTIVE=1
else
EXTRA_CFLAGS += -DHIGH_ACTIVE=0
endif
endif

ifneq ($(CONFIG_WAKEUP_GPIO_IDX), default)
EXTRA_CFLAGS += -DWAKEUP_GPIO_IDX=$(CONFIG_WAKEUP_GPIO_IDX)
endif

ifeq ($(CONFIG_RTW_SDIO_PM_KEEP_POWER), y)
endif

ifeq ($(CONFIG_REDUCE_TX_CPU_LOADING), y)
EXTRA_CFLAGS += -DCONFIG_REDUCE_TX_CPU_LOADING
endif

ifeq ($(CONFIG_BR_EXT), y)
BR_NAME = br0
EXTRA_CFLAGS += -DCONFIG_BR_EXT
EXTRA_CFLAGS += '-DCONFIG_BR_EXT_BRNAME="'$(BR_NAME)'"'
endif

ifeq ($(CONFIG_WIFI_MONITOR), y)
EXTRA_CFLAGS += -DCONFIG_WIFI_MONITOR
endif

ifeq ($(CONFIG_MP_VHT_HW_TX_MODE), y)
EXTRA_CFLAGS += -DCONFIG_MP_VHT_HW_TX_MODE
ifeq ($(CONFIG_PLATFORM_I386_PC), y)
## For I386 X86 ToolChain use Hardware FLOATING
EXTRA_CFLAGS += -mhard-float
else
## For ARM ToolChain use Hardware FLOATING
EXTRA_CFLAGS += -mfloat-abi=hard
endif
endif

ifeq ($(CONFIG_APPEND_VENDOR_IE_ENABLE), y)
EXTRA_CFLAGS += -DCONFIG_APPEND_VENDOR_IE_ENABLE
endif

# ULLI : as defined in hal/phydm/phydm.h this is ODM_CE
EXTRA_CFLAGS += -DDM_ODM_SUPPORT_TYPE=0x04

ifeq ($(CONFIG_PLATFORM_I386_PC), y)
EXTRA_CFLAGS += -DCONFIG_LITTLE_ENDIAN
EXTRA_CFLAGS += -DCONFIG_IOCTL_CFG80211 -DRTW_USE_CFG80211_STA_EVENT
SUBARCH := $(shell uname -m | sed -e s/i.86/i386/)
ARCH ?= $(SUBARCH)
CROSS_COMPILE ?=
KVER  := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/
INSTALL_PREFIX :=
endif

ifneq ($(KERNELRELEASE),)

rtk_core :=	core/rtw_cmd.o \
		core/rtw_security.o \
		core/rtw_debug.o \
		core/rtw_io.o \
		core/rtw_ioctl_query.o \
		core/rtw_ioctl_set.o \
		core/rtw_ieee80211.o \
		core/rtw_mlme.o \
		core/rtw_mlme_ext.o \
		core/rtw_wlan_util.o \
		core/rtw_vht.o \
		core/rtw_pwrctrl.o \
		core/rtw_rf.o \
		core/rtw_recv.o \
		core/rtw_sta_mgt.o \
		core/rtw_ap.o \
		core/rtw_xmit.o	\
		core/rtw_p2p.o \
		core/rtw_tdls.o \
		core/rtw_br_ext.o \
		core/rtw_iol.o \
		core/rtw_sreset.o \
		core/rtw_btcoex.o \
		core/rtw_beamforming.o \
		core/rtw_odm.o \
		core/efuse/rtw_efuse.o

$(MODULE_NAME)-y += $(rtk_core)

$(MODULE_NAME)-$(CONFIG_INTEL_WIDI) += core/rtw_intel_widi.o

$(MODULE_NAME)-$(CONFIG_WAPI_SUPPORT) += core/rtw_wapi.o	\
					core/rtw_wapi_sms4.o

$(MODULE_NAME)-y += $(_OS_INTFS_FILES)
$(MODULE_NAME)-y += $(_HAL_INTFS_FILES)
$(MODULE_NAME)-y += $(_OUTSRC_FILES)

$(MODULE_NAME)-$(CONFIG_MP_INCLUDED) += core/rtw_mp.o \
					core/rtw_mp_ioctl.o

obj-$(CONFIG_RTL8814AU) := $(MODULE_NAME).o

else

export CONFIG_RTL8814AU = m

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd)  modules

installfw:
	mkdir -p /lib/firmware/rtlwifi
	cp -n firmware/* /lib/firmware/rtlwifi/.

.PHONY: modules clean

clean:
	cd hal/phydm/ ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko
	cd hal/phydm/ ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd hal/led ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd hal ; rm -fr */*/*.mod.c */*/*.mod */*/*.o */*/.*.cmd */*/*.ko
	cd hal ; rm -fr */*.mod.c */*.mod */*.o */.*.cmd */*.ko
	cd hal ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd core/efuse ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd core ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd os_dep/linux ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd os_dep ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	cd platform ; rm -fr *.mod.c *.mod *.o .*.cmd *.ko
	rm -fr Module.symvers ; rm -fr Module.markers ; rm -fr modules.order
	rm -fr *.mod.c *.mod *.o .*.cmd *.ko *~
	rm -fr .tmp_versions

help:
	@echo "options :"
	@echo "modules		build this module"
	@echo "installfw	install firmware"
	@echo "clean		clean"
	@echo "RTLWIFI=y	use RTLWIFI from linux kernel not complete !!"

endif

