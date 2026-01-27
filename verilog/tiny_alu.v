module tiny_alu (
    input  logic a,
    input  logic b,
    input  logic [1:0] op,
    output logic y
);

    logic and_ab;
    logic or_ab;
    logic xor_ab;

    assign and_ab = a & b;
    assign or_ab  = a | b;
    assign xor_ab = a ^ b;

    always_comb begin
        case (op)
            2'b00: y = and_ab;
            2'b01: y = or_ab;
            2'b10: y = xor_ab;
            default: y = 1'b0;
        endcase
    end

endmodule
