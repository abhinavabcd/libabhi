
LIB=libtask.a
LIB_O=libtask.o
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



all: $(LIB) $(LIB_O) primes tcpproxy testdelay

lib: $(LIB_O)


$(OFILES): taskimpl.h task.h 386-ucontext.h power-ucontext.h

AS=g++ -c
CC=g++

CFLAGS= -g -Wall -fPIC -pipe -fno-inline \
		-O2 -D_GNU_SOURCE -D_REENTRANT \
		-I. #-DVERSION_DEBUG \



LIBS=



%.o: %.S
	$(AS) $*.S

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $@


WRAP_SYS_CALLS=-Wl,-wrap=write -Wl,-wrap=read -Wl,-wrap=connect -Wl,-wrap=recv \
		-Wl,-wrap=send -Wl,-wrap=recvfrom -Wl,-wrap=sendto

$(LIB): $(OFILES)
	ar rvc -o $(LIB) $(OFILES)

$(LIB_O): $(OFILES)
	ld -o $(LIB_O) -r $(OFILES)


# $(SHARED_LIB): $(OFILES)
# 	$(CC) -fPIC -shared -o $(SHARED_LIB) $(OFILES) 
# 	# or -Wl,--whole-archive libtask.a -Wl,--no-whole-archive

primes: primes.o $(LIB)
	$(CC) $(WRAP_SYS_CALLS) -o primes primes.o $(LIB) $(LIBS)

tcpproxy: tcpproxy.o $(LIB)
	$(CC) $(WRAP_SYS_CALLS)  -o tcpproxy tcpproxy.o $(LIB) $(TCPLIBS) $(LIBS)

httpload: httpload.o $(LIB)
	$(CC) $(WRAP_SYS_CALLS)  -o httpload httpload.o $(LIB) $(LIBS)

testdelay: testdelay.o $(LIB)
	$(CC) $(WRAP_SYS_CALLS)  -o testdelay testdelay.o $(LIB) $(LIBS)

testdelay1: testdelay1.o $(LIB)
	$(CC) $(WRAP_SYS_CALLS)  -o testdelay1 testdelay1.o $(LIB) $(LIBS)

clean:
	rm -f *.o primes tcpproxy testdelay testdelay1 httpload $(LIB) $(LIB_O)

install: $(LIB)
	cp $(LIB) /usr/local/lib
	cp task.h /usr/local/include

