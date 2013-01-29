SRC=myar.c

default: myar

myar: myar.c
	gcc -o myar myar.c
