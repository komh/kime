EXE_NAME   = kime.exe
SRC_LIST   = kime.c kimedlg.c inputbox.c khserver.c hwndlist.c except.c
RC_LIST    = kime.rc
A_LIST     =
LIB_LIST   = kimehook.lib $(HAN_LIB)

GNU_MAKE = make
HAN_LIB_DIR = ../hanlib
HAN_LIB = $(HAN_LIB_DIR)/han.lib

.INCLUDE : "makefile.inc"

kime.obj: kime.c kime.h kimedlg.h kimehook.h khserver.h inputbox.h \
          $(HAN_LIB_DIR)/han.h $(HAN_LIB_DIR)/hch.h $(HAN_LIB_DIR)/hout.h \
          $(HAN_LIB_DIR)/hin.h

kimedlg.obj: kimedlg.c kimedlg.h

inputbox.obj: inputbox.c inputbox.h $(HAN_LIB_DIR)/han.h $(HAN_LIB_DIR)/hch.h \
              $(HAN_LIB_DIR)/hout.h $(HAN_LIB_DIR)/hin.h

khserver.obj: khserver.c khserver.h hwndlist.h

hwndlist.obj: hwndlist.c hwndlist.h

except.obj: except.c except.h

kimehook.obj: kimehook.c kime.h kimehook.h kimedlg.h khserver.h \
              kimeconv.inc inputbox.h $(HAN_LIB_DIR)/han.h $(HAN_LIB_DIR)/hch.h \
              $(HAN_LIB_DIR)/hout.h $(HAN_LIB_DIR)/hin.h

kimehook.lib : kimehook.dll kimehook.def
    $(IMP) -o $@ kimehook.def

kimehook.dll : kimehook.obj $(HAN_LIB) kimehook.def
    $(CC) $(LFDLL) $&

$(HAN_LIB) :
    $(GNU_MAKE) -C $(HAN_LIB_DIR) han.lib
