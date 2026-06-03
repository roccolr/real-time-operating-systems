CC = gcc 
CFLAGS = -Wall
TARGET = main
OBJ = main.o 

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

main.o: main.c 
	$(CC) $(CFLAGS) -c main.c -o $@

clean:
	rm -rf *.o 
	echo "rimozione completa";