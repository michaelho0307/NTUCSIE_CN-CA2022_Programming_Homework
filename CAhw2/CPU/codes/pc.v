module pc #(
    parameter ADDR_W = 64
)(
    input                   i_clk,
    input                   i_rst_n,
    input [ ADDR_W-1 : 0 ]  input_addr,
    output [ ADDR_W-1 : 0 ] output_addr,
    output                  is_validtime
);

reg [4:0] ticktime; 
reg [ ADDR_W-1 : 0 ]  input_addr_reg,output_addr_reg;
reg is_validtime_reg,is_validtime_wire;


assign is_validtime = is_validtime_reg;
assign output_addr = output_addr_reg;
always @(*) begin
    input_addr_reg = input_addr;
end

always @(posedge i_clk or negedge i_rst_n) begin
    if (~i_rst_n)begin
        ticktime = 0;
        output_addr_reg = 0;
        is_validtime_wire = 0;
    end else begin
        ticktime = ticktime+1;
        if (ticktime == 15)begin
            output_addr_reg=input_addr_reg;
            is_validtime_wire = 1;
            ticktime = 0;
        end else begin
            is_validtime_wire = 0;
        end
        is_validtime_reg <= is_validtime_wire;
    end
end

endmodule