build:
	gcc -o puppol.buildrooms puppol.buildrooms.c -g
	gcc -o puppol.adventure puppol.adventure.c -lpthread -g
clean:
	rm -rf puppol.buildrooms puppol.rooms.* puppol.adventure
run:
	puppol.buildrooms && puppol.adventure
gdb:
	puppol.buildroooms && gdb puppol.adventure
valgrind:
	puppol.buildroooms && valgrind puppol.adventure
