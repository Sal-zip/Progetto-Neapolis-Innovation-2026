##############################################################################
# Gesture communication module
#
# Prima di includere questo file definire:
#
#   GESTURE_LINK_ROLE := TX
#
# per la board gesture, oppure:
#
#   GESTURE_LINK_ROLE := RX
#
# per la board principale.
##############################################################################

ifndef GESTURE_LINK_MK_INCLUDED
GESTURE_LINK_MK_INCLUDED := yes

# Directory nella quale si trova questo Makefile.
GESTURELINKDIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# Sorgente condiviso da entrambe le board.
GESTURELINKSRC := \
    $(GESTURELINKDIR)/src/gesture_protocol.c

# Header pubblici del protocollo e del collegamento.
GESTURELINKINC := \
    $(GESTURELINKDIR)/lib

##############################################################################
# Selezione del ruolo
##############################################################################

ifeq ($(strip $(GESTURE_LINK_ROLE)),TX)

# Board gesture: invia i comandi e attende l'ACK.
GESTURELINKSRC += \
    $(GESTURELINKDIR)/src/gesture_link_tx.c

else ifeq ($(strip $(GESTURE_LINK_ROLE)),RX)

# Board principale: riceve i comandi e restituisce l'ACK.
GESTURELINKSRC += \
    $(GESTURELINKDIR)/src/gesture_link_rx.c

else

$(error GESTURE_LINK_ROLE deve essere definito come TX oppure RX)

endif

##############################################################################
# Esportazione verso il Makefile che include il modulo
##############################################################################

ALLCSRC += $(GESTURELINKSRC)
ALLINC  += $(GESTURELINKINC)

endif