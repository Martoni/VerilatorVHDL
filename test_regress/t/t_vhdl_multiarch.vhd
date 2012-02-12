-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_multiarch is
port(a: in std_logic;
     b: in std_logic;
	 c: out std_logic);
	
end t_vhdl_multiarch;  

architecture rtl1 of t_vhdl_multiarch is
begin
	c <= a;
end rtl1;

architecture rtl2 of t_vhdl_multiarch is
begin
	c <= b;
end rtl2;
