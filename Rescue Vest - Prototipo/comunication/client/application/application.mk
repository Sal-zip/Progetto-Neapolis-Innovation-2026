##############################################################################
# Package Application
##############################################################################

APPLICATIONPATH := ./Application

APPLICATIONSRC := $(APPLICATIONPATH)/src/main.c \
                  $(APPLICATIONPATH)/src/application.c \
                  $(APPLICATIONPATH)/src/telemetry.c \
                  $(APPLICATIONPATH)/src/telemetry_json.c

APPLICATIONINC := $(APPLICATIONPATH)/lib

ALLCSRC += $(APPLICATIONSRC)
ALLINC  += $(APPLICATIONINC)