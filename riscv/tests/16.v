module SubModule(
	input wire [2:0] y
);
	localparam Z = 3;
endmodule

module test();
	reg [2:0] c__y;
	SubModule c(
		.y(c__y)
	);
	always @(c__y[0], c__y[1], c__y[2]) begin
		c__y[0] <= 1;
		c__y[1] <= 1;
		c__y[2] <= 1;
	end
endmodule

