PLATFORM_FLAVOR ?= mx6ulevk

# Get SoC associated with the PLATFORM_FLAVOR
mx6ul-flavorlist = mx6ulevk
mx6ull-flavorlist = mx6ullevk
mx6q-flavorlist = mx6qsabrelite mx6qsabresd mx6qhmbedge
mx6d-flavorlist = mx6dhmbedge
mx6dl-flavorlist = mx6dlsabresd mx6dlhmbedge
mx6s-flavorlist = mx6shmbedge
mx7-flavorlist = mx7dsabresd

ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx6ul-flavorlist)))
$(call force,CFG_MX6UL,y)
else ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx6ull-flavorlist)))
$(call force,CFG_MX6ULL,y)
else ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx6q-flavorlist)))
$(call force,CFG_MX6Q,y)
else ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx6d-flavorlist)))
$(call force,CFG_MX6D,y)
else ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx6dl-flavorlist)))
$(call force,CFG_MX6DL,y)
else ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx6s-flavorlist)))
$(call force,CFG_MX6S,y)
else ifneq (,$(filter $(PLATFORM_FLAVOR),$(mx7-flavorlist)))
$(call force,CFG_MX7,y)
else
$(error Unsupported PLATFORM_FLAVOR "$(PLATFORM_FLAVOR)")
endif

ifneq (,$(filter $(PLATFORM_FLAVOR),mx7dsabresd))
CFG_DDR_SIZE ?= 0x40000000
CFG_DT ?= y
CFG_NS_ENTRY_ADDR ?= 0x80800000
CFG_PSCI_ARM32 ?= y
CFG_TEE_CORE_NB_CORE ?= 2
endif

# Common i.MX6 config
core_arm32-platform-aflags	+= -mfpu=neon

$(call force,CFG_GENERIC_BOOT,y)
$(call force,CFG_GIC,y)
$(call force,CFG_IMX_UART,y)
$(call force,CFG_PM_STUBS,y)
$(call force,CFG_WITH_SOFTWARE_PRNG,y)

CFG_CRYPTO_SIZE_OPTIMIZATION ?= n
CFG_WITH_STACK_CANARIES ?= y


# i.MX6UL/ULL specific config
ifneq (,$(filter y, $(CFG_MX6UL) $(CFG_MX6ULL)))
include core/arch/arm/cpu/cortex-a7.mk

$(call force,CFG_MX6,y)
$(call force,CFG_SECURE_TIME_SOURCE_REE,y)
endif

# i.MX6Q specific config
ifeq ($(CFG_MX6Q),y)
# CFG_TZC380 is required for systems having the TZASC_ENABLE fuse burnt.
CFG_TZC380 ?= y

# One bit for each SPI controller to reserve for TZ Supervisor execution mode.
# Examples:
# - CFG_TZ_SPI_CONTROLLERS=0x0  -> no reserved controllers
# - CFG_TZ_SPI_CONTROLLERS=0x1  -> reserve ECSPI1
# - CFG_TZ_SPI_CONTROLLERS=0x2  -> reserve ECSPI2
# - CFG_TZ_SPI_CONTROLLERS=0x3  -> reserve ECSPI1 and ECSPI2
# - CFG_TZ_SPI_CONTROLLERS=0x4  -> reserve ECSPI3
# - CFG_TZ_SPI_CONTROLLERS=0x1f -> reserve ECSPI1, 2, 3, 4, and 5
CFG_TZ_SPI_CONTROLLERS ?= 0x0

endif # ifeq ($(CFG_MX6Q),y)

# i.MX6 Solo/DualLite/Dual/Quad specific config
ifeq ($(filter y, $(CFG_MX6Q) $(CFG_MX6D) $(CFG_MX6DL) $(CFG_MX6S)), y)
include core/arch/arm/cpu/cortex-a9.mk

$(call force,CFG_MX6,y)
$(call force,CFG_PL310,y)
$(call force,CFG_PL310_LOCKED,y)
$(call force,CFG_SECURE_TIME_SOURCE_REE,y)

//CFG_BOOT_SYNC_CPU ?= y
//CFG_BOOT_SECONDARY_REQUEST ?= y
endif

ifeq ($(filter y, $(CFG_MX7)), y)
include core/arch/arm/cpu/cortex-a7.mk

$(call force,CFG_SECURE_TIME_SOURCE_REE,y)
CFG_BOOT_SECONDARY_REQUEST ?= y
endif

ifeq ($(PLATFORM_FLAVOR), mx6qhmbedge)
# In the default configuration: 
# - UART3 is used for OPTEE console
# - UART1 is used for Windows kernel debugging
CFG_CONSOLE_UART ?= UART1_BASE

ifeq ($(CFG_TA_SPI),y)
$(call force,CFG_SPI,y)
endif

ifeq ($(CFG_SPI),y)
$(call force,CFG_IMX_SPI,y)
endif

ifeq ($(CFG_IMX_SPI),y)
$(call force,CFG_IMX_IOMUX,y)
$(call force,CFG_IMX_GPIO,y)
$(call force,CFG_IMX_CLOCK,y)
endif 

endif # ifeq ($(PLATFORM_FLAVOR), mx6qhmbedge)

ta-targets = ta_arm32
