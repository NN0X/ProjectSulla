module ram_async(input clk, input we, input [7:0] addr, input [7:0] din, output reg [7:0] dout);
  reg [7:0] mem [0:255];
  always @(posedge clk) if (we) mem[addr] <= din;
  always @(*) dout = mem[addr];
endmodule
