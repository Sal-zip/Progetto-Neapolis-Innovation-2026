PPGPATH := ./Sensors/PPG

# Driver MAX30102 e servizio/thread PPG.
PPGSRC := $(PPGPATH)/src/ppg.c \
          $(PPGPATH)/src/thread_ppg.c

# Header pubblici del package.
PPGINC := $(PPGPATH)/lib

# Esportazione verso il Makefile principale.
ALLCSRC += $(PPGSRC)
ALLINC  += $(PPGINC)