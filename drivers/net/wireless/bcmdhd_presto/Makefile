# bcmdhd
DHDCFLAGS = -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER                     \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP  \
	-DDHDTHREAD -DDHD_GPL -DDHD_SCHED -DDHD_DEBUG -DSDTEST -DBDC -DTOE    \
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DDONGLEOVERLAYS -DBCMDBG               \
	-DCUSTOMER_HW2 -DCUSTOM_OOB_GPIO_NUM=$(CONFIG_BCM4329_OOB_GPIO) -DOOB_INTR_ONLY -DHW_OOB       \
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
	-DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT -DWLBTAMP         \
	-DKEEP_ALIVE -DCSCAN -DGET_CUSTOM_MAC_ENABLE -DPKT_FILTER_SUPPORT     \
	-DEMBEDDED_PLATFORM -DENABLE_INSMOD_NO_FW_LOAD -DPNO_SUPPORT -DDHD_USE_STATIC_BUF          \
	-DSET_RANDOM_MAC_SOFTAP -DWL_CFG80211_STA_EVENT -DCUSTOMER_HW_PT     \
	-Idrivers/net/wireless/bcmdhd_presto -Idrivers/net/wireless/bcmdhd_presto/include

# DHDOFILES = aiutils.o bcmsdh_sdmmc_linux.o dhd_linux.o siutils.o bcmutils.o   \
#	dhd_linux_sched.o bcmwifi.o dhd_sdio.o bcmevent.o dhd_bta.o hndpmu.o  \
#	bcmsdh.o dhd_cdc.o bcmsdh_linux.o dhd_common.o linux_osl.o            \
#	bcmsdh_sdmmc.o dhd_custom_gpio.o sbutils.o wldev_common.o wl_android.o dhd_cfg80211.o
DHDOFILES := aiutils.o bcmsdh_sdmmc_linux.o dhd_linux.o siutils.o bcmutils.o \
	dhd_linux_sched.o dhd_sdio.o bcmevent.o dhd_bta.o hndpmu.o \
	bcmsdh.o dhd_cdc.o bcmsdh_linux.o dhd_common.o linux_osl.o  dhd_cfg80211.o \
	bcmwifi_channels.o bcmsdh_sdmmc.o dhd_custom_gpio.o sbutils.o wl_android.o \
	wl_cfg80211.o wl_cfgp2p.o wldev_common.o wl_linux_mon.o wl_roam.o


# DHDCFLAGS += -DBCMASSERT_LOG

# For p2p connection issue
# DHDCFLAGS += -DWL_CFG80211_GON_COLLISION
# DHDCFLAGS += -DWL_SCB_TIMEOUT=10

# For Passing all multicast packets to host when not in suspend mode.
# DHDCFLAGS += -DPASS_ALL_MCAST_PKTS

# For Scan result patch
DHDCFLAGS += -DESCAN_RESULT_PATCH
DHDCFLAGS += -DDUAL_ESCAN_RESULT_BUFFER

# DHDCFLAGS += -DROAM_ENABLE -DROAM_CHANNEL_CACHE -DROAM_API 
# DHDCFLAGS += -DDISABLE_FW_ROAM_SUSPEND


DHDCFLAGS += -DWL_CFG80211

# SoftAP
# DHDCFLAGS += -DSUPPORT_AUTO_CHANNEL -DSUPPORT_HIDDEN_AP
# DHDCFLAGS += -DSUPPORT_SOFTAP_SINGL_DISASSOC 
# DHDCFLAGS += -DUSE_STAMAC_4SOFTAP 

# DPC priority
# DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=98

# WiFi turn off delay
# DHDCFLAGS += -DWIFI_TURNOFF_DELAY=100

# DTIM listen interval in suspend mode(0 means follow AP's DTIM period)
# DHDCFLAGS += -DCUSTOM_SUSPEND_BCN_LI_DTIM=0


# Ioctl timeout 5000ms
# DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000

# WiFi Kernel thread type
# DHDCFLAGS += -DUSE_KTHREAD_API

############
# JellyBean 
############
# DHDCFLAGS += -DWL_ENABLE_P2P_IF
# DHDCFLAGS += -DMULTIPLE_SUPPLICANT
# DHDCFLAGS += -DWL_CFG80211_STA_EVENT

	
obj-$(CONFIG_BCMDHD) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)
ifneq ($(CONFIG_WIRELESS_EXT),)
bcmdhd-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP -DWL_WIRELESS_EXT -DCONFIG_WIRELESS_EXT
endif

ifeq ($(CONFIG_WIFI_CONTROL_FUNC),y)
DHDCFLAGS +=  -DCUSTOMER_HW2 -DCONFIG_MACH_MAHIMAHI
ifeq ($(CONFIG_WIFI_MEM_PREALLOC),y)
DHDCFLAGS += -DDHD_USE_STATIC_BUF
endif
endif
ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += wl_cfg80211.o wl_cfgp2p.o wl_linux_mon.o
DHDCFLAGS += -DWL_CFG80211
DHDCFLAGS += -DCUSTOM_ROAM_TRIGGER_SETTING=-65
DHDCFLAGS += -DCUSTOM_ROAM_DELTA_SETTING=15
endif
ifneq ($(CONFIG_DHD_USE_SCHED_SCAN),)
DHDCFLAGS += -DWL_SCHED_SCAN
endif
ifneq ($(CONFIG_DHD_ENABLE_P2P),)
DHDCFLAGS += -DWL_ENABLE_P2P_IF
endif
EXTRA_CFLAGS = $(DHDCFLAGS)
ifeq ($(CONFIG_BCMDHD),m)
EXTRA_LDFLAGS += --strip-debug
endif
