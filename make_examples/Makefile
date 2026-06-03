CC = gcc 
FLAGS = -Wall
INCLUDES = -I./include 
SRC = ./src/lib1.c ./src/lib2.c ./src/main.c
OBJS = lib1.o lib2.o main.o 
MAIN_SRC = ./src/main.c
TARGET_MAIN = ./bin/main

$(TARGET_MAIN): $(OBJS) 
	$(CC) $(FLAGS) $(INCLUDES) -o $@ $?
	rm -rf *.o

main.o: $(MAIN_SRC)
	$(CC) $(FLAGS) $(INCLUDES) -c $? -o $@

lib1.o: ./src/lib1.c 
	$(CC) $(FLAGS) $(INCLUDES) -c $? -o $@

lib2.o: ./src/lib2.c 
	$(CC) $(FLAGS) $(INCLUDES) -c $? -o $@

clean:
	rm -rf *.o
	rm -f main
	rm -rf *.json

boot: 
	bear -- make