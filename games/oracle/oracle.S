/* Phase 0.5 discovery oracle for neo-recomp.
   This source deliberately plants one instance of each structural discovery
   family.  It is not a complete game, only an ELF-backed P-ROM fixture. */

    .section .vectors,"a"
    .long 0x0010f300
    .long oracle_entry

    .section .cart_header,"a"
    .ascii "NEO-GEO"
    .byte 0
    .space 0x1a,0xff
    .word 0x4ef9
    .long oracle_entry

    .section .text.entry,"ax"
    .global oracle_entry
    .type oracle_entry,@function
oracle_entry:
    jsr oracle_direct_func
    lea oracle_object_state_callback,%a1
    move.l %a1,0x70(%a6)
    lea oracle_spawn_callback,%a1
    jsr oracle_spawn_helper
    rts
    .size oracle_entry, .-oracle_entry

    .section .text.direct,"ax"
    .global oracle_direct_func
    .type oracle_direct_func,@function
oracle_direct_func:
    nop
    rts
    .size oracle_direct_func, .-oracle_direct_func

    .section .text.object,"ax"
    .global oracle_object_state_callback
    .type oracle_object_state_callback,@function
oracle_object_state_callback:
    nop
    rts
    .size oracle_object_state_callback, .-oracle_object_state_callback

    .section .text.spawn,"ax"
    .global oracle_spawn_callback
    .type oracle_spawn_callback,@function
oracle_spawn_callback:
    nop
    rts
    .size oracle_spawn_callback, .-oracle_spawn_callback

    .section .text.spawn_helper,"ax"
    .global oracle_spawn_helper
    .type oracle_spawn_helper,@function
oracle_spawn_helper:
    rts
    .size oracle_spawn_helper, .-oracle_spawn_helper

    .section .text.state_a,"ax"
    .global oracle_state_table_callback
    .type oracle_state_table_callback,@function
oracle_state_table_callback:
    nop
    rts
    .size oracle_state_table_callback, .-oracle_state_table_callback

    .section .text.state_b,"ax"
    .global oracle_chained_state_callback
    .type oracle_chained_state_callback,@function
oracle_chained_state_callback:
    nop
    rts
    .size oracle_chained_state_callback, .-oracle_chained_state_callback

    .section .text.tagged,"ax"
    .global oracle_tagged_record_callback
    .type oracle_tagged_record_callback,@function
oracle_tagged_record_callback:
    nop
    rts
    .size oracle_tagged_record_callback, .-oracle_tagged_record_callback

    .section .text.fixed,"ax"
    .global oracle_fixed_record_callback
    .type oracle_fixed_record_callback,@function
oracle_fixed_record_callback:
    nop
    rts
    .size oracle_fixed_record_callback, .-oracle_fixed_record_callback

    .section .text.vector_a,"ax"
    .global oracle_object_vector_callback_a
    .type oracle_object_vector_callback_a,@function
oracle_object_vector_callback_a:
    nop
    rts
    .size oracle_object_vector_callback_a, .-oracle_object_vector_callback_a

    .section .text.vector_b,"ax"
    .global oracle_object_vector_callback_b
    .type oracle_object_vector_callback_b,@function
oracle_object_vector_callback_b:
    nop
    rts
    .size oracle_object_vector_callback_b, .-oracle_object_vector_callback_b

    .section .text.routine_a,"ax"
    .global oracle_routine_table_entry_a
    .type oracle_routine_table_entry_a,@function
oracle_routine_table_entry_a:
    nop
    jsr oracle_direct_func
    rts
    .size oracle_routine_table_entry_a, .-oracle_routine_table_entry_a

    .section .text.routine_b,"ax"
    .global oracle_routine_table_entry_b
    .type oracle_routine_table_entry_b,@function
oracle_routine_table_entry_b:
    nop
    nop
    bra oracle_routine_table_shared_tail
    .size oracle_routine_table_entry_b, .-oracle_routine_table_entry_b

    .section .text.routine_tail,"ax"
    .global oracle_routine_table_shared_tail
    .type oracle_routine_table_shared_tail,@function
oracle_routine_table_shared_tail:
    nop
    rts
    .size oracle_routine_table_shared_tail, .-oracle_routine_table_shared_tail

    .section .text.bank0,"ax"
    .global oracle_bank0_callback
    .type oracle_bank0_callback,@function
oracle_bank0_callback:
    nop
    rts
    .size oracle_bank0_callback, .-oracle_bank0_callback

    .section .text.bank1,"ax"
    .global oracle_bank1_callback
    .type oracle_bank1_callback,@function
oracle_bank1_callback:
    nop
    rts
    .size oracle_bank1_callback, .-oracle_bank1_callback

    .section .data.state_table,"aw"
    .global oracle_state_table
oracle_state_table:
    .long oracle_state_table_callback
    .long oracle_state_table_next
    .long 0xffffffff
    .long 0xdead0000
oracle_state_table_next:
    .long oracle_chained_state_callback
    .long 0xdead0000

    .section .data.tagged,"aw"
    .global oracle_tagged_records
oracle_tagged_records:
    .word 0x0800
    .long oracle_tagged_record_callback
    .space 10,0xff
    .word 0x0700
    .long oracle_wrong_tag_data

    .section .data.fixed,"aw"
    .global oracle_fixed_records
oracle_fixed_records:
    .space 4,0xff
    .long oracle_fixed_record_callback
    .space 8,0xff
    .space 4,0xff
    .long 0xffffffff
    .space 8,0xff

    .section .data.vector,"aw"
    .global oracle_object_vector
oracle_object_vector:
    .long oracle_object_vector_callback_a
    .long oracle_object_vector_callback_b
    .long oracle_vector_data
    .long 0xffffffff

    .section .data.negative_wrong_tag,"aw"
    .global oracle_wrong_tag_data
oracle_wrong_tag_data:
    .word 0x4e75

    .section .data.negative_vector,"aw"
    .global oracle_vector_data
oracle_vector_data:
    .word 0x4e75

    .section .data.bank0_records,"aw"
    .global oracle_bank0_records
oracle_bank0_records:
    .long oracle_bank0_callback

    .section .data.bank1_records,"aw"
    .global oracle_bank1_records
oracle_bank1_records:
    .long oracle_bank1_callback
