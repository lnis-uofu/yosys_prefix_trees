module demo(cout, sum, a, b, c, d, cin, test);

    input  [15:0] a, b;
    input  [ 7:0] c, d;
    input cin;
    output cout;
    output [15:0] sum;
    output [ 8:0] test;

    assign {cout,sum} = a + (* pptrees_alu, pptrees_base="ripple-carry" *) b + cin;

    assign test = c + (* pptrees_alu, pptrees_base="brent-kung" *) d + cin;

endmodule
