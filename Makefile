all: library binder

binder: binder.cc binder.h
	g++ binder.cc -o binder

library: rpc.cc rpc.h server_database.cc server_database.h helper_code.h
	g++ -c rpc.cc server_database.cc 
	ar -cvq librpc.a rpc.o server_database.o


clean:
	rm *.o librpc.a binder


