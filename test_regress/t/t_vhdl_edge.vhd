-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_edge is
port(clk: in std_logic;
     a: in std_logic;
     b: in std_logic;
     d: in std_logic;
     e: in std_logic;
     f: in std_logic;
	 c: out std_logic);
	
end t_vhdl_edge;  

architecture behav2 of t_vhdl_edge is
begin
	process (clk,b,d,e)
	begin
		if clk'event and clk='1' then
			c <= a;
		end if;

		if b'event and b='0' then
			c <= a;
		end if;

		if e'event and f='0' then
			c <= a;
		end if;

		if d'event then
			c <= a;
		end if;
	end process;

end behav2;
