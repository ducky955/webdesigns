/*
 * Running programs from the CESecure Flash application (app build only).
 *
 * The program version uses the toolchain's os_RunPrgm, which returns by
 * reloading the caller as a program. An app can't be reloaded that way, so
 * the app build launches programs itself and, when they finish, restarts
 * the app from __start (its code is in flash, so nothing needs reloading).
 *
 * The launch and return sequence follows Cesium's execute.asm and the
 * toolchain's runprgm.src. Cesium is:
 *
 *   Copyright 2015-2024 Matt "MateoConLechuga" Waltz
 *   Licensed under the BSD 3-Clause License (see THIRD_PARTY.md).
 */

#ifdef CESECURE_APP

	.assume	adl=1

	.equ	ti.ChkFindSym, 0x02050C
	.equ	ti.ChkInRam, 0x021F98
	.equ	ti.Mov9ToOP1, 0x020320
	.equ	ti.OP4ToOP1, 0x0202C8
	.equ	ti.LoadDEInd_s, 0x021D9C
	.equ	ti.EnoughMem, 0x02051C
	.equ	ti.InsertMem, 0x020514
	.equ	ti.DelMem, 0x020590
	.equ	ti.CreateVar, 0x021338
	.equ	ti.ChkBCIs0, 0x021D48
	.equ	ti.DeleteTempPrograms, 0x0214C0
	.equ	ti.DisableAPD, 0x021134
	.equ	ti.EnableAPD, 0x021138
	.equ	ti.AppInit, 0x020190
	.equ	ti.ReloadAppEntryVecs, 0x021DC4
	.equ	ti.PushErrorHandler, 0x020798
	.equ	ti.PopErrorHandler, 0x02079C
	.equ	ti.ParseInp, 0x020F00
	.equ	ti.JError, 0x020790
	.equ	ti.SetParserHook, 0x02149C
	.equ	ti.ClrParserHook, 0x0214A0
	.equ	ti.ClrHomescreenHook, 0x021414
	.equ	ti.ForceFullScreen, 0x021528
	.equ	ti.SaveShadow, 0x020850
	.equ	ti.RStrShadow, 0x020854
	.equ	ti.PutAway, 0x02017C
	.equ	ti.HomeUp, 0x020828
	.equ	ti.ClrScrn, 0x020814
	.equ	ti.GetCSC, 0x02014C

	.equ	ti.OP1, 0xD005F8
	.equ	ti.userMem, 0xD1A881
	.equ	ti.asm_prgm_size, 0xD0118C
	.equ	ti.kbdGetKy, 0xD0058D
	.equ	ti.currLastEntry, 0xD01D0C
	.equ	ti.parserHookPtr, 0xD025F9
	.equ	ti.mpTmrCtrl, 0xF20030

	.equ	ti.flags, 0xD00080
	.equ	ti.newDispF, 0x08
	.equ	ti.progExecuting, 1
	.equ	ti.allowProgTokens, 1
	.equ	ti.cmdFlags, 0x0C
	.equ	ti.cmdExec, 6
	.equ	ti.textFlags, 0x05
	.equ	ti.textInverse, 3
	.equ	ti.onFlags, 0x09
	.equ	ti.onInterrupt, 4
	.equ	ti.appFlags, 0x0D
	.equ	ti.appAutoScroll, 2
	.equ	ti.apiFlg2, 0x29
	.equ	ti.appInpPrmptInit, 0
	.equ	ti.hookflags4, 0x36
	.equ	ti.parserHookActive, 1

	.equ	ti.ProgObj, 0x05
	.equ	ti.TempProgObj, 0x16

	.equ	RUN_NOT_FOUND, -1
	.equ	RUN_NO_MEMORY, -2

	.section	.text.app_run, "ax", @progbits

/* int app_run_asm(const char name[9]);
 * Copies a compiled program to userMem and runs it. Returns only on error. */
	.global	_app_run_asm
	.type	_app_run_asm, @function
_app_run_asm:
	call	find_program		; hl -> size bytes, z = in RAM
	jp	c, return_not_found
	push	af			; remember whether it is in RAM
	call	ti.LoadDEInd_s		; de = size, hl -> data
	inc	hl
	inc	hl			; skip tExtTok, tAsm84CeCmp
	push	hl			; source
	push	de			; size
	ex	de, hl
	call	ti.EnoughMem		; carry = not enough
	pop	hl			; size
	jr	nc, .L.asm_enough
	pop	hl
	pop	af
	jp	return_no_memory
.L.asm_enough:
	ld	(ti.asm_prgm_size), hl
	ld	de, ti.userMem
	call	ti.InsertMem
	pop	hl			; source
	pop	af
	ld	bc, (ti.asm_prgm_size)
	jr	nz, .L.asm_copy		; archived data doesn't move
	add	hl, bc			; RAM data was shifted up by InsertMem
.L.asm_copy:
	ld	de, ti.userMem
	ldir

	call	ti.DisableAPD
	set	ti.appAutoScroll, (iy + ti.appFlags)
	call	setup_context
	ld	hl, run_error
	call	ti.PushErrorHandler
	ld	hl, run_return
	push	hl
	jp	ti.userMem

/* int app_run_basic(const char name[9]);
 * Runs a TI-BASIC program, copying it to a temp program first if it is
 * archived. Returns only on error. */
	.global	_app_run_basic
	.type	_app_run_basic, @function
_app_run_basic:
	call	find_program		; hl -> size bytes, z = in RAM
	jp	c, return_not_found
	jr	z, .L.basic_ready

	call	ti.LoadDEInd_s		; de = size, hl -> data
	push	hl			; source
	push	de			; size
	ex	de, hl
	ld	de, 64			; room for the temp variable's VAT entry
	add	hl, de
	call	ti.EnoughMem
	jr	nc, .L.basic_enough
	pop	hl
	pop	hl
	jp	return_no_memory
.L.basic_enough:
	call	ti.DeleteTempPrograms
	ld	hl, temp_program_name
	call	ti.Mov9ToOP1
	pop	hl			; size
	push	hl
	ld	a, ti.TempProgObj
	call	ti.CreateVar		; de -> size bytes of the new variable
	inc	de
	inc	de
	pop	bc			; size
	pop	hl			; source (flash, so it didn't move)
	call	ti.ChkBCIs0
	jr	z, .L.basic_copied
	ldir
.L.basic_copied:
	call	ti.OP4ToOP1		; OP1 = temp program

.L.basic_ready:
	ld	iy, ti.flags
	call	ti.DisableAPD
	set	ti.progExecuting, (iy + ti.newDispF)
	set	ti.cmdExec, (iy + ti.cmdFlags)
	set	ti.allowProgTokens, (iy + ti.newDispF)

	/* Catch [Stop] so it returns here instead of to the home screen.
	 * Leave any other shell's parser hook alone. */
	bit	ti.parserHookActive, (iy + ti.hookflags4)
	jr	nz, .L.basic_no_hook
	ld	hl, stop_hook
	call	ti.SetParserHook
.L.basic_no_hook:

	call	setup_context
	ld	hl, run_error
	call	ti.PushErrorHandler
	call	ti.EnableAPD
	ei
	ld	hl, run_return
	push	hl
	jp	ti.ParseInp

return_not_found:
	ld	hl, RUN_NOT_FOUND
	ret
return_no_memory:
	ld	hl, RUN_NO_MEMORY
	ret

/* In: (sp + 6) = name (caller's argument, one call deep).
 * Out: carry = not found; otherwise hl -> size bytes, z = in RAM, and OP1
 * holds the program name. */
find_program:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy + 6)
	ld	de, ti.OP1
	ld	a, ti.ProgObj
	ld	(de), a
	inc	de
	ld	bc, 8
	ldir
	xor	a, a
	ld	(de), a
	ld	iy, ti.flags
	call	ti.ChkFindSym
	ret	c
	call	ti.ChkInRam
	ex	de, hl
	jr	nz, .L.find_archived
	xor	a, a			; z, nc
	ret
.L.find_archived:
	ld	de, 9			; skip the archive header and name
	add	hl, de
	ld	e, (hl)
	add	hl, de
	inc	hl
	or	a, 1			; nz, nc
	ret

/* Tell the OS an app-launched program is running (as Cesium does). */
setup_context:
	ld	hl, app_vectors
	jp	ti.AppInit

app_vectors:
	.d24	vector_ret
	.d24	ti.SaveShadow
	.d24	vector_putaway
	.d24	vector_restore
	.d24	vector_ret
	.d24	vector_ret

vector_restore:
	call	ti.HomeUp
	call	ti.ClrScrn
	jp	ti.RStrShadow
vector_ret:
	ret
vector_putaway:
	xor	a, a
	ld	(ti.currLastEntry), a
	bit	ti.appInpPrmptInit, (iy + ti.apiFlg2)
	jr	nz, .L.putaway_prompt
	call	ti.ClrHomescreenHook
	call	ti.ForceFullScreen
.L.putaway_prompt:
	call	ti.ReloadAppEntryVecs
	call	ti.PutAway
	ld	b, 0
	ret

/* Parser hook: turn [Stop] into a zero error so run_error handles it. */
stop_hook:
	.db	0x83			; hook signature
	cp	a, 2
	jr	nz, .L.hook_pass
	ld	a, 0xD9 - 0xCE		; Stop token
	sub	a, b
	jp	z, ti.JError
.L.hook_pass:
	xor	a, a
	ret

/* Both normal exit and errors end here. Clean up, then restart the app;
 * main() sees the CESecRun AppVar and goes straight back to the list. */
run_return:
	call	ti.PopErrorHandler
run_error:
	ld	iy, ti.flags
	ld	hl, ti.mpTmrCtrl + 1	; stop timers the program left running
	ld	a, (hl)
	and	a, l
	ld	(hl), a
	res	ti.progExecuting, (iy + ti.newDispF)
	res	ti.cmdExec, (iy + ti.cmdFlags)
	res	ti.allowProgTokens, (iy + ti.newDispF)
	res	ti.textInverse, (iy + ti.textFlags)
	res	ti.onInterrupt, (iy + ti.onFlags)
	call	ti.ReloadAppEntryVecs
	call	ti.ForceFullScreen

	bit	ti.parserHookActive, (iy + ti.hookflags4)
	jr	z, .L.no_our_hook
	ld	hl, (ti.parserHookPtr)
	ld	de, stop_hook
	or	a, a
	sbc	hl, de
	call	z, ti.ClrParserHook
.L.no_our_hook:

	ld	de, (ti.asm_prgm_size)	; free the copied program
	ld	hl, ti.userMem
	call	ti.DelMem
	or	a, a
	sbc	hl, hl
	ld	(ti.asm_prgm_size), hl
	call	ti.DeleteTempPrograms

.L.debounce:
	call	ti.GetCSC
	or	a, a
	jr	nz, .L.debounce
	ld	(ti.kbdGetKy), a

	jp	__start

temp_program_name:
	.db	ti.TempProgObj, "CESECTMP", 0

#endif
