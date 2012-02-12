-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_comment is
port(F: out std_logic);
end t_vhdl_comment;

-- Test comment here
 -- other comment
architecture behav2 of t_vhdl_comment is
begin--comment
    F <= '0'; -- other comment here
   -- F <= 'L';
end behav2;
