--begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_assert is
end t_vhdl_assert;

architecture behav2 of t_vhdl_assert is
procedure finish is 
begin
	report "finsh" severity failure;
end procedure finish;

procedure stop is
begin
	report "stop" severity failure;
end procedure stop;

begin
	stimulus: process
	begin
		report "Finished";
		finish;
	end process;
end behav2;
