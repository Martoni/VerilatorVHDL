// This file ONLY is placed into the Public Domain, for any use,
// without warranty, 2008 by Lane Brooks

module top (input A, input B, input SEL, output Y1, output Y2, output Z);
   io   io1(.A(A), .OE( SEL), .Z(Z), .Y(Y1));
   pass io2(.A(B), .OE(!SEL), .Z(Z), .Y(Y2));
endmodule

module pass (input A, input OE, inout Z, output Y);
   io io(.A(A), .OE(OE), .Z(Z), .Y(Y));
endmodule

module io (input A, input OE, inout Z, output Y);
   assign Z = (OE) ? A : 1'bz;
   assign Y = Z;
endmodule
