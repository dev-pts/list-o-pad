all:
	ctypesgen -L../ -llop ../include/*.h -o LOP.py
	python3 py-lop.py

tb: tb.cpp a.v
	../../verilator/bin/verilator --no-timing --x-initial-edge --cc --trace -Wno-lint --exe --build -O3 --x-assign fast --x-initial fast --noassert -Wno-fatal --top HZ \
		--build-jobs $(shell nproc) \
		$^

clean:
	rm -f LOP.py
	rm -fr __pycache__
