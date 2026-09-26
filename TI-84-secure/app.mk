# ----------------------------
# CESecure as a Flash application (shows up under [apps]).
#
#   make -f app.mk            builds bin/CESecure.8ek
#   make -f app.mk installer  also builds bin/CESINST.8xp, a single-file
#                             installer with the app embedded
#
# See README.md for installing it on a calculator.
# ----------------------------

NAME = CESecure
APPLICATION = YES
APPLICATION_DESCRIPTION = "PIN-locked launcher"
ICON = icon.png
DESCRIPTION = "CESecure - PIN-locked launcher"
OBJDIR = obj/app

CFLAGS = -Wall -Wextra -Oz -DCESECURE_APP
CXXFLAGS = -Wall -Wextra -Oz -DCESECURE_APP

# ----------------------------

include $(shell cedev-config --makefile)

installer: $(BINDIR)/$(NAME).8ek
	$(Q)convbin --iformat 8ek --input $(BINDIR)/$(NAME).8ek --oformat bin --output $(BINDIR)/$(NAME).bin
	$(Q)$(MAKE) -C installer clean
	$(Q)$(MAKE) -C installer PAYLOAD_DIR=$(abspath $(BINDIR))
	$(Q)cp installer/bin/CESINST.8xp $(BINDIR)/CESINST.8xp
	$(Q)echo "[success] $(BINDIR)/CESINST.8xp (installer with the app inside)"

.PHONY: installer
