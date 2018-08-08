global-incdirs-y += include

srcs-y += llist.c
srcs-y += rpmsg_env.c
srcs-y += rpmsg_lite.c
srcs-y += rpmsg_ns.c
srcs-y += rpmsg_platform.c
srcs-y += rpmsg_queue.c
srcs-y += virtqueue.c

cflags-y += -DRL_USE_CUSTOM_CONFIG=1 -Wno-unused-parameter
cflags-rpmsg_lite.c-y += -fno-strict-aliasing -Wno-unused-but-set-variable
cflags-virtqueue.c-y += -fno-strict-aliasing