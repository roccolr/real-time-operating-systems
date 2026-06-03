CC = gcc
CFLAGS = -Wall 

hello:
	echo "[MAKE] beginning compilation..."

get_time: get_time.c 
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm get_time