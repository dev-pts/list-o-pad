module test();
	localparam INIT_FILE = "mem.hex";
	reg [7:0] mem [255:0];
	reg dummy;
	reg dummy2;
	initial begin
		$readmemh("mem.hex", mem);
	end
	always @(dummy, dummy2) begin
		dummy <= $signed(dummy2);
	end
	always @(posedge dummy or negedge dummy) begin
		dummy <= $signed(dummy) + 3;
	end
endmodule

