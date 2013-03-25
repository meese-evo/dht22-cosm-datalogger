CC = g++
CFLAGS =  -std=c99 -I. -lbcm2835 -lcurl
DEPS = 
OBJ = dht_logger.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

dht_logger: $(OBJ)
	g++ -o $@ $^ $(CFLAGS)
