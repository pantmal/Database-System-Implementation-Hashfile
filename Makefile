hp:
	@echo " Compile ht_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/ht_main.c ./src/hash_file.c -lbf -o ./build/runner -O2
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main_one_file.c ./src/hash_file.c -lbf -o ./build/runner2 -O2
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main_multiple_files.c ./src/hash_file.c -lbf -o ./build/runner3 -O2


bf:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c -lbf -o ./build/runner -O2