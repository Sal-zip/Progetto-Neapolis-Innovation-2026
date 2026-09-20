# Sensors/MQ2/mq2.mk

MQ2PATH := ./Sensors/MQ2

MQ2SRC := $(MQ2PATH)/src/mq2.c \
          $(MQ2PATH)/src/thread_mq2.c

MQ2INC := $(MQ2PATH)/lib

ALLCSRC += $(MQ2SRC)
ALLINC  += $(MQ2INC)