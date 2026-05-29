module SubModule(
	output reg [2:0] y,
	input wire z
);
	localparam Z = 3;
endmodule

module SubModule_Z_2(
	output reg [1:0] y,
	input wire z
);
	localparam Z = 2;
endmodule

module test();
	wire [2:0] a__y;
	reg a__z;
	SubModule a(
		.y(a__y),
		.z(a__z)
	);
	wire [1:0] b__y;
	reg b__z;
	SubModule_Z_2 b(
		.y(b__y),
		.z(b__z)
	);
	wire [2:0] c__y;
	SubModule c(
		.y(c__y),
		.z(1'h0)
	);
endmodule

