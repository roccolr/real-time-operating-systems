# Makefile for dummies 
Wrapper for common used Makefile patterns

## Instruction for vscodium project setup
In order to create a new c project on linux:
1. Organize your project tree creating directories and files
2. Write .h header files 
3. Create Makefile 
4. run ```bear -- make```

Prerequisites:
1. make 
2. vscodium 
3. bear
4. _clangd_ and _clangd tidy_ extensions for vscodium

## Basic Makefile example 
The project should look like this:

```bash
project/
├── bin
│   └── main
├── include
│   ├── lib1.h
│   └── lib2.h
├── Makefile
└── src
    ├── lib1.c
    ├── lib2.c
    └── main.c
```

A basic Makefile should look like this:

``` Makefile
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
```


