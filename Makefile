all:
	cmake -Bbuild -GNinja -DCMAKE_CXX_COMPILER=g++
	cmake --build build
	cp build/compile_commands.json .

run: all
	./build/engine

test: all
	@for test in build/test_*; do \
		$$test; \
	done
