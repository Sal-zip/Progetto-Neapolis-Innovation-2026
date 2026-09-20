GPSPATH := ./Sensors/GPS

GPSSRC := $(GPSPATH)/src/gps.c \
          $(GPSPATH)/src/thread_gps.c

GPSINC := $(GPSPATH)/lib

ALLCSRC += $(GPSSRC)
ALLINC  += $(GPSINC)