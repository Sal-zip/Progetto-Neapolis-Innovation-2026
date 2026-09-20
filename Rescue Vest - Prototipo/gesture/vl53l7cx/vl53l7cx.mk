##############################################################################
# Driver ST VL53L7CX ULD
##############################################################################

VL53L7CXPATH := ./VL53L7CX

# Driver principale, plugin ST e porting ChibiOS/I2C.
VL53L7CXSRC := $(VL53L7CXPATH)/src/vl53l7cx_api.c \
               $(VL53L7CXPATH)/src/vl53l7cx_plugin_detection_thresholds.c \
               $(VL53L7CXPATH)/src/vl53l7cx_plugin_motion_indicator.c \
               $(VL53L7CXPATH)/src/vl53l7cx_plugin_xtalk.c \
               $(VL53L7CXPATH)/platform.c

# Header pubblici ST e interfaccia del porting hardware.
VL53L7CXINC := $(VL53L7CXPATH)/inc \
               $(VL53L7CXPATH)

ALLCSRC += $(VL53L7CXSRC)
ALLINC  += $(VL53L7CXINC)