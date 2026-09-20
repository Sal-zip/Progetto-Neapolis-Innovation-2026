##############################################################################
# Gesture Board - Application module
##############################################################################

ifndef GESTURE_APPLICATION_MK_INCLUDED
GESTURE_APPLICATION_MK_INCLUDED := yes

GESTUREAPPDIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

GESTUREAPPSRC := \
    $(GESTUREAPPDIR)/src/gesture_application.c

GESTUREAPPINC := \
    $(GESTUREAPPDIR)/lib

ALLCSRC += $(GESTUREAPPSRC)
ALLINC  += $(GESTUREAPPINC)

endif