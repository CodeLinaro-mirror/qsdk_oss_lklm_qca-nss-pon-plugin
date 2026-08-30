###################################################
# Makefile for the PON plugin
###################################################

obj ?= .

obj-m += qca-nss-pon-plugin.o
qca-nss-pon-plugin-objs += nss_pon_plugin.o

ccflags-y += -I$(obj)/exports
ccflags-y += -Wall -Werror
