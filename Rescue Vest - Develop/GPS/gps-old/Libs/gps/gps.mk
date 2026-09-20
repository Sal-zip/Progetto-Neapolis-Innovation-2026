GPSPATH = ./Libs/gps

# List of all the SHT40AD1B device files.
GPSSRC := $(GPSPATH)/gps.c

# Required include directories
GPSINC := $(GPSPATH)

# Shared variables
ALLCSRC += $(GPSSRC)
ALLINC  += $(GPSINC)