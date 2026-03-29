
void vm_run(void) {
    uint16_t bc_idx = 0;
    while (bc_idx < code_size_bytes) {
        switch (bytecode[bc_idx++]) {
            case BYOP_NOP: break;

            case BYOP_LOADK: {

            } break;

            case BYOP_LOADK_INT: {

            } break;

            case BYOP_SET_TRUE: {
                uint16_t var_slot = bytecode[bc_idx++];
                var_table[var_slot] = TIRA_TRUE;
            } break;

            case BYOP_SET_FALSE: {
                uint16_t var_slot = bytecode[bc_idx++];
                var_table[var_slot] = TIRA_FALSE;

            } break;

            case BYOP_ADD_INT: {

            } break;

            case BYOP_SUB_INT: {

            } break;

            case BYOP_MUL_INT: {

            } break;

            case BYOP_DIV_INT: {

            } break;

            case BYOP_CALL: {
                typedef TiraVal (*TiraRTFunc)(TiraVal);

                uint16_t func_slot = bytecode[bc_idx++]; 
                uint16_t arg_count = bytecode[bc_idx++];
                uint16_t arg_slot_start = bytecode[bc_idx++];

                //((TiraVal (*)(TiraVal))function_table[func_slot].func)(0);
                ((TiraRTFunc)function_table[func_slot].func)(0);
            } break;

            case BYOP_IS_NIL: {

            } break;

            case BYOP_EQUAL_TO_STRING: {

            } break;

            case BYOP_LOGICAL_AND_BOOL: {

            } break;

            case BYOP_LOGICAL_OR_BOOL: {

            } break;

            default: {
                printf("Operation not implemented\n");
            } break;
        }
    }

    printf("bc_idx: %uh\n", bc_idx);
}