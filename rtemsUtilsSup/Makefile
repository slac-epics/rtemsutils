TOP=..
include $(TOP)/configure/CONFIG
#=======================================

# Build as a library for an IOC:
LIBRARY_IOC_RTEMS += rtemsutils

# Library Source files
rtemsutils_SRCS += spy.cpp

# Link with the asyn and base libraries
rtemsutils_LIBS += $(EPICS_BASE_IOC_LIBS)

# Install .dbd and .db files
DBD += spy.dbd

#=======================================
include $(TOP)/configure/RULES
