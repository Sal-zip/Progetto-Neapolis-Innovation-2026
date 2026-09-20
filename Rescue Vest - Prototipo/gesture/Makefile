##############################################################################
# Build global options
# NOTE: Can be overridden externally.
##############################################################################

# Compiler optimization options.
ifeq ($(USE_OPT),)
  USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16
endif

# C-specific options.
ifeq ($(USE_COPT),)
  USE_COPT =
endif

# C++-specific options.
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

# Remove unused functions and data at link time.
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Extra linker options.
ifeq ($(USE_LDOPT),)
  USE_LDOPT =
endif

# Link-time optimization.
ifeq ($(USE_LTO),)
  USE_LTO = no
endif

# Full compiler output.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# Build only the enabled ChibiOS modules.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

##############################################################################
# Architecture-specific options
##############################################################################

# Cortex-M process stack.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

# Cortex-M exception/main stack.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

# Floating-point support.
ifeq ($(USE_FPU),)
  USE_FPU = no
endif

ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv4-sp-d16
endif

##############################################################################
# Project, target and paths
##############################################################################

PROJECT = gesture_board

MCU = cortex-m4

#
# Questo Makefile viene eseguito dalla directory Gesture-Board.
#
# Rispetto al progetto principale si trova un livello più in profondità:
#
#   progetto principale -> ../../chibios2111
#   Gesture-Board       -> ../../../chibios2111
#
# Il percorso può comunque essere sovrascritto da ChibiStudio:
#
#   make CHIBIOS=C:/ChibiStudio/chibios2111
#
CHIBIOS ?= ../../../chibios2111

CONFDIR  := ./ChibiOS/cfg
BUILDDIR := ./build
DEPDIR   := ./.dep

##############################################################################
# ChibiOS components
##############################################################################

# Licensing files.
include $(CHIBIOS)/os/license/license.mk

# STM32G4 startup files.
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32g4xx.mk

# HAL and platform.
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32G4xx/platform.mk
include $(CHIBIOS)/os/hal/boards/ST_NUCLEO64_G474RE/board.mk
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk

# ChibiOS/RT.
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M-ALT/compilers/GCC/mk/port.mk

# Automatic selection of enabled ChibiOS modules.
include $(CHIBIOS)/tools/mk/autobuild.mk

# Stream e chprintf.
include $(CHIBIOS)/os/hal/lib/streams/streams.mk

##############################################################################
# Gesture Board packages
##############################################################################

#
# Driver ST VL53L7CX:
#   - driver ULD;
#   - plugin motion indicator;
#   - porting ChibiOS/I2C.
#
include ./VL53L7CX/vl53l7cx.mk

#
# Comunicazione tra le due board.
#
# Questa build appartiene alla board gesture, quindi deve includere
# esclusivamente il trasmettitore.
#
GESTURE_LINK_ROLE := TX
include ./Communication/gesture_link.mk

#
# Logica applicativa della Gesture Board.
#
include ./Application/application.mk

#
# Il package Gesture non dispone ancora di un proprio gesture.mk.
#
GESTUREDIR := ./Gesture

GESTURESRC := \
    $(GESTUREDIR)/src/gesture.c

GESTUREINC := \
    $(GESTUREDIR)/lib

ALLCSRC += $(GESTURESRC)
ALLINC  += $(GESTUREINC)

#
# Il package Buzzer non dispone ancora di un proprio buzzer.mk.
#
BUZZERDIR := ./Buzzer

BUZZERSRC := \
    $(BUZZERDIR)/src/buzzer.c

BUZZERINC := \
    $(BUZZERDIR)/lib

ALLCSRC += $(BUZZERSRC)
ALLINC  += $(BUZZERINC)

##############################################################################
# Linker script
##############################################################################

LDSCRIPT = $(STARTUPLD)/STM32G474xE.ld

##############################################################################
# Project sources
##############################################################################

#
# ALLCSRC contiene:
#
#   - sorgenti ChibiOS/HAL;
#   - driver VL53L7CX;
#   - gesture_protocol.c;
#   - gesture_link_tx.c;
#   - gesture_application.c;
#   - gesture.c;
#   - buzzer.c.
#
# Il solo punto di ingresso è Gesture-Board/main.c.
#
CSRC = $(ALLCSRC) \
       $(TESTSRC) \
       main.c

# C++ sources.
CPPSRC = $(ALLCPPSRC)

# Assembly sources.
ASMSRC = $(ALLASMSRC)

# Preprocessed assembly sources.
ASMXSRC = $(ALLXASMSRC)

##############################################################################
# Include directories
##############################################################################

INCDIR = $(CONFDIR) \
         $(ALLINC) \
         $(TESTINC)

##############################################################################
# Compiler warnings
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
# Non è necessario CHPRINTF_USE_FLOAT perché i log della Gesture Board
# non stampano valori floating-point.
#
UDEFS =

# Assembly definitions.
UADEFS =

# Additional include directories.
UINCDIR =

# Additional library directories.
ULIBDIR =

# Additional libraries.
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