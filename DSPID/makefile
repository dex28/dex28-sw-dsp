all:: dspid.hex dspid.out

dspid.hex: dspid.out
	hex500 hex.cmd

dspid.out: dspid.obj dspid.cmd
	lnk500 -o dspid.out dspid.obj dspid.cmd

dspid.obj: dspid.asm
	asm500 -g -al dspid.asm
	asm500 -g dspid.asm

.clean clean::
	@ echo removing object files and binaries ...
	@$(REMOVE) -f *.obj *.out *.lst *.map

