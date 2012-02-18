-- VHDL Simulation helper for GHDL, unsupported constructs in VHDL93 that
-- are helpful to test Verilator

package vhdl_simhelper is
	procedure finish;
	procedure stop;
end package vhdl_simhelper;

package body vhdl_simhelper is
	procedure finish is
	begin
		report "*-* All Finished *-*" severity failure;
	end procedure finish;

	procedure stop is
	begin
		report "!!! Finished with failure !!!" severity failure;
	end procedure stop;	
end package body vhdl_simhelper;

