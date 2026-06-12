module test(
	input wire clk,
	output reg [31:0] apb_0__addr,
	output reg apb_0__sel,
	output reg apb_0__enable,
	output reg apb_0__write,
	input wire apb_0__ready,
	output reg [31:0] apb_0__wdata,
	input wire [31:0] apb_0__rdata,
	output reg [3:0] apb_0__strb,
	output reg [31:0] apb_1__addr,
	output reg apb_1__sel,
	output reg apb_1__enable,
	output reg apb_1__write,
	input wire apb_1__ready,
	output reg [31:0] apb_1__wdata,
	input wire [31:0] apb_1__rdata,
	output reg [3:0] apb_1__strb,
	input wire [31:0] apb2_0__addr,
	input wire apb2_0__sel,
	input wire apb2_0__enable,
	input wire apb2_0__write,
	output reg apb2_0__ready,
	input wire [31:0] apb2_0__wdata,
	output reg [31:0] apb2_0__rdata,
	input wire [3:0] apb2_0__strb,
	input wire [31:0] apb2_1__addr,
	input wire apb2_1__sel,
	input wire apb2_1__enable,
	input wire apb2_1__write,
	output reg apb2_1__ready,
	input wire [31:0] apb2_1__wdata,
	output reg [31:0] apb2_1__rdata,
	input wire [3:0] apb2_1__strb
);
	reg some;
	reg [1:0] some2;
	reg [1:0] some3 [2:0];
	always @(posedge clk) begin
		apb2_1__rdata <= 0;
		apb2_1__ready <= 0;
		some <= apb_0__ready;
		some2 <= some3[0];
		some2[0] <= apb_0__ready;
		some3[1] <= some;
		some2[0] <= apb_0__ready;
		some3[1] <= some;
		some2[1] <= apb_0__ready;
		some3[1] <= some;
		some2[0] <= apb_0__ready;
		some3[0] <= some;
		some2[0] <= apb_0__ready;
		some3[1] <= some;
		some2[0] <= apb_0__ready;
		some3[2] <= some;
		some2[0] <= apb_0__ready;
		some3[0][0] <= some;
		some2[0] <= apb_0__ready;
		some3[0][1] <= some;
		if ((apb2_0__sel & apb2_0__enable) & (~apb2_0__ready)) begin
			apb2_0__ready <= 1;
		end
	end
endmodule

