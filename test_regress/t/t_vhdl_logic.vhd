--begin_keywords "1076-1993"
library ieee;
use ieee.std_logic_1164.all;

library work;
use work.vhdl_simhelper.all;

entity t_vhdl_logic is
	port (clk: in std_logic);
end t_vhdl_logic;

architecture behav2 of t_vhdl_logic is
	signal a, b: std_logic;
	signal oand, oor, onand, onor, oxor, oxnor: std_logic;
begin
	stimulus: process(clk)
		variable count: integer := 0;
	begin
		if rising_edge (clk) then
			case (count) is 
				when 0 =>
					a <= '0';	
					b <= '0';	

				when 1 =>
					a <= '0';	
					b <= '1';	

				when 2 =>
					a <= '1';	
					b <= '0';	

				when 3 =>
					a <= '1';	
					b <= '1';	

				when 4 =>
					finish;

				when others =>
					null;

			end case;
			count := count + 1;
		end if;

	end process;

	work: process(a,b)
	begin
		oand <= a and b;
		oor <= a or b;
		onand <= a nand b;
		onor <= a nor b;
		oxor <= a xor b;
		oxnor <= a xnor b;
	end process;
	
end behav2;
