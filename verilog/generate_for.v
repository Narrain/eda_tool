module top;
  reg clk = 0;
  reg [3:0] r;

  // generate-for just to exercise elaboration
  genvar i;
  for (i = 0; i < 4; i = i + 1) begin : genblk
    wire w;
    assign w = r[i];
  end

  // real activity
  always #5 clk = ~clk;

  initial begin
    r = 4'b0000;
    #10 r = 4'b1010;
    #10 r = 4'b0101;
    #10 $finish;
  end
endmodule
