
def gen_mappings(reg_count, elem_count, wname, inspsarg=0):
  for i in range(0, reg_count):
    for w in range(0,elem_count):
      print( f"    case {wname}~{elem_count*i + w}" )
      print( f"      $SSE_REG {i}" )
      print( f"      $SSE_INDEX {w}" )
      if inspsarg > 0:
        print( f"      $INSPS_arg {inspsarg*w}" )

print("typespec sse-map")
print("  select $SSE_WORD~$RAW_OPERAND")
gen_mappings(14, 2, "q")
print("    ")
gen_mappings(14, 4, "d", 16)
print("    ")
gen_mappings(14, 8, "w")
print("    ")
gen_mappings(14, 16, "b")
print("    ")