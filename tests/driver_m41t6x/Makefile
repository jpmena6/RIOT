APPLICATION = driver_m41t6x
include ../Makefile.tests_common

FEATURES_REQUIRED = periph_i2c

USEMODULE += m41t6x
USEMODULE += xtimer

# set default device parameters in case they are undefined
TEST_M41T6X_I2C  ?= I2C_0
TEST_M41T6X_ADDR ?= 0x68
TEST_M41T6X_IRQ ?= GPIO_UNDEF

# export parameters
CFLAGS += -DTEST_M41T6X_I2C=$(TEST_M41T6X_I2C)
CFLAGS += -DTEST_M41T6X_ADDR=$(TEST_M41T6X_ADDR)
CFLAGS += -DTEST_M41T6X_IRQ=$(TEST_M41T6X_IRQ)

include $(RIOTBASE)/Makefile.include
