module multiplex #(
    parameter ADDR_W = 64
)(
    input                    i_clk,
    input                    i_rst_n,
    input  [ADDR_W-1:0]      data_a,
    input  [ADDR_W-1:0]      data_b,
    input                    choosea0orb1,
    output [ADDR_W-1:0]      data_out
);
    reg [ADDR_W-1:0] output_reg;
    //wire choosela;
    assign data_out = output_reg;
    //assign choosela = choosea0orb1;
    always @(*) begin
        case(choosea0orb1)
            0:begin
                output_reg = data_a;
            end
            1:begin
                output_reg = data_b;
            end
        endcase
    end

    always @(posedge i_clk or negedge ~i_rst_n) begin
        if (~i_rst_n) begin
            output_reg = 0;
        end
    end

endmodule