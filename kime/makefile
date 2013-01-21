EXE_NAME   = kime.exe
SRC_LIST   = kime.c kimedlg.c inputbox.c khserver.c hwndlist.c except.c
RC_LIST    = kime.rc
A_LIST	   =
LIB_LIST   = kimehook.lib $(HAN_LIB) $(HST_LIB)

COPY = +copy

HAN_LIB_DIR = ../hanlib
HAN_LIB = $(HAN_LIB_DIR)/han.lib

HST_LIB_DIR = ../hst
HST_LIB = $(HST_LIB_DIR)/hst.lib

HCHLBDLG_DIR = ..\hchlb
HCHLBDLG_RES = $(HCHLBDLG_DIR)\hchlbdlg.res

IM32_LIB_DIR = ../im32
IM32_LIB = $(IM32_LIB_DIR)/im32.lib

include makefile.inc

kime.obj: kime.c kime.h kimedlg.h kimehook.h khserver.h inputbox.h \
 ../hanlib/han.h ../hanlib/hch.h ../hanlib/hout.h ../hanlib/hin.h \
  ../hst/hst.h

kimedlg.obj: kimedlg.c kimedlg.h

inputbox.obj: inputbox.c inputbox.h $(HAN_LIB_DIR)/han.h $(HAN_LIB_DIR)/hch.h \
			  $(HAN_LIB_DIR)/hout.h $(HAN_LIB_DIR)/hin.h

khserver.obj: khserver.c khserver.h hwndlist.h

hwndlist.obj: hwndlist.c hwndlist.h

except.obj: except.c except.h

kimehook.o: kimehook.c kime.h kimehook.h kimedlg.h khserver.h \
 kimeconv.inc inputbox.h ../hanlib/han.h ../hanlib/hch.h \
  ../hanlib/hout.h ../hanlib/hin.h ../hchlb/hchlbdlg.h ../im32/im32.h \
   ../im32/toggle.h

kimehook.lib : kimehook.dll kimehook.def
	$(IMP) -o $@ kimehook.def

kimehook.dll : kimehook.obj $(HAN_LIB) $(IM32_LIB) kimehook.def
	$(CC) -o $@ $(LFDLL) $^

kime.res : kime.rc kime.h kimedlg.h $(HCHLBDLG_RES)
	$(RC) -r kime.rc
	$(COPY) /b kime.res + $(HCHLBDLG_RES) kime.res

$(HAN_LIB) :
	$(MAKE) -C $(HAN_LIB_DIR) han.lib

$(HST_LIB) :
	$(MAKE) -C $(HST_LIB_DIR) hst.lib

$(HCHLBDLG_RES) :
	$(MAKE) -C $(HCHLBDLG_DIR) hchlbdlg.res

$(IM32_LIB) :
	$(MAKE) -C $(IM32_LIB_DIR) im32.lib

