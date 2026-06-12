module SubModule(
	inout wire p,
	input wire x,
	input wire [1:0] z,
	output reg [2:0] y
);
	reg _auto_p;
	assign p = _auto_p;
	always @(y) begin
		y <= 1;
	end
endmodule

module test();
	localparam B = 16;
	localparam A = 2;


	reg [1:0] b_0__z;
	wire [2:0] b_0__y;
	SubModule b_0(
		.p(k[0][1]),
		.x(1'd1),
		.z(b_0__z),
		.y(b_0__y)
	);


	reg [1:0] b_1__z;
	wire [2:0] b_1__y;
	SubModule b_1(
		.p(k[0][1]),
		.x(1'd1),
		.z(b_1__z),
		.y(b_1__y)
	);
	wire [1:0] k [2:0];
	always @(b_0__z) begin
		b_0__z <= 16;
	end
endmodule

