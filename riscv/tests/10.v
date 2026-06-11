module SubModule(
	inout wire p,
	input wire x,
	input wire z,
	output reg y
);
	always @(y) begin
		y <= 1;
	end
endmodule

module test(
	inout wire a
);
	localparam B = 16;
	localparam A = 2;


	reg b__z;
	wire b__y;
	SubModule b(
		.p(a),
		.x(1'd1),
		.z(b__z),
		.y(b__y)
	);
	reg c;
	reg [2:0] d;
	reg [2:0] f [1:0];
	always @(d, f[0], b__z, c, d[2], f[0][2]) begin
		d <= 4;
		;
		f[0] <= 2;
		;
		b__z <= 16;
		c <= d[2];
		c <= f[0][2];
	end
endmodule

