--begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

library work;
use work.vhdl_simhelper.all;

entity t_vhdl_report is
	port (clk: in std_logic);
end t_vhdl_report;

architecture behav2 of t_vhdl_report is
begin
	stimulus: process(clk)
	begin
		report "Hello World";
		finish;
	end process;
end behav2;
