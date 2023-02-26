module adder_4orsum # (
    parameter ADDR_W = 64
)(
    input                   i_clk,
    input                   i_rst_n,
    input [ADDR_W-1:0]      data_a,
    input [ADDR_W-1:0]      data_b,
    output [ADDR_W-1:0]     data_sum
);
reg [ADDR_W-1:0] data_sum_reg;

assign data_sum = data_sum_reg;

always @(*) begin
    data_sum_reg = data_a+data_b;
end

// always@(posedge i_clk or negedge i_rst_n)begin
//     if (~i_rst_n) begin
//         data_sum_reg = 0;
//     end
// end
endmodule