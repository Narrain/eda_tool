module child (
    input  logic a,
    output logic y
);
    assign y = ~a;
endmodule

module top_with_child (
    input  logic x,
    output logic z
);
    child u1 (
        .a(x),
        .y(z)
    );
endmodule
