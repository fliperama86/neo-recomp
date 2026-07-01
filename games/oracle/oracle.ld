OUTPUT_ARCH(m68k)
ENTRY(oracle_entry)
SECTIONS
{
  .vectors 0x000000 : { *(.vectors) }
  .cart_header 0x000100 : { *(.cart_header) }
  .text.entry 0x000200 : { *(.text.entry) }
  .text.direct 0x000220 : { *(.text.direct) }
  .text.object 0x000240 : { *(.text.object) }
  .text.spawn 0x000260 : { *(.text.spawn) }
  .text.spawn_helper 0x000280 : { *(.text.spawn_helper) }
  .text.state_a 0x0002A0 : { *(.text.state_a) }
  .text.state_b 0x0002C0 : { *(.text.state_b) }
  .text.tagged 0x0002E0 : { *(.text.tagged) }
  .data.state_table 0x000300 : { *(.data.state_table) }
  .data.tagged 0x000320 : { *(.data.tagged) }
  .data.fixed 0x000340 : { *(.data.fixed) }
  .text.fixed 0x000380 : { *(.text.fixed) }
  .text.vector_a 0x0003A0 : { *(.text.vector_a) }
  .text.vector_b 0x0003C0 : { *(.text.vector_b) }
  .data.negative_wrong_tag 0x0003E0 : { *(.data.negative_wrong_tag) }
  .text.routine_a 0x000400 : { *(.text.routine_a) }
  .text.routine_b 0x000410 : { *(.text.routine_b) }
  .text.routine_tail 0x000430 : { *(.text.routine_tail) }
  .data.vector 0x000440 : { *(.data.vector) }
  .data.negative_vector 0x000460 : { *(.data.negative_vector) }
  .data.bank0_records 0x200000 : { *(.data.bank0_records) }
  .text.bank0 0x200020 : { *(.text.bank0) }
  .data.bank1_records 0x210000 : { *(.data.bank1_records) }
  .text.bank1 0x210020 : { *(.text.bank1) }
}
