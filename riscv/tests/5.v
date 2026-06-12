module test(
	input wire a,
	output reg b,
	inout wire c
);
	localparam A = 1;
	reg _auto_c;
	assign c = _auto_c;
endmodule

