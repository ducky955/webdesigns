# ----------------------------
# CESecure as a Flash application (shows up under [apps]).
#
#   make -f app.mk            builds bin/CESecure.8ek
#   make -f app.mk installer  also builds bin/CESINST3.8xp, a single-file
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
	@# The OS copies the header's init data (.data + .bss) into a ~4 KB app RAM
	@# area when the app opens; more than that overwrites the stack and resets
	@# the calculator. Fail the build instead.
	$(Q)size=$$(od -An -tu1 -j280 -N3 $(BINDIR)/$(NAME).bin | awk '{print $$1 + $$2*256 + $$3*65536}'); \
	if [ $$size -gt 3840 ]; then echo "error: .data + .bss is $$size bytes; keep it under 3840 (move big arrays to the heap)"; exit 1; fi; \
	echo "[check] app init data: $$size bytes (limit 3840)"
	$(Q)$(MAKE) -C installer clean
	$(Q)$(MAKE) -C installer PAYLOAD_DIR=$(abspath $(BINDIR))
	$(Q)cp installer/bin/CESINST3.8xp $(BINDIR)/CESINST3.8xp
	$(Q)echo "[success] $(BINDIR)/CESINST3.8xp (installer with the app inside)"

.PHONY: installer
