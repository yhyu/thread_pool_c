
CFLAGS := -O3 -fPIC
TP_OBJ := thread_pool.o

LIBS = -lpthread

SLIB = libtpl.a
SHARED_LIB= libtpl.so

CC := gcc 
AR := ar r

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
