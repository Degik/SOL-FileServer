CC = gcc
CFLAGS = -g -lpthread
CONFIGFILE1 = ./configs/config1.txt
CONFIGFILE1 = ./configs/config2.txt
CLIENTOUTPUT_DIR1 = ./outputTest1
CLIENTOUTPUT_DIR2 = ./outputTest2
FLAG_INCLUDE_DIR = -I ./includes
SOCKNAME = ./sock.sk
TARGETS = server client server.dSYM client.dSYM

SERVER_DEPS = server.c util.c list.c
CLIENT_DEPS = client.c util.c list.c parser.c

.PHONY : all, cleanall, test1, test2

all:
	make --no-print-directory -B server
	make --no-print-directory -B client

cleanall:
	rm -rf $(SOCKNAME) $(TARGETS) $(CLIENTOUTPUT_DIR1) $(CLIENTOUTPUT_DIR2)

#targets per generare gli eseguibili
server: $(SERVER_DEPS)
	$(CC) $(CFLAGS) $(SERVER_DEPS) -o server $(FLAG_INCLUDE_DIR)

client: $(CLIENT_DEPS)
	$(CC) $(CFLAGS) $(CLIENT_DEPS) -o client $(FLAG_INCLUDE_DIR)

test1:
	chmod +x ./test/test1.sh
	./test/test1.sh $(CLIENTOUTPUT_DIR1)

test2:
	chmod +x ./test/test1.sh
	./test/test2.sh $(CLIENTOUTPUT_DIR2)
