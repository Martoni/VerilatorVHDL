package simhelper is
begin
	procedure finish is
		report "finish" severity failure;
	end procedure finish;

	procedure stop is
		report "stop" severity failure;
	end procedure stop;	

end package simhelper;
