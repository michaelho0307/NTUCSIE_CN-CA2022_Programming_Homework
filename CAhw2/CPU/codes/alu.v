module alu#(
    parameter DATA_W = 64,
    parameter INST_W = 32
)(
    input                   i_clk,
    input                   i_rst_n,
    input  [INST_W-1:0]     inputinstruction,      
    input  [DATA_W-1:0]     data_a,
    input  [DATA_W-1:0]     data_b,
    output [DATA_W-1:0]     data_out,          
    output                  isALUzero 
    //isALUzero == 1:(ALU arithmetic result == 0)
);

    reg [DATA_W-1:0] o_data_register,o_data_wire;
    reg [6:0] opcode;
    reg [2:0] func3;
    reg isALUzero_reg,isALUzero_wire;
    assign isALUzero = isALUzero_reg;
    assign data_out = o_data_register;
    always @(*) begin
        opcode = inputinstruction[6:0];
        func3 = inputinstruction[14:12];
        isALUzero_wire = 0;
        case (opcode)
            //load store
            7'b0000011, 7'b0100011:begin  
                o_data_wire = data_a+data_b;
            end
            //beq bne
            7'b1100011:begin
                o_data_wire = data_a-data_b;
                if (o_data_wire == 0) isALUzero_wire = 1;
            end
            default:begin
                case(func3)
                //slli
                3'b001:begin
                    o_data_wire = data_a << data_b;
                end
                //srli
                3'b101:begin
                    o_data_wire = data_a >> data_b;
                end
                //addi add sub
                3'b000:begin
                    if (inputinstruction[30]) begin  //sub
                        o_data_wire = data_a-data_b;
                    end else begin //add
                        o_data_wire = data_a+data_b;
                    end
                end
                //xor
                3'b100:begin
                    o_data_wire = data_a^data_b;
                end
                //or
                3'b110:begin
                    o_data_wire = data_a | data_b;
                end
                //and
                3'b111:begin
                    o_data_wire = data_a & data_b;
                end
                endcase
            end
        endcase
    end

    always @(posedge i_clk or negedge i_rst_n) begin
        if (~i_rst_n) begin   //if reset = 0 then reset
            o_data_register <= 0;
        end
        else begin
            o_data_register <= o_data_wire;
            isALUzero_reg <= isALUzero_wire;
        end
    end


endmodule
