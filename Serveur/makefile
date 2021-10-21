EXEC = ../bin/Serveur
OBJ = main.o 
LFLAGS = -pthread

all: $(EXEC)

$(EXEC): $(OBJ) 
	$(CC) -o $(EXEC) $(OBJ) $(CFLAGS) $(LFLAGS)
	
%.o: %.c %.h
	$(CC) -o $@ -c $< $(CFLAGS) $(LFLAGS)
	
main.o: main.c
	$(CC) -o main.o -c main.c $(CFLAGS) $(LFLAGS) -Wall
	
clean:
	rm -r *.o 
	
clean_all: clean
	rm -r $(EXEC)
