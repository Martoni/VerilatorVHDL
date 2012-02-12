-- begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_counter is
port(clk: in std_logic;
     en: in std_logic;
	 count: out std_logic_vector (3 downto 0));
	
end t_vhdl_counter;  

architecture behav2 of t_vhdl_counter is
	signal counter_value: std_logic_vector (3 downto 0);
begin
	process (clk)
	begin
		if clk'event and clk='1' then
			if en = '1' then
				counter_value <= counter_value + 1;
			end if;
		end if;
	end process;

	count <= counter_value;

end behav2;
