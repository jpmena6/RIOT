APPLICATION = openthread-estructural-test

# must add this
CFLAGS += -D_APP_ESTRUCTURAL_

# added for app
USEMODULE += periph_rtt

# If no BOARD is found in the environment, use this default:
#BOARD ?= samr21-xpro
BOARD ?= frdm-kw41z
# These are the boards that OpenThread stack has been tested on
BOARD_WHITELIST := samr21-xpro iotlab-m3 fox iotlab-a8-m3

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEPKG += openthread
OPENTHREAD_TYPE ?= ftd
USEMODULE += openthread-$(OPENTHREAD_TYPE)

# Comment the following line in order to disable the OpenThread CLI
#USEMODULE += openthread-cli

#Define PANID, CHANNEL and UART_BAUDRATE used by default
OPENTHREAD_PANID ?= 0x1234
OPENTHREAD_CHANNEL ?= 26

CFLAGS += -DOPENTHREAD_PANID=$(OPENTHREAD_PANID)
CFLAGS += -DOPENTHREAD_CHANNEL=$(OPENTHREAD_CHANNEL)

ifneq (,$(filter samr21-xpro,$(BOARD)))
  DRIVER := at86rf233
endif
ifneq (,$(filter iotlab-m3 fox iotlab-a8-m3,$(BOARD)))
  DRIVER := at86rf231
endif
ifneq (,$(filter frdm-kw41z,$(BOARD)))
  DRIVER := kw41zrf
endif

USEMODULE += $(DRIVER)

USEMODULE += random
USEMODULE += ps

#required for C++ compiling
CXXEXFLAGS += -fno-rtti
USEMODULE += cpp11-compat
FEATURES_REQUIRED += periph_spi
USEMODULE += fmt


include $(RIOTBASE)/Makefile.include
