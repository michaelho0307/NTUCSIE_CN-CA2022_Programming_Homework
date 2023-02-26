module regist #(
    parameter ADDR_W = 64
)(
    input                    i_clk,
    input                    i_rst_n,
    input  [4:0]             read_reg1,
    input  [4:0]             read_reg2,
    input  [4:0]             write_reg,
    input  [ADDR_W-1:0]      writedata,
    input                    writedatasignal,
    output [ADDR_W-1:0]      read_data1,
    output [ADDR_W-1:0]      read_data2
);
integer i;
reg [ADDR_W-1:0] Myregister [31:0];
reg [ADDR_W-1:0] read_data1_reg, read_data2_reg;
assign read_data1 = read_data1_reg;
assign read_data2 = read_data2_reg;

always @(*) begin
    Myregister[0] = 0;
    read_data1_reg = Myregister[read_reg1];
    read_data2_reg = Myregister[read_reg2];
end

always @(posedge i_clk or negedge i_rst_n)begin
    if (~i_rst_n) begin
        for (i = 0; i < 32; i++) Myregister[i] = 0;
        read_data1_reg = 0;
        read_data2_reg = 0;
    end else if (writedatasignal) begin
        Myregister[write_reg] = writedata;
    end
end
endmodule