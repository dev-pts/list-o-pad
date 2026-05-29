module SubModule_Z_6_B2_5(
	inout wire p,
	input wire x,
	input wire [4:0] z,
	output reg [5:0] y
);
	localparam Z = 6;
	localparam B2 = 5;
	always @(y) begin
		y <= 1;
	end
endmodule

module test();
	localparam B = 16;
	localparam A = 2;
	wire b__0__p;
	reg b__0__x;
	reg [4:0] b__0__z;
	wire [5:0] b__0__y;
	SubModule_Z_6_B2_5 b__0(
		.p(b__0__p),
		.x(b__0__x),
		.z(b__0__z),
		.y(b__0__y)
	);
	wire b__1__p;
	reg b__1__x;
	reg [4:0] b__1__z;
	wire [5:0] b__1__y;
	SubModule_Z_6_B2_5 b__1(
		.p(b__1__p),
		.x(b__1__x),
		.z(b__1__z),
		.y(b__1__y)
	);
endmodule

