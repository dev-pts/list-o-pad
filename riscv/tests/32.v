module test(
	inout wire a,
	inout wire b__a,
	inout wire c__a__a
);
	reg _auto_a;
	assign a = _auto_a;
	reg _auto_b__a;
	assign b__a = _auto_b__a;
	reg _auto_c__a__a;
	assign c__a__a = _auto_c__a__a;
endmodule

