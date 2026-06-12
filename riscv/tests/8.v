module test(
	input wire a,
	output reg b,
	inout wire c
);
	localparam A = 1;
	reg _auto_c;
	assign c = _auto_c;
	reg d;
	always @(b) begin
		b <= 1;
	end
	always @(posedge a) begin
		d <= 1;
	end
endmodule

