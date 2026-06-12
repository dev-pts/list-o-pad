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
	wire b__p;
	reg b__x;
	reg [1:0] b__z;
	wire [2:0] b__y;
	SubModule b(
		.p(b__p),
		.x(b__x),
		.z(b__z),
		.y(b__y)
	);
	reg [1:0] k;
	always @(k[0]) begin
		k[0] <= 2;
	end
endmodule

