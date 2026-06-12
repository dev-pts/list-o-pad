module test(
	input wire a,
	output reg b,
	inout wire c
);
	localparam A = 1;
	reg _auto_c;
	assign c = _auto_c;
	reg d;
	always @(b, d) begin
		b <= 1;
		if (d == 2) begin
			b <= 2;
		end else begin
			b <= 4;
		end
	end
	always @(posedge a) begin
		d <= 1;
	end
endmodule

