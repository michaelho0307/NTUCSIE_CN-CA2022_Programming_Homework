module alu #(
    parameter DATA_WIDTH = 32,
    parameter INST_WIDTH = 5
)(
    input                   i_clk,
    input                   i_rst_n,
    input  [DATA_WIDTH-1:0] i_data_a,
    input  [DATA_WIDTH-1:0] i_data_b,
    input  [INST_WIDTH-1:0] i_inst,
    input                   i_valid,
    output [DATA_WIDTH-1:0] o_data,
    output                  o_overflow,
    output                  o_valid
);

// homework
    integer i;
    //wires and registers
    reg [DATA_WIDTH-1:0] o_data_register,o_data_wire;
    reg                  o_overflow_register,o_overflow_wire;
    reg                  o_valid_register,o_valid_wire;

    //some vars to calculate more easily
    reg [DATA_WIDTH-1:0] unsignedmulleft;
    wire signed [DATA_WIDTH-1:0] signed_a,signed_b;
    reg [DATA_WIDTH:0] signedaddsub_temp;
    reg signed [DATA_WIDTH*2-1:0] signedmul;
    //continuous assignment
    assign o_data = o_data_register;
    assign o_overflow = o_overflow_register;
    assign o_valid = o_valid_register;
    assign signed_a = i_data_a;
    assign signed_b = i_data_b;
    //combinational part
    always @(*) begin
        if (i_valid) begin
            case (i_inst)
                //signed add
                5'd0: begin
                    signedaddsub_temp = signed_a+signed_b;
                    o_valid_wire = 1;
                    if ((signedaddsub_temp[DATA_WIDTH] ^ signedaddsub_temp[DATA_WIDTH-1]) == 1)begin
                        o_overflow_wire = 1;
                        o_data_wire = 0;
                    end else begin
                        o_overflow_wire = 0;
                        o_data_wire = signed_a+signed_b;
                    end
                end
                //signed sub
                5'd1: begin
                    o_valid_wire = 1;
                    if ((signed_a < 0 && signed_b > 0) && (signed_a-signed_b > 0))begin
                        o_overflow_wire = 1;
                        o_data_wire = 0;
                    end else if ((signed_a > 0 && signed_b < 0) && (signed_a-signed_b < 0)) begin
                        o_overflow_wire = 1;
                        o_data_wire = 0;
                    end else begin
                        o_overflow_wire = 0;
                        o_data_wire = signed_a-signed_b;
                    end
                end
                //signed mul
                5'd2: begin
                    o_data_wire= signed_a*signed_b;
                    if (o_data_wire[DATA_WIDTH-1] != (signed_a[DATA_WIDTH-1]^signed_b[DATA_WIDTH-1]))begin
                        o_overflow_wire = 1;
                    end else begin
                        o_overflow_wire = 0;
                    end
                    o_valid_wire = 1;
                end
                //signed max
                5'd3: begin
                    o_data_wire = (signed_a >= signed_b)? signed_a : signed_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //signed min
                5'd4: begin
                    o_data_wire = (signed_a < signed_b)? signed_a : signed_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //unsigned add
                5'd5: begin
                    {o_overflow_wire,o_data_wire} = i_data_a+i_data_b;
                    o_valid_wire = 1;
                end
                //unsigned sub
                5'd6: begin
                    o_data_wire = i_data_a-i_data_b;
                    if (i_data_a >= i_data_b) begin  //can sub
                        o_overflow_wire = 0;
                    end
                    else begin //overflow
                        o_overflow_wire = 1;
                    end   
                    o_valid_wire = 1;
                end
                //unsigned mul
                5'd7: begin
                    {unsignedmulleft,o_data_wire} = i_data_a*i_data_b;
                    if (unsignedmulleft != 0) begin  //overflow
                        o_overflow_wire = 1;
                    end
                    else begin //save
                        o_overflow_wire = 0;
                    end
                    o_valid_wire = 1;
                end
                //unsigned max
                5'd8: begin
                    o_data_wire = (i_data_a >= i_data_b)? i_data_a : i_data_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //unsigned min
                5'd9: begin
                    o_data_wire = (i_data_a < i_data_b)? i_data_a : i_data_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //And
                5'd10: begin
                    o_data_wire = i_data_a & i_data_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //Or
                5'd11: begin
                    o_data_wire = i_data_a | i_data_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end 
                //Xor
                5'd12: begin
                    o_data_wire = i_data_a ^ i_data_b;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //BitFlip
                5'd13: begin
                    o_data_wire = ~i_data_a;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //BitReverse
                5'd14: begin
                    for (i = 0; i < DATA_WIDTH; i++) begin
                        o_data_wire[i] = i_data_a[DATA_WIDTH-1-i];
                    end
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //Signed LT
                5'd15: begin
                    o_data_wire = (signed_a < signed_b)? 1 : 0;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end
                //Signed GE
                5'd16: begin
                    o_data_wire = (signed_a >= signed_b)? 1 : 0;
                    o_overflow_wire = 0;
                    o_valid_wire = 1;
                end  
                default: begin
                    o_overflow_wire = 0;
                    o_data_wire = 0;
                    o_valid_wire = 1;
                end
            endcase
        end
        else begin
            o_overflow_wire = 0;
            o_data_wire = 0;
            o_valid_wire = 0;
        end
    end
    //sequential part
    always @(posedge i_clk or negedge i_rst_n) begin
        if (~i_rst_n) begin   //if reset = 0 then reset
            o_data_register <= 0;
            o_overflow_register <= 0;
            o_valid_register <= 0;
        end
        else begin
            o_data_register <= o_data_wire;
            o_overflow_register <= o_overflow_wire;
            o_valid_register <= o_valid_wire;
        end
    end

endmodule
