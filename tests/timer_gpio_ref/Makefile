include ../Makefile.tests_common

FEATURES_REQUIRED = periph_timer periph_gpio
FEATURES_OPTIONAL = periph_adc
USEMODULE += matstat
USEMODULE += fmt
USEMODULE += ds3234

TEST_ON_CI_WHITELIST += all

include $(RIOTBASE)/Makefile.include

test:
	tests/01-run.py
