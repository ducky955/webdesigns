# ----------------------------
# CESecure as a Flash application (shows up under [apps]).
#
#   make -f app.mk            builds bin/CESecure.8ek
#   make -f app.mk installer  also builds bin/CESINST.8xp + bin/CESecA0.8xv
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

# The installer reads the app from AppVars named CESecA0, CESecA1, ...
APPVAR_PREFIX = CESecA
APPVAR_SPLIT_SIZE = 65200

# ----------------------------

include $(shell cedev-config --makefile)

installer: $(BINDIR)/$(NAME).8ek
	$(Q)convbin --iformat 8ek --input $(BINDIR)/$(NAME).8ek --oformat 8xv-split \
		--maxvarsize $(APPVAR_SPLIT_SIZE) --output $(BINDIR)/$(APPVAR_PREFIX).8xv --name $(APPVAR_PREFIX)
	$(Q)$(MAKE) -C installer APPVAR_PREFIX=\"$(APPVAR_PREFIX)\" APPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)
	$(Q)cp installer/bin/CESINST.8xp $(BINDIR)/CESINST.8xp
	$(Q)echo [success] $(BINDIR)/CESINST.8xp + $(BINDIR)/$(APPVAR_PREFIX)*.8xv

.PHONY: installer
