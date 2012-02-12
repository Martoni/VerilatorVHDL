-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_const is
port(F: out std_logic);
end t_vhdl_const;  

architecture behav2 of t_vhdl_const is
begin
    F <= '1';
    F <= '0';
    F <= 'X';
    F <= 'Z';
    F <= 'U';
    F <= 'H';
    F <= 'L';
    F <= '-';
end behav2;
