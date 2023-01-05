make:
	gcc -o main main.c
	
clean:
	rm -f main

run:
	# rm -rf test
	clear
	make clean
	make
	# ./main test
	./main
