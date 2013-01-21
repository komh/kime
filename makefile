.PHONY: kime clean

CP = copy

kime :
	$(MAKE) -C hchlb release KIME=1
	$(MAKE) -C hanlib release KIME=1
	$(MAKE) -C hst release KIME=1
	$(MAKE) -C kime
	$(CP) kime\kime.exe
	$(CP) kime\kimehook.dll
	$(CP) kime\except.dat

clean :
	$(MAKE) -C hchlb clean
	$(MAKE) -C hanlib clean
	$(MAKE) -C hst clean
	$(MAKE) -C kime clean
