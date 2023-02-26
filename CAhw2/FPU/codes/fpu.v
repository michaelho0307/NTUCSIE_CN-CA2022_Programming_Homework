module fpu #(
    parameter DATA_WIDTH = 32,
    parameter INST_WIDTH = 1
)(
    input                   i_clk,
    input                   i_rst_n,
    input  [DATA_WIDTH-1:0] i_data_a,
    input  [DATA_WIDTH-1:0] i_data_b,
    input  [INST_WIDTH-1:0] i_inst,
    input                   i_valid,
    output [DATA_WIDTH-1:0] o_data,
    output                  o_valid
);

// homework
    integer i;
    //wires and registers
    reg [DATA_WIDTH-1:0] o_data_register,o_data_wire;
    reg                  o_valid_register,o_valid_wire;
    //some vars to calculate more easily
    localparam Sbit = DATA_WIDTH-1;          //31   31
    localparam Exp_highbit = DATA_WIDTH-2;   //30   23-30
    localparam Frac_highbit = DATA_WIDTH-10; //22   0-22
    //some vars to calculate more easily for multi
    reg [Exp_highbit-Frac_highbit-1:0] mul_exp_temp;
    reg [Frac_highbit*2+3:0] mul_frac_temp; //0-47
    reg [Frac_highbit*2+3:0] roundbit; //0-47
    reg [22:0] temp_mul; //0-22
    //some vars to calculate more easily for add
    reg [303:0] temp;
    reg signed [303:0] signed_a,signed_b; //2 before dots
    reg signed [303:0] sum_signed;
    reg [Exp_highbit-Frac_highbit-1:0] diff;
    reg [303:0] rounded_sum;
    reg [22:0] result;
    //continuous assignment
    assign o_data = o_data_register;
    assign o_valid = o_valid_register;

    //combinational part
    always @(*) begin
        if (i_valid) begin
            case (i_inst)
                //ADD
                1'd0: begin
                    //calculate signed frac a and b
                    if (i_data_a[31])begin
                        temp = 0;
                        temp[300:277] = {2'b1,i_data_a[Frac_highbit:0]};
                        signed_a = (~temp)+1;
                    end else begin
                        signed_a = 0;
                        signed_a[300:277] = {2'b1,i_data_a[Frac_highbit:0]}; 
                    end
                    if (i_data_b[31])begin
                        temp = 0;
                        temp[300:277] = {2'b1,i_data_b[Frac_highbit:0]};
                        signed_b = (~temp)+1;
                    end else begin
                        signed_b = 0;
                        signed_b[300:277] = {2'b1,i_data_b[Frac_highbit:0]}; 
                    end
                    // calculate diff
                    if (i_data_a[30:23] >= i_data_b[30:23]) begin
                        diff = i_data_a[30:23]-i_data_b[30:23];  //a is larger
                        o_data_wire[30:23] = i_data_a[30:23];
                        //shift bits
                        for (i =0; i < diff; i++) begin
                            signed_b >>>= 1;
                        end
                    end else begin
                        diff = i_data_b[30:23]-i_data_a[30:23];  //b is larger
                        o_data_wire[30:23] = i_data_b[30:23];
                        //shift bits
                        for (i =0; i < diff; i++) begin
                            signed_a >>>= 1;
                        end
                    end
                    // signed_add & decide signbit
                    sum_signed = signed_a+signed_b;
                    if (sum_signed[303]) begin
                        o_data_wire[31] = 1;
                        rounded_sum = ~(sum_signed)+1;
                    end else begin
                        o_data_wire[31] = 0;
                        rounded_sum = sum_signed;
                    end
                    // shift bits
                    if (rounded_sum[301] == 1)begin  //if 11.blablabla 10.blablabla
                        rounded_sum >>= 1;
                        o_data_wire[30:23] += 1;
                    end
                    if (rounded_sum != 0)begin
                        while (rounded_sum[300] != 1) begin
                            rounded_sum <<= 1;
                            o_data_wire[30:23] -= 1;
                        end
                    end
                    // round to the nearest even
                    if (rounded_sum[276] == 1 && rounded_sum[275:0] > 0)begin
                        result = rounded_sum[299:277];
                        result = result +1;
                        o_data_wire[22:0] = result;
                    end else begin
                        o_data_wire[22:0] = rounded_sum[299:277];
                    end
                    o_valid_wire = 1;
                end
                //MUL
                1'd1: begin
                    //calculate sign bit
                    o_data_wire[31] = (i_data_a[31] ^ i_data_b[31]);
                    //calculate exp bit
                    mul_exp_temp = i_data_a[Exp_highbit:Frac_highbit+1]+i_data_b[Exp_highbit:Frac_highbit+1] - 8'd127;
                    //mul_exp_temp = mul_exp_temp - 8'd127;
                    //calculate frac bit
                    mul_frac_temp = {1'b1,i_data_a[Frac_highbit:0]}*{1'b1,i_data_b[Frac_highbit:0]};
                    if (mul_frac_temp[47] == 1)begin  //if 11.blablabla 10.blablabla
                        mul_frac_temp >>= 1;
                        mul_exp_temp += 1;
                    end
                    while (mul_frac_temp[46] != 1) begin
                        mul_exp_temp -= 1;
                        mul_frac_temp <<= 1;
                    end
                    //round to the nearest even
                    if (mul_frac_temp[22] == 1 && mul_frac_temp[21:0] > 0)begin
                        temp_mul = mul_frac_temp[45:23];
                        temp_mul = temp_mul +1;
                        o_data_wire[22:0] = temp_mul;
                    end else begin
                        o_data_wire[22:0] = mul_frac_temp[45:23];
                    end
                    o_data_wire[30:23] = mul_exp_temp[7:0];
                    o_valid_wire = 1;
                end
                default: begin
                    o_data_wire = 0;
                    o_valid_wire = 1;
                end
            endcase
        end else begin
            o_data_wire = 0;
            o_valid_wire = 0;
        end
    end
    //sequential part
    always @(posedge i_clk or negedge i_rst_n) begin
        if (~i_rst_n) begin   //if reset = 0 then reset
            o_data_register <= 0;
            o_valid_register <= 0;
        end
        else begin
            o_data_register <= o_data_wire;
            o_valid_register <= o_valid_wire;
        end
    end


endmodule