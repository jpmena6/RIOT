MODULE = cpu

DIRS += periph
DIRS += vfs

CFLAGS += -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809

ifneq (,$(filter netdev_tap,$(USEMODULE)))
  DIRS += netdev_tap
endif

ifneq (,$(filter socket_zep,$(USEMODULE)))
  DIRS += socket_zep
endif

ifneq (,$(filter mtd_native,$(USEMODULE)))
  DIRS += mtd
endif

ifneq (,$(filter can_linux,$(USEMODULE)))
  DIRS += can
endif
ifneq (,$(filter trace,$(USEMODULE)))
	DIRS += trace
endif

include $(RIOTBASE)/Makefile.base

INCLUDES = $(NATIVEINCLUDES)
