

// We need a notion of built-in functions or intrinsic functions
// Lower-case convention used to match the names in source

#include "memory.h"

/* typedef enum {
    BIF_invalid,
    BIF_append,
} BuiltinFuncID;

typedef enum {
    BIPROP_invalid,
    BIPROP_rawptr,
    BIPROP_count,
    BIPROP_flat_count,
    BIPROP_type,
} BuiltinPropertyID; */

void raise_semantic_error(const char* err_msg) {
    error(err_msg);
    printf("\n");
}

typedef enum {
    SE_INVALID,
    SE_VAR_DECL,
    SE_FUNC_DECL,
} SemanticEntityKind;

typedef struct {
    SemanticEntityKind kind;
    ParserNode *node;
    bool is_rhs_constexpr;
} SemanticEntity;


typedef struct {
    SemanticEntity *entities;
    uint32_t entities_count;
    uint32_t entities_cap;

    Parser *parser;
    Arena *arena;
} SemanticAnalyzer;

// Forward declarations for semantic function passes:
void semantic1(SemanticAnalyzer *p);
void semantic2(SemanticAnalyzer *p);

typedef struct {

} SemanticContext;


void new_semantic_entity(SemanticAnalyzer *sema, ParserNode* node) {
    sema->entities = realloc_array(sema->arena, sema->entities, SemanticEntity, sema->entities_count + 1);
    SemanticEntity *e = &sema->entities[sema->entities_count++];
    mem_zero(e);

    switch (node->kind) {
        case NODE_VAR_DECL:
            e->kind = SE_VAR_DECL;
            break;

        case NODE_FUNC_DECL:
            e->kind = SE_FUNC_DECL;
            break;
    }

    e->node = node;
}

b32 nodekind_is_literal(ParserNodeKind kind) {
    switch (kind) {
        case NODE_NUMBER:
        case NODE_STRING:
        case NODE_BOOL:
        case NODE_NIL:
            return true;

        default:
            return false;
    }
}

// Semantic entities are identifiers, not literals
bool check_if_constexpr(SemanticContext *ctx, SemanticEntity *entity) {
    
}

SemanticEntity *get_semantic_entity(SemanticContext *ctx, ParserNode *node) {
    return NULL;
}

// Need to check that the r-values are constants, expressions of constants, or expressions of vardecls that are constants
// Returns true if constexpr
bool check_var_decl_rhs_constexpr(SemanticContext *ctx, SemanticEntity *var_decl) {
    ParserNode *rhs = var_decl->node->const_var_decl.rhs;

    if (rhs == NULL) {
        raise_semantic_error("There is no right-hand side.");
        return false;
    }

    if (nodekind_is_literal(rhs->kind)) {
        var_decl->is_rhs_constexpr = true;
    } else {
        //var_decl->is_rhs_constexpr = check_if_constexpr(ctx, get_semantic_entity(ctx, rhs));
        switch (rhs->kind) {
            case NODE_ARRAY_LITERAL: {
                var_decl->is_rhs_constexpr = true;
                for (uint32_t i = 0 ; i < rhs->array_literal.count; i++) {
                    if (!nodekind_is_literal(rhs->array_literal.elems[i]->kind)) {
                        var_decl->is_rhs_constexpr = false;
                        break;
                    }
                }
            } break;

            default: {
                assert(0);
            }
        }
    }

    if (!var_decl->is_rhs_constexpr) {
        raise_semantic_error("Variable declaration right-hand side cannot be evaluated to a constant.");
    }
}

void check_func_decl(SemanticContext *ctx, SemanticEntity *func_decl) {

}

// struct used to mark the start and end of a block node
typedef struct {
    uint32_t start;
    uint32_t end;
} StartEndMarker;

void get_start_end_of_block(Parser* p, ParserNode *node, StartEndMarker *marker) {
    assert(node->kind == NODE_BLOCK);
    marker->start = (node - p->nodes);
    marker->end = marker->start + node->block.statements_count;
}

// In the first pass, collect all the top-level declarations and error
// for cases that are not a declaration, also want to determine if vardecls are constants, or form const exprs (may do folding here)

// This could be moved into the parser.
// Also consider having errors stream by file and line number
void semantic1(SemanticAnalyzer *sema) {
    assert(sema->parser->nodes->kind == NODE_BLOCK);
    Node_Block top_level = sema->parser->nodes->block;

    for (uint32_t i = 0; i < top_level.statements_count; i++) {
        ParserNodeKind kind = top_level.stmts[i]->kind;
        String kind_str = node_kind_strings[kind];

        switch (kind) {
            case NODE_VAR_DECL:
            case NODE_FUNC_DECL:
                new_semantic_entity(sema, top_level.stmts[i]);
                break;

            default: {
                print(kind_str);
                raise_semantic_error("%S is not a valid node at top-level.");
            } break;
        }
    }

    printf("----------\n");
    semantic2(sema);
}

// Second pass of semantic analysis should go through the function bodies, collecting
// any new declarations and checking usages are correct

// Note: Consider the case of inner function declarations!!
void semantic2(SemanticAnalyzer *sema) {
    Node_Block top_level_block = sema->parser->nodes->block;
    SemanticContext *ctx = NULL;

    // Iterate over added semantic entities:
    for (uint32_t i = 0; i < sema->entities_count; i++) {
        SemanticEntity *entity = &sema->entities[i];

        switch (entity->kind) {
            case SE_VAR_DECL: {
                check_var_decl_rhs_constexpr(ctx, entity);
            } break;

            case SE_FUNC_DECL: {
                check_func_decl(ctx, entity);
            } break;

            case SE_INVALID: {
                continue; // previous pass semantic error
            } break;

            default: {
                raise_semantic_error("Unhandled entity kind");
                //assert(0);
            } break;
        }
    }
}


