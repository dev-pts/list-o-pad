module SubModule(
	output reg e__a,
	input wire [1:0] e__b,
	input wire f__a,
	output reg [1:0] f__b,
	output reg g_0__a,
	input wire [1:0] g_0__b,
	output reg g_1__a,
	input wire [1:0] g_1__b,
	input wire h_0__a,
	output reg [1:0] h_0__b,
	input wire h_1__a,
	output reg [1:0] h_1__b,
	output reg i,
	output reg [2:0] j_0, output reg [2:0] j_1
);
endmodule

module test(
	output reg a__a,
	input wire [1:0] a__b,
	input wire b__a,
	output reg [1:0] b__b,
	input wire c_0__a,
	output reg [1:0] c_0__b,
	input wire c_1__a,
	output reg [1:0] c_1__b,
	output reg k,
	output reg [2:0] l_0, output reg [2:0] l_1
);
	wire d__e__a;
	reg [1:0] d__e__b;

	reg d__f__a;
	wire [1:0] d__f__b;

	wire d__g_0__a;
	reg [1:0] d__g_0__b;
	wire d__g_1__a;
	reg [1:0] d__g_1__b;

	reg d__h_0__a;
	wire [1:0] d__h_0__b;
	reg d__h_1__a;
	wire [1:0] d__h_1__b;

	wire d__i;
	wire [2:0] d__j_0;wire [2:0] d__j_1;
	SubModule d(
		.e__a(d__e__a),
		.e__b(d__e__b),
		.f__a(d__f__a),
		.f__b(d__f__b),
		.g_0__a(d__g_0__a),
		.g_0__b(d__g_0__b),
		.g_1__a(d__g_1__a),
		.g_1__b(d__g_1__b),
		.h_0__a(d__h_0__a),
		.h_0__b(d__h_0__b),
		.h_1__a(d__h_1__a),
		.h_1__b(d__h_1__b),
		.i(d__i),
		.j_0(d__j_0),
		.j_1(d__j_1)
	);
	wire e_0__e__a;
	reg [1:0] e_0__e__b;

	reg e_0__f__a;
	wire [1:0] e_0__f__b;

	wire e_0__g_0__a;
	reg [1:0] e_0__g_0__b;
	wire e_0__g_1__a;
	reg [1:0] e_0__g_1__b;

	reg e_0__h_0__a;
	wire [1:0] e_0__h_0__b;
	reg e_0__h_1__a;
	wire [1:0] e_0__h_1__b;

	wire e_0__i;
	wire [2:0] e_0__j_0;wire [2:0] e_0__j_1;
	SubModule e_0(
		.e__a(e_0__e__a),
		.e__b(e_0__e__b),
		.f__a(e_0__f__a),
		.f__b(e_0__f__b),
		.g_0__a(e_0__g_0__a),
		.g_0__b(e_0__g_0__b),
		.g_1__a(e_0__g_1__a),
		.g_1__b(e_0__g_1__b),
		.h_0__a(e_0__h_0__a),
		.h_0__b(e_0__h_0__b),
		.h_1__a(e_0__h_1__a),
		.h_1__b(e_0__h_1__b),
		.i(e_0__i),
		.j_0(e_0__j_0),
		.j_1(e_0__j_1)
	);
	wire e_1__e__a;
	reg [1:0] e_1__e__b;

	reg e_1__f__a;
	wire [1:0] e_1__f__b;

	wire e_1__g_0__a;
	reg [1:0] e_1__g_0__b;
	wire e_1__g_1__a;
	reg [1:0] e_1__g_1__b;

	reg e_1__h_0__a;
	wire [1:0] e_1__h_0__b;
	reg e_1__h_1__a;
	wire [1:0] e_1__h_1__b;

	wire e_1__i;
	wire [2:0] e_1__j_0;wire [2:0] e_1__j_1;
	SubModule e_1(
		.e__a(e_1__e__a),
		.e__b(e_1__e__b),
		.f__a(e_1__f__a),
		.f__b(e_1__f__b),
		.g_0__a(e_1__g_0__a),
		.g_0__b(e_1__g_0__b),
		.g_1__a(e_1__g_1__a),
		.g_1__b(e_1__g_1__b),
		.h_0__a(e_1__h_0__a),
		.h_0__b(e_1__h_0__b),
		.h_1__a(e_1__h_1__a),
		.h_1__b(e_1__h_1__b),
		.i(e_1__i),
		.j_0(e_1__j_0),
		.j_1(e_1__j_1)
	);
	always @(a__a, a__b[0], c_0__b[0], b__a, c_0__b[1], c_1__b[0], b__b[0], c_0__a, b__b[1], c_1__a, c_0__b, a__b, c_1__b, b__b, d__f__b, d__f__a, d__e__b, d__e__a, e_0__e__b, e_1__f__b, e_1__f__a, e_0__e__a, e_0__g_0__b, e_1__h_1__b, e_1__h_1__a, e_0__g_0__a) begin
		a__a <= a__b[0];
		c_0__b[0] <= b__a;
		c_0__b[1] <= b__a;
		c_0__b[0] <= b__a;
		c_1__b[0] <= b__a;
		b__b[0] <= c_0__a;
		b__b[1] <= c_0__a;
		b__b[0] <= c_0__a;
		b__b[1] <= c_1__a;
		a__a <= c_0__a;
		c_0__b <= a__b;
		a__a <= c_1__a;
		c_1__b <= a__b;
		b__b <= d__f__b;
		d__f__a <= b__a;
		d__e__b <= d__f__b;
		d__f__a <= d__e__a;
		d__f__a <= d__e__a;
		d__e__b <= d__f__b;
		e_0__e__b <= e_1__f__b;
		e_1__f__a <= e_0__e__a;
		e_0__g_0__b <= e_1__h_1__b;
		e_1__h_1__a <= e_0__g_0__a;
	end
endmodule

