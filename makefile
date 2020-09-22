# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
#che dipende dai target client e server scritti sotto
all: lotto_client lotto_server
# make rule per il client
client: lotto_client.o
	gcc –Wall lotto_client.o –o lotto_client
# make rule per il server
server: lotto_server.o
	gcc –Wall lotto_server.o –o lotto_server
# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o lotto_client lotto_server
