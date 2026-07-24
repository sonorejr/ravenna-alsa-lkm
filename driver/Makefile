#####################################################################################################
# Makefile for the Linux Kernel Module for Merging Ravenna ALSA driver
# This will build MergingRavennaALSA.ko
#####################################################################################################

obj-m := MergingRavennaALSA.o
MergingRavennaALSA-objs := c_wrapper_lib.o audio_driver.o manager.o module_main.o PTP.o module_interface.o module_netlink.o module_timer.o EtherTubeNetfilter.o RTP_streams_manager.o RTP_audio_stream.o RTP_stream.o RTP_stream_info.o MTAL_EthUtils.o MTAL_LKernelAPI.o MTConvert.o

# Opt-in driver diagnostics:  make modules MTAL_DP_ENABLE=1
# Turns MTAL_DP (see MTAL_DP.h) back on for the translation units whose failure
# messages are actually diagnostic -- CRTP_audio_stream::Init's codec/engine-format
# rejections and the manager/stream-manager state. Deliberately NOT global: MTAL_DP
# has been a no-op for years, so several other files' debug calls have BIT-ROTTED and
# only fail to compile once the macro emits code again (RTP_audio_stream.c referenced
# a commented-out pEth_netfilter; MTAL_EthUtils.c's AppleMIDI prints name
# TAppleMIDI_PacketBase members that no longer exist). Keep OFF for production images:
# MTAL_DP sits in packet-rate paths.
# MTAL_DP_ENABLE=1 -> CONFIG/VALIDATION paths only. These fire on stream add/remove
# (rare), so they are safe even on the thin i.MX6. RTP_stream_info.o is essential:
# is_valid() is where a bad stream config is rejected, and add_RTP_stream_() returns
# on it with NO message of its own -- that silent path cost a full debug round.
ifdef MTAL_DP_ENABLE
CFLAGS_manager.o             += -DMTAL_DP_ENABLE
CFLAGS_RTP_streams_manager.o += -DMTAL_DP_ENABLE
CFLAGS_RTP_stream_info.o     += -DMTAL_DP_ENABLE
# Safe to include now: the per-packet calls in this file were moved to MTAL_DP_PKT
# (see MTAL_DP.h), so -DMTAL_DP_ENABLE here enables ONLY the low-rate diagnostics --
# above all CRTP_audio_stream::Init's codec / engine-format rejection messages, which
# are the ones that say WHY a sink or source was refused.
CFLAGS_RTP_audio_stream.o    += -DMTAL_DP_ENABLE
endif

# MTAL_DP_ENABLE_PKT=1 -> ALSO the per-PACKET path (MTAL_DP_PKT). These fire ~2-3k
# times/sec on a DSD stream; enabling them wedged a source box via printk storm.
# Host-side debugging ONLY -- never on a box that will receive a live stream.
ifdef MTAL_DP_ENABLE_PKT
CFLAGS_RTP_audio_stream.o    += -DMTAL_DP_ENABLE_PKT
endif

MAKE = make
KERNELRELEASE = $(shell uname -r)
KSRC = /lib/modules/$(KERNELRELEASE)/build/
SRC = $(shell pwd)

modules:
	$(MAKE) -C $(KSRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KSRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KSRC) M=$(SRC) clean

