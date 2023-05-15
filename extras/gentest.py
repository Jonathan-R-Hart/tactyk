# A python script to help generate tactyk test scripts for instructions which perform simple transforms.
# Edit as needed.  Simple modifications to cover new tests should remain private.

import math
import random

PARAM_NULL = 0
PARAM_ZERO = 1
PARAM_ONE = 2
PARAM_SEQUENTIAL = 3
PARAM_RANDOM = 4
PARAM_PRODUCT = 5

class tparam:
  def __init__(self, name):
    self.type = PARAM_NULL
    self.per_var = False
    self.value = 0
    self.use_expect = False
    self.expect = 0
    self.min = 0
    self.max = 1
    self.incr = 0
    self.name = name
    self.is_int = name[0] == 'r'
    self.maybe_imm = False

pxa = tparam("xA")
pxb = tparam("xB")
pxc = tparam("xC")
pxd = tparam("xD")

pxe = tparam("xE")
pxf = tparam("xF")
pxg = tparam("xG")
pxh = tparam("xH")
pxi = tparam("xI")
pxj = tparam("xJ")
pxk = tparam("xK")
pxl = tparam("xL")
pxm = tparam("xM")
pxn = tparam("xN")

pra = tparam("rA")
prb = tparam("rB")
prc = tparam("rC")
prd = tparam("rD")
pre = tparam("rE")

params = [ pxa, pxb, pxc, pxd,  pra, prb, prc, prd, pre,  pxe, pxf, pxg, pxh, pxi, pxj, pxk, pxl, pxm, pxn ]

vars = [ pxe, pxf, pxg, pxh, pxi, pxj, pxk, pxl, pxm, pxn ]

test_r = ""
test_i = ""
test_c = ""
prog_i = ""
prog_r = ""
prog_c = ""

prog_c_consts = ""
ctable = {}

simulate = None

def gen_tests():
  global params, vars, test_r, test_i, test_c, prog_r, prog_i, prog_c, num_segments, prog_c_consts, simulate
  
  var_idx = 0
  for i in range(0, num_tests):
    test_r += f"EXEC {prefix}_{num_segments}_reg\nTEST\n"
    prog_r += f"  {prefix}_{num_segments}_reg:\n"
    test_i += f"EXEC {prefix}_{num_segments}_imm\nTEST\n"
    prog_i += f"  {prefix}_{num_segments}_imm:\n"
    test_c += f"EXEC {prefix}_{num_segments}_const\nTEST\n"
    prog_c += f"  {prefix}_{num_segments}_const:\n"
    
    for param in params:
      if not param.per_var:
        asn = True
        if param.type == PARAM_NULL:
          continue
        elif param.type == PARAM_ZERO:
          param.value = 0
        elif param.type == PARAM_ONE:
          param.value = 1
        elif param.type == PARAM_SEQUENTIAL:
          param.value += param.incr
        elif param.type == PARAM_RANDOM:
          param.value = random.random() * (param.max - param.min) + param.min
        elif param.type == PARAM_PRODUCT:
          asn = False
        if asn:
          prog_r += f"    assign {param.name} {param.value}\n"
          if not param.maybe_imm:
            prog_i += f"    assign {param.name} {param.value}\n"
            prog_c += f"    assign {param.name} {param.value}\n"
    
    num_segments += 1
    
    for var in vars:
      for param in params:
        if param.per_var:
          asn = True
          if param.type == PARAM_NULL:
            continue
          elif param.type == PARAM_ZERO:
            param.value = 0
          elif param.type == PARAM_ONE:
            param.value = 1
          elif param.type == PARAM_SEQUENTIAL:
            param.value += param.incr
          elif param.type == PARAM_RANDOM:
            param.value = random.random() * (param.max - param.min) + param.min
          elif param.type == PARAM_PRODUCT:
            asn = False
          if asn:
            prog_r += f"    assign {param.name} {param.value}\n"
            if not param.maybe_imm:
              prog_i += f"    assign {param.name} {param.value}\n"
              prog_c += f"    assign {param.name} {param.value}\n"
      simulate(var)
      prog_r += f"    {op}"
      prog_i += f"    {op}"
      prog_c += f"    {op}"
      for param in arg_order:
        if type(param) is list:
          _param = param[var_idx]
          var_idx = (var_idx + 1) % len(param)
          param = _param
        if type(param) is tparam:
          if param.maybe_imm:
            prog_i += f" {param.value}"
            prog_c += f" C{param.value}"
            prog_r += f" {param.name}"
            cname = f"C{param.value}"
            if param.value < 0:
              cname = f"CN{-param.value}"
            if ctable.get(cname) == None:
              ctable[cname] = True
              prog_c_consts += f"  const {cname} {param.value}\n"
          else:
            prog_i += f" {param.name}"
            prog_c += f" {param.name}"
            prog_r += f" {param.name}"
        else:
          prog_i += f" {str(param)}"
          prog_c += f" {str(param)}"
          prog_r += f" {str(param)}"
      prog_r += "\n"
      prog_i += "\n"
      prog_c += "\n"
    
    for param in params:
      if param.type != PARAM_NULL:
        if param.use_expect:
          test_r += f"  {param.name} {str(param.expect)}\n"
          test_i += f"  {param.name} {str(param.expect)}\n"
          test_c += f"  {param.name} {str(param.expect)}\n"
        else:
          test_r += f"  {param.name} {str(param.value)}\n"
          test_i += f"  {param.name} {str(param.value)}\n"
          test_c += f"  {param.name} {str(param.value)}\n"
  prog_r += "    exit\n"
  prog_i += "    exit\n"
  prog_c += "    exit\n"


# settings
num_tests = 1
num_segments = 1
export_reg = True
export_const = False
export_imm = False


# variable/parameter setup
# This setup is for a unary operation ("instruction <register>")
for var in vars:
  var.type = PARAM_RANDOM
  var.min = 0.01
  var.max = 100
  var.maybe_imm = True
  var.use_expect = True

# Python implementation of the intended function
def simulate_x(var):
  var.expect = math.log(var.value)

simulate = simulate_x

# set up the tactyk instruction
arg_order = [vars]
prefix = "LOG_X"
op = "log"

# Append test code to each test script chunk
# gen_tests()


# reconfigure the test to target a variant of the tactyk instruction

# This setup is for a basic unary operation ("instruction <destination-register> <source-register>")

for var in vars:
  var.type = PARAM_RANDOM
  var.min = 0.01
  var.max = 100
  var.maybe_imm = True
  var.use_expect = True


# prepare the source register.
pxa.type = PARAM_RANDOM
pxa.per_var = True
pxa.maybe_imm = True
pxa.min = 0.001
pxa.max = 10

arg_order = [vars, pxa]
prefix = "LOG_XX"

def simulate_xx(var):
  var.expect = math.log(var.value)/math.log(pxa.value)

simulate = simulate_xx

gen_tests()



for var in vars:
  var.type = PARAM_PRODUCT
  var.maybe_imm = False
  var.use_expect = True

# prepare the source register.
pxa.type = PARAM_RANDOM
pxa.per_var = True
pxa.maybe_imm = True
pxa.min = 0.001
pxa.max = 100

pxb.type = PARAM_RANDOM
pxb.per_var = True
pxb.maybe_imm = True
pxb.min = 0.001
pxb.max = 10

arg_order = [vars, pxa, pxb]
prefix = "LOG_XXX"

def simulate_xxx(var):
  var.expect = math.log(pxa.value)/math.log(pxb.value)

simulate = simulate_xxx

gen_tests()

# export the test script
#  for now, stdout will suffice

print("PROGRAM")
if export_const:
  print(prog_c_consts)

if export_reg:
  print(prog_r)

if export_const:
  print(prog_c)

if export_imm:
  print(prog_i)


if export_reg:
  print(test_r)

if export_const:
  print(test_c)

if export_imm:
  print(test_i)


