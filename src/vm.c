
void vm_run(void) {
    uint16_t bc_idx = 0;
    while (bc_idx < code_size_bytes) {
        uint16_t operator = bytecode[bc_idx++];
        uint16_t var_slot;
        uint16_t arr_slot;
        TiraVal tira_const;

        switch (operator) {
            case BYOP_NOP: break;

            { // LOADK ops
            case BYOP_LOADK: {
                var_slot = bytecode[bc_idx++];
                tira_const = bytecode[bc_idx++];
            } break;
            } // End of LOADK ops

            case BYOP_LOAD_ARRAY: {
                var_slot = bytecode[bc_idx++];
                arr_slot = bytecode[bc_idx++];
            } break;

            case BYOP_SET_TRUE: {
                var_slot = bytecode[bc_idx++];
                var_table[var_slot] = TIRA_TRUE;
            } break;

            case BYOP_SET_FALSE: {
                var_slot = bytecode[bc_idx++];
                var_table[var_slot] = TIRA_FALSE;
            } break;

            case BYOP_CALL: {
                typedef TiraVal (*TiraRTFunc)(TiraVal);

                uint16_t func_slot = bytecode[bc_idx++]; 
                uint16_t arg_count = bytecode[bc_idx++];
                uint16_t arg_slot_start = bytecode[bc_idx++];

                //((TiraVal (*)(TiraVal))function_table[func_slot].func)(0);
                ((TiraRTFunc)function_table[func_slot].func)(0);
            } break;

            case BYOP_TEST: { // immediately followed by JMP_REL_32


                // JMP_REL_32
            } break;

            case BYOP_FOR_ARRAY: { // immediately followed by JMP_REL_32


                // JMP_REL_32
            } break;

            case BYOP_JMP: { // JMP_REL_32

            } break;

            default: {
                printf("VM operation not implemented: ");
                printf(byop_strings[operator].data); // @Fixup 1) use of %S (char* vs String), 2) put in as argument
                printf("\n");
            } break;
        }
    }

    printf("bc_idx: %uh\n", bc_idx);
}
