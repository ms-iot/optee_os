RIOT_ROOT=../../../external/RIoT
CYREP_ROOT=$(RIOT_ROOT)/CyReP

global-incdirs-y += $(CYREP_ROOT)/cyrep
global-incdirs-y += $(CYREP_ROOT)/tcps
global-incdirs-y += ./include

srcs-y += $(CYREP_ROOT)/RiotAes128.c
srcs-y += $(CYREP_ROOT)/RiotBase64.c
srcs-y += $(CYREP_ROOT)/RiotCrypt.c
srcs-y += $(CYREP_ROOT)/RiotDerEnc.c
srcs-y += $(CYREP_ROOT)/RiotEcc.c
srcs-y += $(CYREP_ROOT)/RiotHmac.c
srcs-y += $(CYREP_ROOT)/RiotKdf.c
srcs-y += $(CYREP_ROOT)/RiotSha256.c
srcs-y += $(CYREP_ROOT)/RiotX509Bldr.c
srcs-y += $(CYREP_ROOT)/tcps/TcpsId.c
srcs-y += $(CYREP_ROOT)/tcps/cborhelper.c
srcs-y += cborencoder.c
srcs-y += cborparser.c

cflags-$(CYREP_ROOT)/RiotSha256.c-y += -fno-strict-aliasing
