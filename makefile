SRC=myar.c

default: myar

myar: myar.c
	gcc -std=c99 -o myar myar.c
