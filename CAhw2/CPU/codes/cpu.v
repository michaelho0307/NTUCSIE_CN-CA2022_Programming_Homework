module cpu #( // Do not modify interface
	parameter ADDR_W = 64,
	parameter INST_W = 32,
	parameter DATA_W = 64
)(
    input                   i_clk,
    input                   i_rst_n,
    input                   i_i_valid_inst, // from instruction memory
    input  [ INST_W-1 : 0 ] i_i_inst,       // from instruction memory
    input                   i_d_valid_data, // from data memory
    input  [ DATA_W-1 : 0 ] i_d_data,       // from data memory
    output                  o_i_valid_addr, // to instruction memory
    output [ ADDR_W-1 : 0 ] o_i_addr,       // to instruction memory
    output [ DATA_W-1 : 0 ] o_d_data,       // to data memory
    output [ ADDR_W-1 : 0 ] o_d_addr,       // to data memory
    output                  o_d_MemRead,    // to data memory
    output                  o_d_MemWrite,   // to data memory
    output                  o_finish
);

// homework
// define some wires 
wire [ADDR_W-1:0] outfromPC,gotoPC,goto4adder,outfrom4adder,gotosumadder1,gotosumadder2,outfromsumadder;
wire [ADDR_W-1:0] gotoPCmul1,gotoPCmul2,outfromPCmul,gotoALUmul1,gotoALUmul2,outfromALUmul;
wire [ADDR_W-1:0] gotoDATAmul1,gotoDATAmul2,outfromDATAmul,outfromimmgen;
wire [ADDR_W-1:0] gotoregwritedata,outfromregreaddata1,outfromregreaddata2;
wire [ADDR_W-1:0] gotoALU1,gotoALU2,outfromALU,datamemory_reg;
wire ALUzero;
//connect lines
assign o_i_addr = outfromPC;
assign goto4adder = outfromPC;
assign gotoPCmul1 = outfrom4adder;
assign gotoPCmul2 = outfromsumadder;
assign gotoPC = outfromPCmul;
assign gotosumadder1 = outfromPC;
assign gotosumadder2 = outfromimmgen;
assign gotoALUmul2 = outfromimmgen;
assign gotoregwritedata = outfromDATAmul;
assign o_d_data = outfromregreaddata2;
assign gotoDATAmul1 = datamemory_reg;
assign gotoALUmul1 = outfromregreaddata2;
assign gotoALU1 = outfromregreaddata1;
assign gotoALU2 = outfromALUmul;
assign o_d_addr = outfromALU;
assign gotoDATAmul2 = outfromALU;
//assign datas
reg ALUoperation_start; //1: if ALU need to operate, 0: not yet
reg ALUoperation_done; //1: if ALU finish operation, 0: not yet
reg [INST_W-1:0] current_instruction; //Current instruction fetched from InstMem
reg writeback_data; //1:if write back to register, 0: dont write
reg o_finish_reg,o_d_MemRead_reg,o_d_MemWrite_reg;
assign datamemory_reg = i_d_data;
assign o_finish = o_finish_reg;
assign o_d_MemRead = o_d_MemRead_reg;
assign o_d_MemWrite = o_d_MemWrite_reg;
//handling control signals
always @(*) begin
    //instruction fetched
    if (i_i_valid_inst)begin
        current_instruction = i_i_inst;
        if (current_instruction != 32'b11111111111111111111111111111111)begin  //Do operations
            ALUoperation_start = 1;
        end else begin  //Stop
            o_finish_reg = 1;
            ALUoperation_start = 0;
        end
        //i_i_valid_inst = 0;
    end else begin
        ALUoperation_start = 0;
    end
    //data memory fetched or not fetched,write back to reg
    if (ALUoperation_done && (current_instruction[4:0] == 5'b10011))begin  //R-type
        writeback_data = 1;
    end else if (i_d_valid_data)begin  //load
        writeback_data = 1;
        //i_d_valid_data = 0;
    end else begin
        writeback_data = 0;
    end
    //start to getch data memory(ld or sd), ALU finish operations
    if (ALUoperation_done)begin
        o_d_MemRead_reg = (current_instruction[6:0] == 7'b0000011)?1:0;   //load
        o_d_MemWrite_reg = (current_instruction[6:0] == 7'b0100011)?1:0;  //store
    end else begin
        o_d_MemRead_reg = 0;
        o_d_MemWrite_reg = 0;
    end
end

always @(posedge i_clk or negedge i_rst_n) begin
    if (~i_rst_n) begin
        current_instruction <= 0;  //reset instruction to none
    end else begin
        ALUoperation_done <= ALUoperation_start; //finish ALU operation
        ALUoperation_start = 0;
    end
end

//include module
pc #(
    .ADDR_W(ADDR_W)
) Progcounter(
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .input_addr(gotoPC),
    .output_addr(outfromPC),
    .is_validtime(o_i_valid_addr)
);

adder_4orsum #(
    .ADDR_W(ADDR_W)
) adder4 (
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .data_a(goto4adder),
    .data_b(64'b100),
    .data_sum(outfrom4adder)
);
adder_4orsum #(
    .ADDR_W(ADDR_W)
) addersum (
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .data_a(gotosumadder1),
    .data_b(gotosumadder2),  //maybe
    .data_sum(outfromsumadder)
);
//multiplex selection!!!
multiplex #(
    .ADDR_W(ADDR_W)
) PCmul (
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .data_a(gotoPCmul1),
    .data_b(gotoPCmul2),
    .choosea0orb1((current_instruction[6:0] == 7'b1100011) && (ALUzero ^ current_instruction[12])),
    .data_out(outfromPCmul)
);
multiplex #(
    .ADDR_W(ADDR_W)
) ALUmul (
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .data_a(gotoALUmul1),
    .data_b(gotoALUmul2),
    .choosea0orb1((current_instruction[14:12] == 3'b011) || (current_instruction[6:0] == 7'b0010011)),
    .data_out(outfromALUmul)
);
multiplex #(
    .ADDR_W(ADDR_W)
) DATAmul (
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .data_a(gotoDATAmul2),
    .data_b(gotoDATAmul1),
    .choosea0orb1((current_instruction[14:12] == 3'b011) &&(current_instruction[5] == 0)),
    .data_out(outfromDATAmul)
);

immgen #(
    .ADDR_W(ADDR_W)
) immgen_u(
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .inputinstruction(current_instruction), //imp
    .outputdata(outfromimmgen)
);
regist #(
    .ADDR_W(ADDR_W)
) reg_u(
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .read_reg1(current_instruction[19:15]),
    .read_reg2(current_instruction[24:20]),
    .write_reg(current_instruction[11:7]),
    .writedata(gotoregwritedata),
    .writedatasignal(writeback_data),
    .read_data1(outfromregreaddata1),
    .read_data2(outfromregreaddata2)
);
alu #(
    .DATA_W(DATA_W),
    .INST_W(INST_W)
) alu_u(
    .i_clk(i_clk),
    .i_rst_n(i_rst_n),
    .inputinstruction(current_instruction),
    .data_a(gotoALU1),
    .data_b(gotoALU2),
    .data_out(outfromALU),
    .isALUzero(ALUzero)    
);
endmodule
