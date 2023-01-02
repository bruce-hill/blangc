#pragma once
// The GCC JIT library is *way* *too* *verbose*
#include <libgccjit.h>

#define gcc_ctx_t gcc_jit_context
#define gcc_result_t gcc_jit_result
#define gcc_obj_t gcc_jit_object
#define gcc_loc_t gcc_jit_location
#define gcc_type_t gcc_jit_type
#define gcc_field_t gcc_jit_field
#define gcc_struct_t gcc_jit_struct
#define gcc_func_type_t gcc_jit_function_type
#define gcc_vector_type_t gcc_jit_vector_type
#define gcc_func_t gcc_jit_function
#define gcc_block_t gcc_jit_block
#define gcc_rvalue_t gcc_jit_rvalue
#define gcc_lvalue_t gcc_jit_lvalue
#define gcc_param_t gcc_jit_param
#define gcc_case_t gcc_jit_case
#define gcc_extended_asm_t gcc_jit_extended_asm
#define gcc_new_ctx gcc_jit_context_acquire
#define gcc_release_ctx gcc_jit_context_release
#define GCC_STR_OPTION_PROGNAME GCC_JIT_STR_OPTION_PROGNAME
#define GCC_NUM_STR_OPTIONS GCC_JIT_NUM_STR_OPTIONS
typedef enum gcc_jit_int_option gcc_int_opt_e;
#define GCC_INT_OPTION_OPTIMIZATION_LEVEL GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL
#define GCC_NUM_INT_OPTIONS GCC_JIT_NUM_INT_OPTIONS
typedef enum gcc_jit_bool_option gcc_bool_opt_e;
#define GCC_BOOL_OPTION_DEBUGINFO GCC_JIT_BOOL_OPTION_DEBUGINFO
#define GCC_BOOL_OPTION_DUMP_INITIAL_TREE GCC_JIT_BOOL_OPTION_DUMP_INITIAL_TREE
#define GCC_BOOL_OPTION_DUMP_INITIAL_GIMPLE GCC_JIT_BOOL_OPTION_DUMP_INITIAL_GIMPLE
#define GCC_BOOL_OPTION_DUMP_GENERATED_CODE GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE
#define GCC_BOOL_OPTION_DUMP_SUMMARY GCC_JIT_BOOL_OPTION_DUMP_SUMMARY
#define GCC_BOOL_OPTION_DUMP_EVERYTHING GCC_JIT_BOOL_OPTION_DUMP_EVERYTHING
#define GCC_BOOL_OPTION_SELFCHECK_GC GCC_JIT_BOOL_OPTION_SELFCHECK_GC
#define GCC_BOOL_OPTION_KEEP_INTERMEDIATES GCC_JIT_BOOL_OPTION_KEEP_INTERMEDIATES
#define GCC_NUM_BOOL_OPTIONS GCC_JIT_NUM_BOOL_OPTIONS
#define gcc_set_str_opt gcc_jit_context_set_str_option
#define gcc_set_int_opt gcc_jit_context_set_int_option
#define gcc_set_bool_opt gcc_jit_context_set_bool_option
#define gcc_allow_unreachable_blocks gcc_jit_context_set_bool_allow_unreachable_blocks
#define gcc_set_print_errs_to_stderr gcc_jit_context_set_bool_print_errors_to_stderr
#define gcc_use_external_driver gcc_jit_context_set_bool_use_external_driver
#define gcc_add_command_line_opt gcc_jit_context_add_command_line_option
#define gcc_add_driver_opt gcc_jit_context_add_driver_option
#define gcc_compile gcc_jit_context_compile
typedef enum gcc_jit_output_kind gcc_output_kind_e;
#define GCC_OUTPUT_KIND_ASSEMBLER GCC_JIT_OUTPUT_KIND_ASSEMBLER
#define GCC_OUTPUT_KIND_OBJECT_FILE GCC_JIT_OUTPUT_KIND_OBJECT_FILE
#define GCC_OUTPUT_KIND_DYNAMIC_LIBRARY GCC_JIT_OUTPUT_KIND_DYNAMIC_LIBRARY
#define GCC_OUTPUT_KIND_EXECUTABLE GCC_JIT_OUTPUT_KIND_EXECUTABLE
#define gcc_compile_to_file gcc_jit_context_compile_to_file
#define gcc_dump_to_file gcc_jit_context_dump_to_file
#define gcc_set_logfile gcc_jit_context_set_logfile
#define gcc_first_err gcc_jit_context_get_first_error
#define gcc_last_err gcc_jit_context_get_last_error
#define gcc_result_code gcc_jit_result_get_code
#define gcc_result_global gcc_jit_result_get_global
#define gcc_result_release gcc_jit_result_release
#define gcc_obj_ctx gcc_jit_object_get_context
#define gcc_obj_debug_string gcc_jit_object_get_debug_string
#define gcc_new_location gcc_jit_context_new_location
#define gcc_location_as_obj gcc_jit_location_as_object
#define gcc_type_as_obj gcc_jit_type_as_object
typedef enum gcc_jit_types gcc_types_e;
#define GCC_T_VOID GCC_JIT_TYPE_VOID
#define GCC_T_VOID_PTR GCC_JIT_TYPE_VOID_PTR
#define GCC_T_BOOL GCC_JIT_TYPE_BOOL
#define GCC_T_CHAR GCC_JIT_TYPE_CHAR
#define GCC_T_SHORT GCC_JIT_TYPE_SHORT
#define GCC_T_UNSIGNED_SHORT GCC_JIT_TYPE_UNSIGNED_SHORT
#define GCC_T_INT GCC_JIT_TYPE_INT
#define GCC_T_UNSIGNED_INT GCC_JIT_TYPE_UNSIGNED_INT
#define GCC_T_LONG GCC_JIT_TYPE_LONG
#define GCC_T_UNSIGNED_LONG GCC_JIT_TYPE_UNSIGNED_LONG
#define GCC_T_LONG_LONG GCC_JIT_TYPE_LONG_LONG
#define GCC_T_UNSIGNED_LONG_LONG GCC_JIT_TYPE_UNSIGNED_LONG_LONG
#define GCC_T_FLOAT GCC_JIT_TYPE_FLOAT
#define GCC_T_CONST_CHAR_PTR GCC_JIT_TYPE_CONST_CHAR_PTR
#define GCC_T_STRING GCC_JIT_TYPE_CONST_CHAR_PTR
#define GCC_T_SIZE GCC_JIT_TYPE_SIZE_T
#define GCC_T_FILE_PTR GCC_JIT_TYPE_FILE_PTR
#define GCC_T_COMPLEX_FLOAT GCC_JIT_TYPE_COMPLEX_FLOAT
#define GCC_T_UINT8 GCC_JIT_TYPE_UINT8_T
#define GCC_T_UINT16 GCC_JIT_TYPE_UINT16_T
#define GCC_T_UINT32 GCC_JIT_TYPE_UINT32_T
#define GCC_T_UINT64 GCC_JIT_TYPE_UINT64_T
#define GCC_T_UINT128 GCC_JIT_TYPE_UINT128_T
#define GCC_T_INT8 GCC_JIT_TYPE_INT8_T
#define GCC_T_INT16 GCC_JIT_TYPE_INT16_T
#define GCC_T_INT32 GCC_JIT_TYPE_INT32_T
#define GCC_T_INT64 GCC_JIT_TYPE_INT64_T
#define GCC_T_INT128 GCC_JIT_TYPE_INT128_T
#define GCC_T_FLOAT GCC_JIT_TYPE_FLOAT
#define GCC_T_DOUBLE GCC_JIT_TYPE_DOUBLE
#define gcc_get_type gcc_jit_context_get_type
#define gcc_get_int_type gcc_jit_context_get_int_type
#define gcc_get_ptr_type gcc_jit_type_get_pointer
#define gcc_get_const_type gcc_jit_type_get_const
#define gcc_get_volatile_type gcc_jit_type_get_volatile
#define gcc_compatible_types gcc_jit_compatible_types
#define gcc_type_size gcc_jit_type_get_size
#define gcc_array_type gcc_jit_context_new_array_type
#define gcc_new_field gcc_jit_context_new_field
#define gcc_bitfield gcc_jit_context_new_bitfield
#define gcc_field_as_obj gcc_jit_field_as_object
#define gcc_new_struct_type gcc_jit_context_new_struct_type
#define gcc_opaque_struct gcc_jit_context_new_opaque_struct
#define gcc_struct_as_type gcc_jit_struct_as_type
#define gcc_set_fields gcc_jit_struct_set_fields
#define gcc_get_field gcc_jit_struct_get_field
#define gcc_field_count gcc_jit_struct_get_field_count
#define gcc_union gcc_jit_context_new_union_type
#define gcc_new_func_type gcc_jit_context_new_function_ptr_type
#define gcc_new_param gcc_jit_context_new_param
#define gcc_param_as_obj gcc_jit_param_as_object
#define gcc_param_as_lvalue gcc_jit_param_as_lvalue
#define gcc_param_as_rvalue gcc_jit_param_as_rvalue
typedef enum gcc_jit_function_kind gcc_function_kind_e;
#define GCC_FUNCTION_EXPORTED GCC_JIT_FUNCTION_EXPORTED
#define GCC_FUNCTION_INTERNAL GCC_JIT_FUNCTION_INTERNAL
#define GCC_FUNCTION_IMPORTED GCC_JIT_FUNCTION_IMPORTED
#define GCC_FUNCTION_ALWAYS_INLINE GCC_JIT_FUNCTION_ALWAYS_INLINE
typedef enum gcc_jit_tls_model gcc_tls_model_e;
#define GCC_TLS_MODEL_NONE GCC_JIT_TLS_MODEL_NONE
#define GCC_TLS_MODEL_GLOBAL_DYNAMIC GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC
#define GCC_TLS_MODEL_LOCAL_DYNAMIC GCC_JIT_TLS_MODEL_LOCAL_DYNAMIC
#define GCC_TLS_MODEL_INITIAL_EXEC GCC_JIT_TLS_MODEL_INITIAL_EXEC
#define GCC_TLS_MODEL_LOCAL_EXEC GCC_JIT_TLS_MODEL_LOCAL_EXEC
#define gcc_new_func gcc_jit_context_new_function
#define gcc_builtin_func gcc_jit_context_get_builtin_function
#define gcc_func_as_obj gcc_jit_function_as_object
#define gcc_func_get_param gcc_jit_function_get_param
#define gcc_func_dump_to_dot gcc_jit_function_dump_to_dot
#define gcc_new_block gcc_jit_function_new_block
#define gcc_block_as_obj gcc_jit_block_as_object
#define gcc_block_func gcc_jit_block_get_function
typedef enum gcc_jit_global_kind gcc_global_kind_e;
#define GCC_GLOBAL_EXPORTED GCC_JIT_GLOBAL_EXPORTED
#define GCC_GLOBAL_INTERNAL GCC_JIT_GLOBAL_INTERNAL
#define GCC_GLOBAL_IMPORTED GCC_JIT_GLOBAL_IMPORTED
#define gcc_global gcc_jit_context_new_global
#define gcc_struct_constructor gcc_jit_context_new_struct_constructor
#define gcc_union_constructor gcc_jit_context_new_union_constructor
#define gcc_array_constructor gcc_jit_context_new_array_constructor
#define gcc_global_set_initializer_rvalue gcc_jit_global_set_initializer_rvalue
#define gcc_global_set_initializer gcc_jit_global_set_initializer
#define gcc_lvalue_as_obj gcc_jit_lvalue_as_object
#define gcc_rval gcc_jit_lvalue_as_rvalue
#define gcc_rvalue_as_obj gcc_jit_rvalue_as_object
#define gcc_rvalue_type gcc_jit_rvalue_get_type
#define gcc_rvalue_from_int gcc_jit_context_new_rvalue_from_int
#define gcc_rvalue_from_long gcc_jit_context_new_rvalue_from_long
#define gcc_zero gcc_jit_context_zero
#define gcc_one gcc_jit_context_one
#define gcc_rvalue_from_double gcc_jit_context_new_rvalue_from_double
#define gcc_rvalue_from_ptr gcc_jit_context_new_rvalue_from_ptr
#define gcc_null gcc_jit_context_null
#define gcc_str gcc_jit_context_new_string_literal
typedef enum gcc_jit_unary_op gcc_unary_op_e;
#define GCC_UNOP_MINUS GCC_JIT_UNARY_OP_MINUS
#define GCC_UNOP_BITWISE_NEGATE GCC_JIT_UNARY_OP_BITWISE_NEGATE
#define GCC_UNOP_LOGICAL_NEGATE GCC_JIT_UNARY_OP_LOGICAL_NEGATE
#define GCC_UNOP_ABS GCC_JIT_UNARY_OP_ABS
#define gcc_unary_op gcc_jit_context_new_unary_op
typedef enum gcc_jit_binary_op gcc_binary_op_e;
#define GCC_BINOP_PLUS GCC_JIT_BINARY_OP_PLUS
#define GCC_BINOP_MINUS GCC_JIT_BINARY_OP_MINUS
#define GCC_BINOP_MULT GCC_JIT_BINARY_OP_MULT
#define GCC_BINOP_DIVIDE GCC_JIT_BINARY_OP_DIVIDE
#define GCC_BINOP_MODULO GCC_JIT_BINARY_OP_MODULO
#define GCC_BINOP_BITWISE_AND GCC_JIT_BINARY_OP_BITWISE_AND
#define GCC_BINOP_BITWISE_XOR GCC_JIT_BINARY_OP_BITWISE_XOR
#define GCC_BINOP_BITWISE_OR GCC_JIT_BINARY_OP_BITWISE_OR
#define GCC_BINOP_LOGICAL_AND GCC_JIT_BINARY_OP_LOGICAL_AND
#define GCC_BINOP_LOGICAL_OR GCC_JIT_BINARY_OP_LOGICAL_OR
#define GCC_BINOP_LSHIFT GCC_JIT_BINARY_OP_LSHIFT
#define GCC_BINOP_RSHIFT GCC_JIT_BINARY_OP_RSHIFT
#define gcc_binary_op gcc_jit_context_new_binary_op
typedef enum gcc_jit_comparison gcc_comparison_e;
#define GCC_COMPARISON_EQ GCC_JIT_COMPARISON_EQ
#define GCC_COMPARISON_NE GCC_JIT_COMPARISON_NE
#define GCC_COMPARISON_LT GCC_JIT_COMPARISON_LT
#define GCC_COMPARISON_LE GCC_JIT_COMPARISON_LE
#define GCC_COMPARISON_GT GCC_JIT_COMPARISON_GT
#define GCC_COMPARISON_GE GCC_JIT_COMPARISON_GE
#define gcc_comparison gcc_jit_context_new_comparison
#define gcc_call gcc_jit_context_new_call
#define gcc_callx(ctx, loc, fn, ...) gcc_jit_context_new_call(ctx, loc, fn, sizeof((gcc_rvalue_t*[]){__VA_ARGS__})/sizeof(gcc_rvalue_t*), (gcc_rvalue_t*[]){__VA_ARGS__})
#define gcc_call_ptr gcc_jit_context_new_call_through_ptr
#define gcc_callx_ptr(ctx, loc, fn, ...) gcc_jit_context_new_call_through_ptr(ctx, loc, fn, sizeof((gcc_rvalue_t*[]){__VA_ARGS__})/sizeof(gcc_rvalue_t*), (gcc_rvalue_t*[]){__VA_ARGS__})
#define gcc_cast gcc_jit_context_new_cast
#define gcc_bitcast gcc_jit_context_new_bitcast
#define gcc_lvalue_set_alignment gcc_jit_lvalue_set_alignment
#define gcc_lvalue_get_alignment gcc_jit_lvalue_get_alignment
#define gcc_array_access gcc_jit_context_new_array_access
#define gcc_lvalue_access_field gcc_jit_lvalue_access_field
#define gcc_rvalue_access_field gcc_jit_rvalue_access_field
#define gcc_rvalue_dereference_field gcc_jit_rvalue_dereference_field
#define gcc_rvalue_dereference gcc_jit_rvalue_dereference
#define gcc_lvalue_address gcc_jit_lvalue_get_address
#define gcc_lvalue_set_tls_model gcc_jit_lvalue_set_tls_model
#define gcc_lvalue_set_link_section gcc_jit_lvalue_set_link_section
#define gcc_lvalue_set_register_name gcc_jit_lvalue_set_register_name
#define gcc_local gcc_jit_function_new_local
#define gcc_eval(block, ...) gcc_jit_block_add_eval((assert(block), block), __VA_ARGS__)
#define gcc_assign gcc_jit_block_add_assignment
#define gcc_update gcc_jit_block_add_assignment_op
#define gcc_comment gcc_jit_block_add_comment
#define gcc_jump_condition gcc_jit_block_end_with_conditional
#define gcc_jump gcc_jit_block_end_with_jump
#define gcc_return gcc_jit_block_end_with_return
#define gcc_return_void gcc_jit_block_end_with_void_return
#define gcc_new_case gcc_jit_context_new_case
#define gcc_case_as_obj gcc_jit_case_as_object
#define gcc_switch gcc_jit_block_end_with_switch
#define gcc_child_context gcc_jit_context_new_child_context
#define gcc_dump_reproducer_to_file gcc_jit_context_dump_reproducer_to_file
#define gcc_enable_dump gcc_jit_context_enable_dump
#define gcc_timer_new gcc_jit_timer_new
#define gcc_timer_release gcc_jit_timer_release
#define gcc_set_timer gcc_jit_context_set_timer
#define gcc_get_timer gcc_jit_context_get_timer
#define gcc_timer_push gcc_jit_timer_push
#define gcc_timer_pop gcc_jit_timer_pop
#define gcc_timer_print gcc_jit_timer_print
#define gcc_rvalue_require_tail_call gcc_jit_rvalue_set_bool_require_tail_call
#define gcc_aligned_type gcc_jit_type_get_aligned
#define gcc_new_vector_type gcc_jit_type_get_vector
#define gcc_get_func_address gcc_jit_function_get_address
#define gcc_rvalue_from_vector gcc_jit_context_new_rvalue_from_vector
#define gcc_add_extended_asm gcc_jit_block_add_extended_asm
#define gcc_extended_asm_goto gcc_jit_block_end_with_extended_asm_goto
#define gcc_extended_asm_as_obj gcc_jit_extended_asm_as_object
#define gcc_extended_asm_set_volatile_flag gcc_jit_extended_asm_set_volatile_flag
#define gcc_extended_asm_set_inline_flag gcc_jit_extended_asm_set_inline_flag
#define gcc_extended_asm_add_output_operand gcc_jit_extended_asm_add_output_operand
#define gcc_extended_asm_add_input_operand gcc_jit_extended_asm_add_input_operand
#define gcc_extended_asm_add_clobber gcc_jit_extended_asm_add_clobber
#define gcc_context_add_top_level_asm gcc_jit_context_add_top_level_asm
#define gcc_func_return_type gcc_jit_function_get_return_type
#define gcc_func_param_count gcc_jit_function_get_param_count
#define gcc_dyncast_array gcc_jit_type_dyncast_array
#define gcc_is_bool gcc_jit_type_is_bool
#define gcc_dyncast_func_ptr_type gcc_jit_type_dyncast_function_ptr_type
#define gcc_func_type_return_type gcc_jit_function_type_get_return_type
#define gcc_func_type_param_count gcc_jit_function_type_get_param_count
#define gcc_func_type_param_type gcc_jit_function_type_get_param_type
#define gcc_type_is_integral gcc_jit_type_is_integral
#define gcc_type_if_pointer gcc_jit_type_is_pointer
#define gcc_type_dyncast_vector gcc_jit_type_dyncast_vector
#define gcc_type_if_struct gcc_jit_type_is_struct
#define gcc_vector_type_get_num_units gcc_jit_vector_type_get_num_units
#define gcc_vector_type_get_element_type gcc_jit_vector_type_get_element_type
#define gcc_type_unqualified gcc_jit_type_unqualified
