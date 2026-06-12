module SubModule(
	inout wire a
);
	reg _auto_a;
	assign a = _auto_a;
endmodule

module test(
	inout wire a,
	inout wire [2:0] b__a,
	inout wire [2:0] c_0__a__a,
	inout wire [2:0] c_1__a__a,
	output reg e,
	inout wire [3:0] f_0, inout wire [3:0] f_1
);
	reg _auto_a;
	assign a = _auto_a;
	reg _auto_b__a;
	assign b__a = _auto_b__a;
	reg _auto_c_0__a__a;
	assign c_0__a__a = _auto_c_0__a__a;
	reg _auto_c_1__a__a;
	assign c_1__a__a = _auto_c_1__a__a;
	reg [3:0] _auto_f_0;
	assign f = _auto_f_0;
	reg [3:0] _auto_f_1;
	assign f = _auto_f_1;

	SubModule d(
		.a(a)
	);
	always @(_auto_a, e, _auto_c_0__a__a, _auto_b__a, _auto_c_1__a__a, _auto_f_0) begin
		_auto_a <= 1;
		{ _auto_a, e, _auto_c_0__a__a } <= 1;
		_auto_b__a <= 1;
		_auto_c_1__a__a <= 1;
		_auto_f_0 <= 1;
	end
endmodule

