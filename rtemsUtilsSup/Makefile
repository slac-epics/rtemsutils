TOP=..
include $(TOP)/configure/CONFIG
#=======================================

# =============================================
# Build as a static library for an linking
# in with an external IOC application:
# ============================================
LIBRARY_IOC_RTEMS += rtemsutils

# Library Source files
rtemsutils_SRCS += spy.cpp

# Link with the asyn and base libraries
rtemsutils_LIBS += $(EPICS_BASE_IOC_LIBS)

# Install .dbd and .db files
DBD += spy.dbd
# ===========================================


# ==================================================
# Let's build and OBJ file for loading dynamically
# =================================================
PROD_IOC_RTEMS += rtemsUtilsSup

# rtemsUtilsSup.dbd will be created and installed
rtemsUtilsSup_DBD += spy.dbd

# rtemsUtilsSup_registerRecordDeviceDriver.cpp derives from 
# rtemsUtilsSup.dbd
rtemsUtilsSup_SRCS += rtemsUtilsSup_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
rtemsUtilsSup_SRCS += rtemsUtilsSupMain.cpp

# Compile in the RTEMS Spy utility based on the
# RTEMS Capture Engine
rtemsUtilsSup_SRCS += spy.cpp

# Finally link to the EPICS Base libraries
rtemsUtilsSup_LIBS += $(EPICS_BASE_IOC_LIBS)

#=======================================
include $(TOP)/configure/RULES
