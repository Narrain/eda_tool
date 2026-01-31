module top;

  reg clk;
  reg [3:0] r;
  wire [3:0] w;

  assign w = r;

  always begin
    #5 clk = 1;
    #5 clk = 0;
  end

  genvar i;
  generate
    for (i = 0; i < 4; i = i + 1) begin : gen_blk
      always @(posedge clk) begin
        r[i] <= ~r[i];
      end
    end
  endgenerate

  initial begin
    r = 4'b0000;
    #50;
    $finish;
  end

endmodule
