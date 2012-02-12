-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_inst1 is
port(clk: in std_logic;
	 count: out std_logic_vector (3 downto 0));
	
end t_vhdl_inst1;  

architecture behav2 of t_vhdl_inst1 is
	signal enable: std_logic;
begin
	enable <= '1';
	cnt: t_vhdl_counter port map (clk, enable, count);
end behav2;
