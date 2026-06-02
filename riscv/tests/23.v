module test(
	output reg a__a,
	input wire [1:0] a__b,
	input wire b__a,
	output reg [1:0] b__b,
	input wire c_0__a,
	output reg [1:0] c_0__b,
	input wire c_1__a,
	output reg [1:0] c_1__b
);
	always @(a__a, a__b[0], b__a, c_0__b[0], c_0__b[1], c_1__b[0]) begin
		a__a <= a__b[0];
		b__a <= c_0__b[0];
		b__a <= c_0__b[1];
		b__a <= c_0__b[0];
		b__a <= c_1__b[0];
	end
endmodule

