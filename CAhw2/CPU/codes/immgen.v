module immgen #(
    parameter ADDR_W = 64
)(
    input                   i_clk,
    input                   i_rst_n,
    input  [31:0]           inputinstruction,
    output [ADDR_W-1:0]     outputdata
);

reg [ADDR_W-1:0]     outputdata_reg;
assign outputdata = outputdata_reg;

always @(*) begin
    outputdata_reg = 0;
    if (inputinstruction[6:0] != 7'b0110011)begin
        case(inputinstruction[6:5])
            //load and immediate
            2'b00:begin
                outputdata_reg[11:0] = inputinstruction[31:20];
            end
            //store
            2'b01:begin
                outputdata_reg[4:0] = inputinstruction[11:7];
                outputdata_reg[11:5] = inputinstruction[31:25];
            end
            //bne beq
            //already left shift!!! dont need to shift again
            2'b11:begin
                outputdata_reg[11] = inputinstruction[7];
                outputdata_reg[4:1] = inputinstruction[11:8];
                outputdata_reg[12] = inputinstruction[31];
                outputdata_reg[10:5] = inputinstruction[30:25];
            end
        endcase
    end
end


endmodule