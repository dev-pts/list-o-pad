#!/bin/sh

for i in tests/*.lop; do
	f=$(dirname $i)/$(basename $i .lop)
	python3 py-lop.py test $f.lop | cmp $f.v - || exit
	iverilog $f.v
done
