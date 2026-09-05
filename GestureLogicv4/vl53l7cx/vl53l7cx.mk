# lsm6dso PATH
VL53L7CXPATH = ./vl53l7cx

# List of all the LSM6DO device files.
VL53L7CXSRC := $(VL53L7CXPATH)/src/vl53l7cx_api.c \
			   $(VL53L7CXPATH)/src/vl53l7cx_plugin_detection_thresholds.c \
			   $(VL53L7CXPATH)/src/vl53l7cx_plugin_motion_indicator.c \
			   $(VL53L7CXPATH)/src/vl53l7cx_plugin_xtalk.c \
			   $(VL53L7CXPATH)/platform.c
			 

# Required include directories
VL53L7CXINC := $(VL53L7CXPATH)/inc \
               $(VL53L7CXPATH)

# Shared variables
ALLCSRC += $(VL53L7CXSRC)
ALLINC  += $(VL53L7CXINC)