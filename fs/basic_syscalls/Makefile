CC = gcc 
FLAGS = -Wall
INCLUDES = -I./include
SRC = ./src/main.c
OBJS = main.o
TARGET = ./out/main
FILEDIR = ./files/
  
$(TARGET): $(OBJS)
	$(CC) $(FLAGS) $(INCLUDES) -o $@ $?
	rm -rf *.o 

main.o: $(SRC)
	$(CC) $(FLAGS) $(INCLUDES) -c $? -o $@

clean:
	rm -rf *.o 
	rm -f $(TARGET)

clean_files: 
	rm -rf $(FILEDIR)output.txt
