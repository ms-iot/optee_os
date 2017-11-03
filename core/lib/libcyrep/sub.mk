global-incdirs-y += include

srcs-y += Cyrep.c
srcs-y += RiotAes128.c
srcs-y += RiotBase64.c
srcs-y += RiotCrypt.c
srcs-y += RiotDerEnc.c
srcs-y += RiotEcc.c
srcs-y += RiotHmac.c
srcs-y += RiotKdf.c
srcs-y += RiotSha256.c
srcs-y += RiotX509Bldr.c

cflags-RiotSha256.c-y += -fno-strict-aliasing
cflags-y += -DCONFIG_CYREP_OPTEE_BUILD=1
