--begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

library work;
use work.vhdl_simhelper.all;

entity t_vhdl_EXAMPLE is
	port (clk: in std_logic);
end t_vhdl_EXAMPLE;

architecture behav2 of t_vhdl_EXAMPLE is
begin
	stimulus: process(clk)
	begin

	end process;
	
	work: process(clk)
	begin

	end process;

	check: process(clk)
	begin

	end process;
end behav2;
