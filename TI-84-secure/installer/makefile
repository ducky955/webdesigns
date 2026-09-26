# ----------------------------
# Makefile Options
# ----------------------------

NAME = CESINST
COMPRESSED = NO
ARCHIVED = NO
LTO = NO
HAS_PRINTF = NO

APPVAR_PREFIX ?= "CESecA"
APPVAR_SPLIT_SIZE ?= 65200

# CESecure: the app image is embedded in the program (src/payload.S) instead
# of being read from AppVars. PAYLOAD_DIR holds CESecure.bin.
PAYLOAD_DIR ?= ../bin
ASFLAGS = -I $(abspath $(PAYLOAD_DIR))

CFLAGS = -Wall -Wextra -Oz -DEMBEDDED_APP -DAPPVAR_PREFIX=\"$(APPVAR_PREFIX)\" -DAPPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)
CXXFLAGS = -Wall -Wextra -Oz -DEMBEDDED_APP -DAPPVAR_PREFIX=\"$(APPVAR_PREFIX)\" -DAPPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)

# ----------------------------

include $(shell cedev-config --makefile)
