##############################################################################
# Build global options
# NOTE: Can be overridden externally.
##############################################################################

ifeq ($(USE_OPT),)
  USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16
endif

ifeq ($(USE_COPT),)
  USE_COPT =
endif

ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

ifeq ($(USE_LDOPT),)
  USE_LDOPT =
endif

ifeq ($(USE_LTO),)
  USE_LTO = no
endif

ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

##############################################################################
# Architecture-specific options
##############################################################################

ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

ifeq ($(USE_FPU),)
  USE_FPU = no
endif

ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv4-sp-d16
endif

##############################################################################
# Project, target and paths
##############################################################################

PROJECT = sos_client

MCU = cortex-m4

#
# Può essere sovrascritto da ChibiStudio o da riga di comando:
#
# make CHIBIOS=C:/ChibiStudio/chibios2111
#
CHIBIOS ?= ../../chibios2111

CONFDIR  := ./ChibiOS/cfg
BUILDDIR := ./build
DEPDIR   := ./.dep

##############################################################################
# ChibiOS components
##############################################################################

# Licensing.
include $(CHIBIOS)/os/license/license.mk

# Startup STM32G4.
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32g4xx.mk

# HAL e piattaforma STM32G4.
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32G4xx/platform.mk
include $(CHIBIOS)/os/hal/boards/ST_NUCLEO64_G474RE/board.mk
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk

# ChibiOS/RT.
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M-ALT/compilers/GCC/mk/port.mk

# Selezione automatica dei moduli abilitati.
include $(CHIBIOS)/tools/mk/autobuild.mk

# Stream e chprintf.
include $(CHIBIOS)/os/hal/lib/streams/streams.mk

##############################################################################
# Sensor packages
##############################################################################

#
# Ogni package aggiunge autonomamente i propri sorgenti ad ALLCSRC
# e le directory degli header ad ALLINC.
#
include ./Sensors/GPS/gps.mk
include ./Sensors/PPG/ppg.mk
include ./Sensors/MQ2/mq2.mk

##############################################################################
# Gesture communication package
##############################################################################

#
# La board principale riceve i comandi dalla Gesture Board.
#
# Il modulo condiviso si trova attualmente dentro:
#
#   Gesture-Board/Communication
#
# Il relativo gesture_link.mk aggiunge automaticamente:
#
#   gesture_protocol.c
#   gesture_link_rx.c
#
# ad ALLCSRC.
#
GESTURE_LINK_ROLE := RX
include ./Gesture-Board/Communication/gesture_link.mk

##############################################################################
# Application package
##############################################################################

#
# Contiene:
#
#   Application/src/main.c
#   Application/src/application.c
#   Application/src/telemetry.c
#   Application/src/telemetry_json.c
#
# Application/src/main.c è l'unico entry point del firmware principale.
#
include ./Application/application.mk

##############################################################################
# ESP8266 and MQTT communication
##############################################################################

#
# Sono inclusi soltanto i due moduli utilizzati dalla nuova Application.
#
# I vecchi file:
#
#   Communication/main.c
#   Communication/src/sos_client.c
#   Communication/src/sos_outbox.c
#   Communication/src/sos_request.c
#
# appartengono alla precedente implementazione e non devono essere compilati
# insieme alla nuova logica applicativa.
#
COMMUNICATIONSRC := \
    ./Communication/src/esp8266_transport.c \
    ./Communication/src/mqtt_client.c

COMMUNICATIONINC := \
    ./Communication/lib

##############################################################################
# Linker script
##############################################################################

LDSCRIPT = $(STARTUPLD)/STM32G474xE.ld

##############################################################################
# C sources
##############################################################################

#
# ALLCSRC contiene:
#
#   - sorgenti ChibiOS/HAL;
#   - GPS;
#   - PPG;
#   - MQ2;
#   - Application;
#   - protocollo e ricevitore GestureLink.
#
CSRC = $(ALLCSRC) \
       $(TESTSRC) \
       $(COMMUNICATIONSRC)

##############################################################################
# C++ and assembly sources
##############################################################################

CPPSRC = $(ALLCPPSRC)

ASMSRC = $(ALLASMSRC)

ASMXSRC = $(ALLXASMSRC)

##############################################################################
# Include directories
##############################################################################

INCDIR = $(CONFDIR) \
         $(ALLINC) \
         $(TESTINC) \
         $(COMMUNICATIONINC)

##############################################################################
# Warning options
##############################################################################

CWARN = -Wall \
        -Wextra \
        -Wundef \
        -Wstrict-prototypes \
        -Wcast-align=strict

CPPWARN = -Wall \
          -Wextra \
          -Wundef

##############################################################################
# User section
##############################################################################

#
# Il JSON usa rappresentazioni intere:
#
#   latitudeE7
#   longitudeE7
#   speedMilliKnots
#   headingMilliDegrees
#
# Non viene usato %f nei messaggi chprintf.
#
UDEFS =

UADEFS =

UINCDIR =

ULIBDIR =

#
# Il codice corrente non richiama direttamente funzioni della libreria
# matematica come sqrt(), pow() o sin().
#
ULIBS =

##############################################################################
# Common rules
##############################################################################

RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk

include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk

##############################################################################
# Custom rules
##############################################################################