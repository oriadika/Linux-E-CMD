.SILENT: link compile clean run rm_o

all: clean compile link rm_o

rm_o:
	rm -f *.o

link:
	gcc -g -m32 -Wall -o out myshell.o LineParser.o
	gcc -g -m32 -Wall -o pipe mypipeline.o

compile:
	gcc -g -m32 -Wall -c -o myshell.o myshell.c
	gcc -g -m32 -Wall -c -o mypipeline.o mypipeline.c
	gcc -g -m32 -Wall -c -o LineParser.o LineParser.c

clean:
	rm -f *.o out pipe

run:
	out -d