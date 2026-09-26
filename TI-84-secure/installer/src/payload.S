/* CESecure: the application image, embedded in the installer program.
 * Built from bin/CESecure.8ek by `make -f app.mk installer`. */
#ifdef EMBEDDED_APP
	.assume	adl=1
	.section	.rodata._app_payload, "a", @progbits
	.global	_app_payload
	.global	_app_payload_end
_app_payload:
	.incbin	"CESecure.bin"
_app_payload_end:
#endif
