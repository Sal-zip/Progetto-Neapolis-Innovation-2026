# Shell PATH
MQ2BPMPATH = ./Libs/mq2bpm

# RT Shell files.
MQ2BPMSRC = $(MQ2BPMPATH)/mq2bpm.c

MQ2BPMINC = $(MQ2BPMPATH)

# Shared variables
ALLCSRC += $(MQ2BPMSRC)
ALLINC  += $(MQ2BPMINC)