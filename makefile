CC=gcc
FLAGS=-Wall -Wextra -pedantic -std=c11 -Wswitch-enum
LIB=-lncursesw -lpanelw

all:
	$(CC) taskminal.c -o taskminal $(FLAGS) $(LIB)
	./taskminal
build:
	$(CC) taskminal.c -o taskminal $(FLAGS) $(LIB)
