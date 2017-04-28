CFLAGS=-g

all: rapl_lib_shared rapl_lib_static power_gov_static power_govd_static socket_client

rapl_lib_shared: 
	gcc $(CFLAGS) -fpic -c msr.c cpuid.c rapl.c 
	gcc $(CFLAGS) -shared -o librapl.so msr.o cpuid.o rapl.o

rapl_lib_static: 
	gcc $(CFLAGS) -c msr.c cpuid.c rapl.c 
	ar rcs librapl.a msr.o cpuid.o rapl.o

power_gov_static: 
	gcc $(CFLAGS) power_gov.c -I. -L. -lm -o power_gov ./librapl.a

power_gov: 
	gcc $(CFLAGS) power_gov.c -I. -L. -lm -lrapl -o power_gov 

power_govd_static: 
	gcc $(CFLAGS) power_govd.c -I. -L. -lm -o power_govd ./librapl.a

socket_client: 
	gcc $(CFLAGS) socket_client.c -o socket_client 

clean: 
	rm -f power_gov librapl.so librapl.a msr.o cpuid.o rapl.o power_govd socket_client
