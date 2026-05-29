module test(
	input wire a,
	output reg b,
	inout wire c
);
	localparam A = 1;
	reg d;
	always @(b) begin
		b <= 1;
	end
	always @(posedge a) begin
		d <= 1;
	end
endmodule

