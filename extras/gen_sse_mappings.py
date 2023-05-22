
def gen_mappings(reg_count, elem_count, wname):
  for i in range(0, reg_count):
    for w in range(0,elem_count):
      print( f"    case {wname}.{elem_count*i + w}" )
      print( f"      $SIMD_REG {i}" )
      print( f"      $SIMD_INDEX {w}" )

print("subroutine sse-map")
print("  select $SSE_WORD~$RAW_OPERAND")
gen_mappings(14, 2, "qword")
print("    ")
gen_mappings(14, 4, "word")
print("    ")
gen_mappings(14, 4, "dword")
print("    ")
gen_mappings(14, 16, "byte")
print("    ")