#
# SHARP configuration
#

obj-$(CONFIG_SHARP_SHLOG) += shrlog.o

obj-$(CONFIG_SHARP_PNP) += pnp/

obj-$(CONFIG_SHARP_BOOT) += shboot/

# for SHARP_PANIC_ON_PWRKEY
EXTRA_CFLAGS += -DSVERSION_OF_SOFT=\"$(SH_BUILD_ID)\"

