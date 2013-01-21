.SUFFIXES : .h .c .cpp .o .obj .exe .rc .res

CC		 = gcc
CFLAGS_R = -Wall -Zomf -Zcrtdll -s
LFLAGS_R = -lwrap
LFDLL	 = -Zdll -Zso -Zsys -Zomf

IMP 	= emximp

INC_DIR = ../include
LIB_DIR = ../lib

RC		= rc

CP		= +copy
RM		= +del

#Follow macros is defined by 'makefile' in each directory
#EXE_NAME	=
#SRC_LIST	=
#RC_LIST	=
#A_LIST 	=
#LIB_LIST	=
O_LIST	   = $(addsuffix .o, $(basename $(SRC_LIST)))
OBJ_LIST   = $(O_LIST:.o=.obj)
RES_LIST   = $(addsuffix .res, $(basename $(RC_LIST)))
DEF_NAME   = $(EXE_NAME:.exe=.def)

.c.o :
	$(CC) $(CFLAGS_D) -I$(INC_DIR) -c $< -o $@

.cpp.o :
	$(CC) $(CFLAGS_D) -I$(INC_DIR) -c $< -o $@

.c.obj :
	$(CC) $(CFLAGS_R) -I$(INC_DIR) -c $< -o $@

.cpp.obj :
	$(CC) $(CFLAGS_R) -I$(INC_DIR) -c $< -o $@

.rc.res :
	$(RC) -r $<

$(EXE_NAME) : $(OBJ_LIST) $(LIB_LIST) $(DEF_NAME) $(RES_LIST)
	$(CC) $(CFLAGS_R) $(LFLAGS_R) $^ -o $@

clean :
	-$(RM) *.o *.obj
	-$(RM) core*.*
	-$(RM) *.dll *.lib *.exe
	-$(RM) *.~* *.bak
	-$(RM) *.res *.rwp
