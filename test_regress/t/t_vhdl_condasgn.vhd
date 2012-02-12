-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_condasgn is
port(	x: in std_logic;
	y: in std_logic;
	z: in std_logic;
	F: out std_logic;
	G: out std_logic
);
end t_vhdl_condasgn;

architecture behav2 of t_vhdl_condasgn is
begin
    F <= x when y = 1 else
	 y when 0 = 0 else
	 z when 1 = 1
	else 0;
    G <= x when z = 1 else y;
end behav2;
