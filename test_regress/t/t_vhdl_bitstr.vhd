-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_bitstr is
port(F: out std_logic_vector (7 downto 0);
     G: out std_logic_vector (5 downto 0)
);
end t_vhdl_bitstr;

architecture behav2 of t_vhdl_bitstr is
begin
	F <= b"0010_0100";
	F <= B"00100100";
    F <= X"AB";
	F <= x"C_D";
    G <= o"77";
    G <= O"7_7";

end behav2;
