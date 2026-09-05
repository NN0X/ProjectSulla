module dff(input clk, input D, input EN, output Q);
  reg q=0; assign Q=q;
  always @(posedge clk) if (EN) q<=D;
endmodule

module regbank8(input clk, input [7:0] D, input [7:0] EN, output [7:0] Q);
  dff u0(.clk(clk), .D(D[0]), .EN(EN[0]), .Q(Q[0]));
  dff u1(.clk(clk), .D(D[1]), .EN(EN[1]), .Q(Q[1]));
  dff u2(.clk(clk), .D(D[2]), .EN(EN[2]), .Q(Q[2]));
  dff u3(.clk(clk), .D(D[3]), .EN(EN[3]), .Q(Q[3]));
  dff u4(.clk(clk), .D(D[4]), .EN(EN[4]), .Q(Q[4]));
  dff u5(.clk(clk), .D(D[5]), .EN(EN[5]), .Q(Q[5]));
  dff u6(.clk(clk), .D(D[6]), .EN(EN[6]), .Q(Q[6]));
  dff u7(.clk(clk), .D(D[7]), .EN(EN[7]), .Q(Q[7]));
endmodule
