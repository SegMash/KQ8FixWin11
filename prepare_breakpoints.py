import re

# Your log content - just paste it here between the triple quotes
log_content = """
  vtable[0] = 0x004093a0
  vtable[1] = 0x004093c0
  vtable[2] = 0x004f7920
  vtable[3] = 0x004f7900
  vtable[4] = 0x004f7910
  vtable[5] = 0x00468a50
  vtable[6] = 0x0046e410
  vtable[7] = 0x00481b40
  vtable[8] = 0x004a9890
  vtable[9] = 0x00481730
  vtable[10] = 0x00534fe0
  vtable[11] = 0x00469150
  vtable[12] = 0x0046cc60
  vtable[13] = 0x0046e080
  vtable[14] = 0x0046e200
  vtable[15] = 0x0046dd20
  vtable[16] = 0x00487290
  vtable[17] = 0x00487110
  vtable[18] = 0x004acf70
  vtable[19] = 0x004acf40
  vtable[20] = 0x004872f0
  vtable[21] = 0x00487670
  vtable[22] = 0x004acf50
  vtable[23] = 0x004874b0
  vtable[24] = 0x004aced0
  vtable[25] = 0x004acf80
  vtable[26] = 0x00469830
  vtable[27] = 0x004810f0
  vtable[28] = 0x0046a8e0
  vtable[29] = 0x004ab4a0
  vtable[30] = 0x004ab6d0
  vtable[31] = 0x004878f0
  vtable[32] = 0x0046c740
  vtable[33] = 0x0046ca30
  vtable[34] = 0x004abee0
  vtable[35] = 0x004ab760
  vtable[36] = 0x004aba70
  vtable[37] = 0x004ac1b0
  vtable[38] = 0x004810c0
  vtable[39] = 0x0046e540
  vtable[40] = 0x0046e9b0
  vtable[41] = 0x004ac240
  vtable[42] = 0x0046eda0
  vtable[43] = 0x00426bf0
  vtable[44] = 0x004abb50
  vtable[45] = 0x004a9c70
  vtable[46] = 0x0046caa0
  vtable[47] = 0x0046cae0
  vtable[48] = 0x00481680
  vtable[49] = 0x00481440
"""

# Extract addresses and create breakpoint commands
addresses = re.findall(r'vtable\[\d+\] = (0x[0-9a-fA-F]+)', log_content)

# Create breakpoint commands separated by semicolons on one line
breakpoint_commands = [f"bp {addr}" for addr in addresses]
print("; ".join(breakpoint_commands))