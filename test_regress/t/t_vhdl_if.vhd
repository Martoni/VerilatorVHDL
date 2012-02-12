-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_if is
port(a: in std_logic;
     b: in std_logic;
	 H: out std_logic;
	 F: out std_logic;
     G: out std_logic);
	
end t_vhdl_if;  

architecture behav2 of t_vhdl_if is
begin
	process (a, b)
	begin
		if a = '1' and b = '1' then
			H <= '1';
        elsif a = '1' and b = '0' then
            H <= '0';
		elsif a = '0' and b = '1' then
			H <= '1';
		elsif a = '1' and b = '1' then
			H <= '1';
		else 
			H <= '0';
		end if;

		if a = '1' then
			F <= '1';
		elsif b = '1' then
			F <= '1';
		else
			F <= '0';
		end if;

		if a = '1' then
			G <= b;
		else
			G <= '0';
		end if;

		if b='1' then
			G <= a;
		end if;
	end process;
end behav2;
