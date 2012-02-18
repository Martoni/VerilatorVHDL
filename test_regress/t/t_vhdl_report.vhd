--begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

library work;
use work.vhdl_simhelper.all;

entity t_vhdl_report is
end t_vhdl_report;

architecture behav2 of t_vhdl_report is
begin
	stimulus: process
	begin
		report "Finished";
		finish;
	end process;
end behav2;
