
test : rgbe.o Source.c
	gcc -g rgbe.o Source.c -Wall -o test -lSDL2 -lGL -lm -lpthread

rgbe.o : rgbe.c rgbe.h
	gcc -c -g rgbe.c rgbe.h