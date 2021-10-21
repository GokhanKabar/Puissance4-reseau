EXEC = ../bin/Client
OBJ = main_client.o
LFLAGS = -pthread

all: $(EXEC)

$(EXEC): $(OBJ) 
	$(CC) -o $(EXEC) $(OBJ) $(CFLAGS) $(LFLAGS)
	
%.o: %.c %.h
	$(CC) -o $@ -c $< $(CFLAGS) $(LFLAGS)
	
main_client.o: main_client.c
	$(CC) -o main_client.o -c main_client.c $(CFLAGS) $(LFLAGS) 
	
clean:
	rm -r *.o
	
clean_all: clean
	rm -r $(EXEC)
