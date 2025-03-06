from triton import *

def parse_trace_line(line):
    parts = line.split()
    rip = int(parts[1], 16)
    opcode_str = parts[3]
    opcode = bytes.fromhex(opcode_str.replace(" ", ""))
    regs = {}
    mem_read = None
    mem_write = None

    for part in parts[4:]:
        if '=' in part:
            reg, val = part.split('=')
            regs[reg] = int(val, 16)
        elif 'MEMORY_READ' in part:
            addr, val = part.split('=')[1].split('=')
            mem_read = (int(addr, 16), int(val, 16))
        elif 'MEMORY_WRITE' in part:
            addr, val = part.split('=')[1].split('=')
            mem_write = (int(addr, 16), int(val, 16))
    return rip, opcode, regs, mem_read, mem_write

ctx = TritonContext()
ctx.setArchitecture(ARCH.X86_64)

ctx.symbolizeRegister(ctx.registers.rdi, "input_rdi")

with open("trace.log", "r") as f:
    trace = f.readlines()

for line in trace:
    rip, opcode, regs, mem_read, mem_write = parse_trace_line(line)

    for reg_name, value in regs.items():
        reg = getattr(ctx.registers, reg_name.lower())
        ctx.setConcreteRegisterValue(reg, value)

    if mem_read:
        ctx.setConcreteMemoryValue(mem_read[0], mem_read[1])

    inst = Instruction()
    inst.setAddress(rip)
    inst.setOpcode(opcode)
    ctx.processing(inst)

    if mem_write:
        ctx.setConcreteMemoryValue(mem_write[0], mem_write[1])

ast_ctx = ctx.getAstContext()
rax_ast = ctx.getSymbolicRegister(ctx.registers.rax).getAst()
simplified_ast = ctx.simplify(rax_ast)

with open("devirt_func.ll", "w") as f:
    module = ctx.liftToLLVM(simplified_ast, "devirt_func")
    f.write(str(module))