-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_concat is
port(a: in std_logic;
     b: in std_logic;
     outp: out std_logic_vector (1 downto 0));

end t_vhdl_concat;

architecture behav2 of t_vhdl_concat is
begin
	outp <= a & b;
end behav2;
