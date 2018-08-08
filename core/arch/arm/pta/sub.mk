srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += pta_invoke_tests.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += core_self_tests.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += interrupt_tests.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += core_mutex_tests.c
ifeq ($(CFG_WITH_USER_TA),y)
srcs-$(CFG_SECSTOR_TA_MGMT_PTA) += secstor_ta_mgmt.c
srcs-$(CFG_TEE_CORE_EMBED_INTERNAL_TESTS) += core_fs_htree_tests.c
endif
srcs-$(CFG_WITH_STATS) += stats.c
srcs-$(CFG_TA_GPROF_SUPPORT) += gprof.c
srcs-$(CFG_TEE_BENCHMARK) += benchmark.c
srcs-$(CFG_SDP_PTA) += sdp_pta.c
srcs-$(CFG_TA_HELLO_WORLD) += pta_hello_world.c
srcs-$(CFG_CYREP) += pta_cyrep.c
srcs-$(CFG_RPMSG) += pta_rpmsg.c
cflags-pta_rpmsg.c-$(CFG_RPMSG) += -DRL_USE_CUSTOM_CONFIG=1

ifeq ($(CFG_SE_API),y)
srcs-$(CFG_SE_API_SELF_TEST) += se_api_self_tests.c
cppflags-se_api_self_tests.c-y += -Icore/tee/se
endif

srcs-$(CFG_TA_SPI) += pta_spi.c
srcs-$(CFG_TA_RPC) += pta_rpc.c
