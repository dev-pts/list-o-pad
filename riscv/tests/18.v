module test();
	reg a__CLKIN;
	reg a__CLKFB;
	reg a__RST;
	reg a__PSEN;
	reg a__PSINCDEC;
	reg a__PSCLK;
	wire a__CLK0;
	wire a__CLK90;
	wire a__CLK180;
	wire a__CLK270;
	wire a__CLK2X;
	wire a__CLK2X180;
	wire a__CLKDV;
	wire a__CLKFX;
	wire a__CLKFX180;
	wire a__LOCKED;
	wire a__PSDONE;
	DCM_SP #(
		.CLKIN_PERIOD(20),
		.CLKFX_MULTIPLY(24),
		.CLKFX_DIVIDE(25)
	) a(
		.CLKIN(a__CLKIN),
		.CLKFB(a__CLKFB),
		.RST(a__RST),
		.PSEN(a__PSEN),
		.PSINCDEC(a__PSINCDEC),
		.PSCLK(a__PSCLK),
		.CLK0(a__CLK0),
		.CLK90(a__CLK90),
		.CLK180(a__CLK180),
		.CLK270(a__CLK270),
		.CLK2X(a__CLK2X),
		.CLK2X180(a__CLK2X180),
		.CLKDV(a__CLKDV),
		.CLKFX(a__CLKFX),
		.CLKFX180(a__CLKFX180),
		.LOCKED(a__LOCKED),
		.PSDONE(a__PSDONE)
	);
endmodule

