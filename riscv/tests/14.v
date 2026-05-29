module SubModule(
	inout wire p,
	input wire x,
	input wire [1:0] z,
	output reg [2:0] y
);
	always @(y) begin
		y <= 1;
	end
endmodule

module test();
	localparam B = 16;
	localparam A = 2;
	wire b__0__p;
	reg [1:0] b__0__z;
	wire [2:0] b__0__y;
	SubModule b__0(
		.p(b__0__p),
		.x(1'd1),
		.z(b__0__z),
		.y(b__0__y)
	);
	wire b__1__p;
	reg [1:0] b__1__z;
	wire [2:0] b__1__y;
	SubModule b__1(
		.p(b__1__p),
		.x(1'd1),
		.z(b__1__z),
		.y(b__1__y)
	);
	reg [1:0] k [2:0];
	always @(b__0__z) begin
		b__0__z <= 16;
	end
endmodule

