make:
	gcc -o main main.c
	
clean:
	rm -f main

run:
	clear
	make clean
	make
	./main test
