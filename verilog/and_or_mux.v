module and_or_mux(output y);
  wire a;
  wire b;
  wire sel;
  wire and_ab;
  wire or_ab;

  assign a      = 1'b0;
  assign b      = 1'b1;
  assign sel    = 1'b1;
  assign and_ab = a & b;
  assign or_ab  = a | b;
  assign y      = sel ? or_ab : and_ab;
endmodule
