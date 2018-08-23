BIN=supzip
LIB=lib$(BIN).a
DLL=lib$(BIN).so
GCC=cc

CFLAGS= -g -O -I../..
LFLAGS= -static -L ./ -lsupzip -lz

SRCS := supzip.c unzip.c zip.c ioapi.c
OBJS := $(patsubst %.c,%.o,$(notdir $(SRCS))) 

$(LIB):$(OBJS)
	ar -r $@ $(LIB_OBJS)
$(OBJS):%.o:%.c
	$(GCC) -c $< $(CFLAGS)
	
$(DLL):$(OBJS)
	$(GCC) -c -fpic $(SRCS) $(CFLAGS)
	$(GCC) -shared $(OBJS) -o $@
	
$(BIN): 
	gcc -o $@ test.c -L ./ -lsupzip -lz
	
all: clean static shared $(BIN)
static: $(LIB) 
shared: $(DLL)
clean:
	/bin/rm -f *.o *~ $(BIN)
