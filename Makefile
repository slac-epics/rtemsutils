# Makefile for Asyn spy support
#
# Created by norume on Mon Mar 27 09:37:53 2006
# Based on the Asyn top template

TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure
DIRS += $(wildcard *[Ss]up)
DIRS += $(wildcard *[Aa]pp)
DIRS += $(wildcard ioc[Bb]oot)

include $(TOP)/configure/RULES_TOP
