all:
	ctypesgen -L../../ -llop ../../include/*.h -o LOP.py
	LD_LIBRARY_PATH=../../ python3 py-lop.py > result.v
	cat result.v

clean:
	rm -f LOP.py
	rm -f result.v
	rm -fr __pycache__
