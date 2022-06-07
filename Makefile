PROJ2_TASKS = proj2.o main_proc.o oxygen.o hydrogen.o
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic -lpthread

all: $(PROJ2_TASKS)
	gcc $(PROJ2_TASKS) $(CFLAGS) -o proj2

proj2.o: proj2.c
	gcc proj2.c -c $(CFLAGS) -o proj2.o

main_proc.o: main_proc.c
	gcc main_proc.c -c $(CFLAGS) -o main_proc.o

oxygen.o: oxygen.c
	gcc oxygen.c -c $(CFLAGS) -o oxygen.o

hydrogen.o: hydrogen.c
	gcc hydrogen.c -c $(CFLAGS) -o hydrogen.o

clean:
	rm -rf *.o