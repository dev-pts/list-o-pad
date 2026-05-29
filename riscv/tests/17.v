module SubModule(
	output reg [2:0] y
);
	localparam Z = 3;
endmodule

module SubModule_Z_2(
	output reg [1:0] y
);
	localparam Z = 2;
endmodule

module test();
	wire [2:0] a__y;
	SubModule a(
		.y(a__y)
	);
	wire [1:0] b__y;
	SubModule_Z_2 b(
		.y(b__y)
	);
	wire [2:0] c__y;
	SubModule c(
		.y(c__y)
	);
endmodule

