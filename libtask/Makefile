LIB=libtask.a
TCPLIBS=

ASM=asm.o
OFILES=\
	$(ASM)\
	channel.o\
	context.o\
	fd.o\
	net.o\
	print.o\
	qlock.o\
	rendez.o\
	task.o\

all: $(LIB) primes tcpproxy testdelay

$(OFILES): taskimpl.h task.h 386-ucontext.h power-ucontext.h

AS=g++ -c
CC=g++

CFLAGS= -g -Wall -fPIC -pipe  -c -fno-inline -fno-strict-aliasing \
		-O2 -export-dynamic -Wno-deprecated -m64  \
		 -D_GNU_SOURCE -D_REENTRANT -DVERSION_DEBUG \
		-I. 

LIBFLAGS=  -lpthread -ldl

%.o: %.S
	$(AS) $*.S

%.o: %.c
	$(CC) $(CFLAGS) $*.c -o $@

$(LIB): $(OFILES)
	ar rvc $(LIB) $(OFILES)

primes: primes.o $(LIB)
	$(CC) -o primes primes.o $(LIB) $(LIBFLAGS)

tcpproxy: tcpproxy.o $(LIB)
	$(CC) -o tcpproxy tcpproxy.o $(LIB) $(TCPLIBS) $(LIBFLAGS)

httpload: httpload.o $(LIB)
	$(CC) -o httpload httpload.o $(LIB) $(LIBFLAGS)

testdelay: testdelay.o $(LIB)
	$(CC) -o testdelay testdelay.o $(LIB) $(LIBFLAGS)

testdelay1: testdelay1.o $(LIB)
	$(CC) -o testdelay1 testdelay1.o $(LIB) $(LIBFLAGS)

clean:
	rm -f *.o primes tcpproxy testdelay testdelay1 httpload $(LIB)

install: $(LIB)
	cp $(LIB) /usr/local/lib
	cp task.h /usr/local/include

