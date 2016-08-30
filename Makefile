
CFLAGS := -O3 -fPIC
TP_OBJ := thread_pool.o

LIBS = -lpthread

SLIB = libtpl.a
SHARED_LIB= libtpl.so

CC := gcc 
AR := ar r

#EXTRA_LDFLAGS = -L.
#EXTRA_SYSLIBS += -Wl,--no-as-needed -lpthread -ldl -lm $(SLIB)
#
#EXTRA_INCS = -I. -O3
#EXTRA_CFLAGS += -fno-strict-aliasing -Wno-error=array-bounds -Wno-error=aggressive-loop-optimizations -Wno-unused-result

#TEST_PROG = thread_pool_test
#OBJECTS_PROG = thread_pool_test.o
TEST_SRC = thread_pool_test.c

all: tpl_lib tpl_test

tpl_lib: $(TP_OBJ)
	$(AR) $(SLIB) $(TP_OBJ)
	$(CC) -shared -o $(SHARED_LIB) $(TP_OBJ)

tpl_test: $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@  $^ $(LIBS) $(SLIB)

clean: 
	rm -f *.o *.a *.so core* tpl_test

thread_pool.o: thread_pool.h
