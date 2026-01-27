module and_or_mux (
    input  logic a,
    input  logic b,
    input  logic sel,
    output logic y
);

    logic and_ab;
    logic or_ab;

    assign and_ab = a & b;
    assign or_ab  = a | b;

    assign y = sel ? or_ab : and_ab;

endmodule
