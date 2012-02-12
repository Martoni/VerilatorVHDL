-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_concurr_asgn is
port(	x: in std_logic;
	y: in std_logic;
	Ao: out std_logic;
	Oo: out std_logic;
	Xo: out std_logic;
	No: out std_logic
);
end t_vhdl_concurr_asgn;

architecture behav2 of t_vhdl_concurr_asgn is
begin
    Ao <= x and y;
    Oo <= x or y;
    Xo <= x xor y;
    No <= not x;
end behav2;
