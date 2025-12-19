// traductor.inc - LLVM IR Translation (Algebraic Backend)

incorporate "ss_analysis.inc"

datum tr_last_reg : series = "";
datum tr_last_type : series = "";
datum current_block_label : series = "entry";
datum block_terminated : state = false;

// Forward declarations
algorithm translate_expr(node_idx : whole) -> void;
algorithm translate_stmt(node_idx : whole) -> void;
algorithm translate_block(node_idx : whole) -> void;
algorithm translate_program(prog_idx : whole) -> void;
algorithm emit_llvm_runtime() -> void;

// Helpers
algorithm llvm_emit_label_update(l : series) -> void
begin
    llvm_label(l);
    current_block_label = l;
    block_terminated = false;
end

algorithm llvm_emit_br_update(l : series) -> void
begin
    llvm_br(l);
    block_terminated = true;
end

algorithm llvm_emit_cond_br_update(cond : series, t : series, f : series) -> void
begin
    llvm_cond_br(cond, t, f);
    block_terminated = true;
end

algorithm llvm_type_string(t : series) -> series
begin
    verify (t == "whole") then result "i64"; conclude
    verify (t == "precise") then result "double"; conclude
    verify (t == "state") then result "i1"; conclude
    verify (t == "series") then result "i8*"; conclude
    verify (t == "sequence") then result "%struct.sequence*"; conclude

    datum len : whole = length(t);
    verify (len > 0) then
         datum first : series = substring(t, 0, 1);
         verify (first == "%") then result t; conclude
    conclude

    // Fallback for struct name only
    datum sz : whole = mem_get_struct_size(t);
    verify (sz > 0) then result "%struct." + t + "*"; conclude

    result "i64";
end

algorithm is_sequence_type(t : series) -> state
begin
    datum len : whole = length(t);
    verify (len >= 8) then
        verify (substring(t, 0, 8) == "sequence") then result true; conclude
    conclude
    result false;
end

algorithm get_element_type_from_sequence(t : series) -> series
begin
    datum len : whole = length(t);
    verify (len > 9) then
        result substring(t, 9, len - 10);
    conclude
    result "whole";
end

algorithm map_node_type(node_idx : whole) -> series
begin
    verify (node_idx == -1) then result "whole";
    conclude
    datum node : ASTNode = node_heap[node_idx];
    datum name : series = node.text_val;
    
    // FIX: Manejo recursivo de genéricos (sequence<T>)
    verify (name == "sequence") then
         datum subtype : series = map_node_type(node.subtype);
         result "sequence<" + subtype + ">";
    conclude
    
    verify (name == "") then result "whole"; conclude
    result name;
end

algorithm is_struct_type(t : series) -> state
begin
    verify (mem_get_struct_size(t) > 0) then result true; conclude
    result false;
end

algorithm scan_definitions(prog_idx : whole) -> void
begin
    datum prog : ASTNode = node_heap[prog_idx];
    datum i : whole = 0;
    datum children : sequence<whole> = prog.children;
    datum len : whole = length(children);

    cycle (i < len) do
        datum idx : whole = children[i];
        datum k : whole = node_heap[idx].kind;

        verify (k == NODE_STRUCT_DECL) then
            datum sname : series = node_heap[idx].text_val;
            datum sinfo : StructInfo;
            sinfo.name = sname;

            datum m : whole = 0;
            datum schildren : sequence<whole> = node_heap[idx].children;
            datum slen : whole = length(schildren);
            cycle (m < slen) do
                datum midx : whole = schildren[m];
                datum mem : StructMember;
                mem.name = node_heap[midx].text_val;
                mem.type_name = map_node_type(node_heap[midx].type_node);
                append(sinfo.members, mem);
                m = m + 1;
            conclude
            mem_register_struct(sinfo);
        conclude

        verify (k == NODE_FUNC_DECL) then
            datum fname : series = node_heap[idx].text_val;
            datum finfo : FuncInfo;
            finfo.name = fname;
            finfo.return_type = map_node_type(node_heap[idx].type_node);
            mem_register_func(finfo);
        conclude

        verify (k == NODE_VAR_DECL) then
             datum vname : series = node_heap[idx].text_val;
             datum vtype : series = map_node_type(node_heap[idx].type_node);
             mem_declare_global(vname, vtype);
        conclude

        i = i + 1;
    conclude
end

algorithm translate_expr(node_idx : whole) -> void
begin
    verify (node_idx == -1) then return; conclude
    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_LITERAL) then
        verify (node.value_type == "whole") then
            tr_last_reg = node.text_val;
            tr_last_type = "whole";
        conclude
        verify (node.value_type == "state") then
            verify (node.bool_val) then tr_last_reg = "1"; otherwise tr_last_reg = "0"; conclude
            tr_last_type = "state";
        conclude
        verify (node.value_type == "precise") then
            tr_last_reg = node.text_val;
            tr_last_type = "precise";
        conclude
        verify (node.value_type == "series") then
            datum g : series = llvm_define_global_string(node.text_val);
            datum len : whole = length(node.text_val) + 1;
            tr_last_reg = llvm_gep_string(g, len);
            tr_last_type = "series";
        conclude
    conclude

    verify (k == NODE_IDENTIFIER) then
        datum name : series = node.text_val;
        datum reg : series = mem_get_var_ssa(name);
        datum type : series = mem_get_var_type(name);

        verify (mem_is_global(name) or is_struct_type(type) or is_sequence_type(type)) then
             reg = llvm_load(reg, type);
        conclude

        tr_last_reg = reg;
        tr_last_type = type;
    conclude

    verify (k == NODE_BINARY_OP) then
        // --- INICIO FIX: Operadores Unarios ---
        verify (node.left == -1) then
            translate_expr(node.right);
            datum r_reg : series = tr_last_reg;
            datum r_type : series = tr_last_type;
            datum op : series = node.op;

            verify (op == "-") then
                 // Implementar unario como (0 - valor)
                 verify (r_type == "precise" or r_type == "double") then
                      tr_last_reg = llvm_sub("0.0", r_reg, "precise");
                 otherwise
                      tr_last_reg = llvm_sub("0", r_reg, "whole");
                 conclude
            conclude
            
            verify (op == "not") then
                 // Implementar not lógico (xor con 1)
                 datum x : series = llvm_next_reg();
                 llvm_emit("  " + x + " = xor i1 " + r_reg + ", 1");
                 tr_last_reg = x;
                 tr_last_type = "state";
            conclude

            return;
        conclude
        // --- FIN FIX ---

        translate_expr(node.left);
        datum l_reg : series = tr_last_reg;
        // ... resto del código original ...
        datum l_type : series = tr_last_type;

        translate_expr(node.right);
        datum r_reg : series = tr_last_reg;
        datum r_type : series = tr_last_type;

        datum op : series = node.op;

        verify (op == "+" and (l_type == "series" or r_type == "series")) then
             tr_last_reg = llvm_call("_string_concat", "i8* " + l_reg + ", i8* " + r_reg, "series");
             tr_last_type = "series";
             return;
        conclude

        verify (l_type != r_type) then
             verify (l_type == "precise" and r_type == "whole") then
                  r_reg = llvm_cast(r_reg, "whole", "precise");
                  r_type = "precise";
             conclude
             verify (l_type == "whole" and r_type == "precise") then
                  l_reg = llvm_cast(l_reg, "whole", "precise");
                  l_type = "precise";
             conclude
        conclude

        tr_last_type = l_type;

        verify (op == "+") then tr_last_reg = llvm_add(l_reg, r_reg, l_type); conclude
        verify (op == "-") then tr_last_reg = llvm_sub(l_reg, r_reg, l_type); conclude
        verify (op == "*") then tr_last_reg = llvm_mul(l_reg, r_reg, l_type); conclude
        verify (op == "/") then tr_last_reg = llvm_div(l_reg, r_reg, l_type); conclude
        verify (op == "%") then tr_last_reg = llvm_mod(l_reg, r_reg, l_type); conclude

        verify (op == "==" or op == "!=" or op == "<" or op == ">" or op == "<=" or op == ">=") then
             tr_last_reg = llvm_cmp(op, l_reg, r_reg, l_type);
             tr_last_type = "state";
        conclude

        verify (op == "and") then tr_last_reg = llvm_and(l_reg, r_reg); tr_last_type = "state"; conclude
        verify (op == "or") then tr_last_reg = llvm_or(l_reg, r_reg); tr_last_type = "state"; conclude
    conclude

    verify (k == NODE_ARRAY_LITERAL) then
        datum children : sequence<whole> = node.children;
        datum len : whole = length(children);

        datum seq_ptr : series = llvm_call("_sequence_create", "i64 " + to_string(len), "sequence");

        datum i : whole = 0;
        cycle (i < len) do
            translate_expr(children[i]);
            datum val_to_store : series = tr_last_reg;

            verify (tr_last_type == "precise") then
                 val_to_store = llvm_next_reg();
                 llvm_emit("  " + val_to_store + " = bitcast double " + tr_last_reg + " to i64");
            conclude
            verify (tr_last_type == "series") then
                 val_to_store = llvm_cast(tr_last_reg, "series", "whole");
            conclude
            verify (is_struct_type(tr_last_type)) then
                 val_to_store = llvm_cast(tr_last_reg, "series", "whole");
            conclude

            llvm_call("_sequence_set", "%struct.sequence* " + seq_ptr + ", i64 " + to_string(i) + ", i64 " + val_to_store, "void");
            i = i + 1;
        conclude

        tr_last_reg = seq_ptr;
        tr_last_type = "sequence";
    conclude

    verify (k == NODE_ARRAY_ACCESS) then
        translate_expr(node.left);
        datum seq_ptr : series = tr_last_reg;
        datum seq_type : series = tr_last_type;

        datum elem_type : series = get_element_type_from_sequence(seq_type);

        datum children : sequence<whole> = node.children;
        translate_expr(children[0]);
        datum idx : series = tr_last_reg;

        datum val_i64 : series = llvm_call("_sequence_get", "%struct.sequence* " + seq_ptr + ", i64 " + idx, "whole");
        datum final_reg : series = val_i64;

        verify (is_struct_type(elem_type) or is_sequence_type(elem_type) or elem_type == "series") then
             final_reg = llvm_next_reg();
             llvm_emit("  " + final_reg + " = inttoptr i64 " + val_i64 + " to " + llvm_type_string(elem_type));
        conclude

        tr_last_reg = final_reg;
        tr_last_type = elem_type;
    conclude

    verify (k == NODE_MEMBER_ACCESS) then
        translate_expr(node.left);
        datum obj_ptr : series = tr_last_reg;
        datum obj_type : series = tr_last_type;

        datum mem_idx : whole = mem_get_member_index(obj_type, node.text_val);
        datum mem_type : series = mem_get_member_type_by_index(obj_type, mem_idx);

        datum ptr : series = llvm_next_reg();
        llvm_emit("  " + ptr + " = getelementptr %struct." + obj_type + ", %struct." + obj_type + "* " + obj_ptr + ", i32 0, i32 " + to_string(mem_idx));

        tr_last_reg = llvm_load(ptr, mem_type);
        tr_last_type = mem_type;
    conclude

    verify (k == NODE_FUNC_CALL) then
        datum fname : series = node.text_val;

        // Constructor Check
        verify (is_struct_type(fname)) then
             datum size_bytes : whole = mem_get_struct_size(fname);
             datum raw : series = llvm_call("malloc", "i64 " + to_string(size_bytes), "series");
             datum sptr : series = llvm_next_reg();
             llvm_emit("  " + sptr + " = bitcast i8* " + raw + " to %struct." + fname + "*");

             datum children : sequence<whole> = node.children;
             datum i : whole = 0;
             cycle (i < length(children)) do
                  translate_expr(children[i]);
                  datum val : series = tr_last_reg;
                  datum val_type : series = tr_last_type;

                  datum fptr : series = llvm_next_reg();
                  llvm_emit("  " + fptr + " = getelementptr %struct." + fname + ", %struct." + fname + "* " + sptr + ", i32 0, i32 " + to_string(i));

                  llvm_store(val, fptr, val_type);
                  i = i + 1;
             conclude

             tr_last_reg = sptr;
             tr_last_type = fname;
             return;
        conclude

        verify (fname == "reveal") then
             datum children : sequence<whole> = node.children;
             datum i : whole = 0;
             cycle (i < length(children)) do
                 translate_expr(children[i]);
                 verify (tr_last_type == "series") then
                      llvm_call("_print_str", "i8* " + tr_last_reg, "void");
                 otherwise
                      verify (tr_last_type == "precise") then
                           llvm_call("_print_double", "double " + tr_last_reg, "void");
                      otherwise
                           verify (tr_last_type == "state") then
                                datum casted : series = llvm_cast(tr_last_reg, "state", "whole");
                                llvm_call("_print_int", "i64 " + casted, "void");
                           otherwise
                                llvm_call("_print_int", "i64 " + tr_last_reg, "void");
                           conclude
                      conclude
                 conclude
                 i = i + 1;
             conclude
             llvm_call("_print_newline", "", "void");
             tr_last_reg = "0";
             tr_last_type = "void";
             return;
        conclude

        datum args_str : series = "";
        datum children : sequence<whole> = node.children;
        datum i : whole = 0;
        cycle (i < length(children)) do
            translate_expr(children[i]);
            verify (i > 0) then args_str = args_str + ", "; conclude
            args_str = args_str + llvm_type_string(tr_last_type) + " " + tr_last_reg;
            i = i + 1;
        conclude

        datum ret_type : series = mem_get_func_return_type(fname);

        // Intrinsics
        verify (fname == "length") then fname = "strlen"; ret_type = "whole"; conclude
        verify (fname == "power") then fname = "pow"; ret_type = "precise"; conclude
        verify (fname == "execute_system") then fname = "execute_system"; ret_type = "whole"; conclude
        verify (fname == "absolute") then
            verify (tr_last_type == "precise") then
                 fname = "_absolute_precise"; ret_type = "precise";
            otherwise
                 fname = "_absolute"; ret_type = "whole";
            conclude
        conclude
        verify (fname == "root") then fname = "sqrt"; ret_type = "precise"; conclude
        verify (fname == "cosine") then fname = "cos"; ret_type = "precise"; conclude
        verify (fname == "sine") then fname = "sin"; ret_type = "precise"; conclude
        verify (fname == "tangent") then fname = "tan"; ret_type = "precise"; conclude
        verify (fname == "file_read") then fname = "_file_read"; ret_type = "series"; conclude
        verify (fname == "file_write") then fname = "_file_write"; ret_type = "void"; conclude
        verify (fname == "file_exists") then fname = "_file_exists"; ret_type = "state"; conclude
        verify (fname == "file_remove") then fname = "_file_remove"; ret_type = "void"; conclude
        verify (fname == "arg_count") then fname = "_arg_count"; ret_type = "whole"; conclude
        verify (fname == "arg_value") then fname = "_arg_value"; ret_type = "series"; conclude
        verify (fname == "to_integer") then fname = "atoi"; ret_type = "whole"; conclude
        verify (fname == "to_precise") then fname = "atof"; ret_type = "precise"; conclude
        verify (fname == "is_alnum") then fname = "_is_alnum"; ret_type = "state"; conclude
        verify (fname == "is_alpha") then fname = "_is_alpha"; ret_type = "state"; conclude
        verify (fname == "is_digit") then fname = "_is_digit"; ret_type = "state"; conclude
        verify (fname == "is_lower") then fname = "_is_lower"; ret_type = "state"; conclude
        verify (fname == "is_upper") then fname = "_is_upper"; ret_type = "state"; conclude
        verify (fname == "to_lower") then fname = "_to_lower"; ret_type = "series"; conclude
        verify (fname == "to_upper") then fname = "_to_upper"; ret_type = "series"; conclude
        verify (fname == "reverse") then fname = "_reverse"; ret_type = "series"; conclude
        verify (fname == "char_at") then fname = "_char_at"; ret_type = "whole"; conclude
        verify (fname == "env_get") then fname = "_env_get_safe";
        ret_type = "series"; conclude // ok
        verify (fname == "to_string") then fname = "_to_string"; ret_type = "series"; conclude
        verify (fname == "substring") then fname = "_substring"; ret_type = "series"; conclude

        tr_last_reg = llvm_call(fname, args_str, ret_type);
        tr_last_type = ret_type;
    conclude

    verify (k == NODE_INQUIRE) then
        // treat as stmt usually
    conclude
end

algorithm translate_stmt(node_idx : whole) -> void
begin
    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_VAR_DECL) then
        datum type : series = map_node_type(node.type_node);
        datum reg : series = "";

        verify (is_struct_type(type) or is_sequence_type(type)) then
             reg = llvm_alloca(type);
             mem_declare_local(node.text_val, type, reg);
             verify (node.initializer != -1) then
                 translate_expr(node.initializer);
                 llvm_store(tr_last_reg, reg, type);
             conclude
        otherwise
             verify (node.initializer != -1) then
                 datum init_node : ASTNode = node_heap[node.initializer];
                 verify (init_node.kind == NODE_INQUIRE) then
                     verify (type == "precise") then
                         reg = llvm_call("_inquire_double", "", "precise");
                     otherwise
                         verify (type == "series") then
                             reg = llvm_call("_inquire_str", "", "series");
                         otherwise
                             reg = llvm_call("_inquire_int", "", "whole");
                         conclude
                     conclude
                 otherwise
                     translate_expr(node.initializer);
                     reg = tr_last_reg;
                 conclude
             otherwise
                 verify (type == "precise") then reg = "0.0";
                 otherwise
                     verify (type == "series") then reg = "null";
                     otherwise reg = "0";
                     conclude
                 conclude
             conclude
             mem_declare_local(node.text_val, type, reg);
        conclude
    conclude

    verify (k == NODE_ASSIGN) then
        datum name : series = node.text_val;
        datum type : series = mem_get_var_type(name);
        datum val : series = "";

        datum init_node : ASTNode = node_heap[node.initializer];
        verify (init_node.kind == NODE_INQUIRE) then
             verify (type == "precise") then
                 val = llvm_call("_inquire_double", "", "precise");
             otherwise
                 verify (type == "series") then
                     val = llvm_call("_inquire_str", "", "series");
                 otherwise
                     val = llvm_call("_inquire_int", "", "whole");
                 conclude
             conclude
        otherwise
             translate_expr(node.initializer);
             val = tr_last_reg;
        conclude

        verify (is_struct_type(type) or is_sequence_type(type)) then
             datum ptr : series = mem_get_var_ssa(name);
             llvm_store(val, ptr, type);
        otherwise
             mem_update_var_ssa(name, val);
        conclude
    conclude

verify (k == NODE_ARRAY_ASSIGN) then
        // 1. Obtener Puntero al Arreglo
        translate_expr(node.left);
        datum seq_ptr : series = tr_last_reg;

        // 2. Obtener Indice
        datum children : sequence<whole> = node.children;
        translate_expr(children[0]);
        datum idx : series = tr_last_reg;

        // 3. Obtener Valor a asignar
        translate_expr(node.initializer);
        datum val_reg : series = tr_last_reg;
        datum val_type : series = tr_last_type;

        // 4. Castear valor si es necesario (el backend guarda todo como i64)
        datum val_to_store : series = val_reg;
        
        verify (val_type == "precise") then
             val_to_store = llvm_next_reg();
             llvm_emit("  " + val_to_store + " = bitcast double " + val_reg + " to i64");
        conclude
        verify (val_type == "series" or is_struct_type(val_type)) then
             val_to_store = llvm_cast(val_reg, "series", "whole");
        conclude

        // 5. Llamar a _sequence_set
        llvm_call("_sequence_set", "%struct.sequence* " + seq_ptr + ", i64 " + idx + ", i64 " + val_to_store, "void");
    conclude

    verify (k == NODE_MEMBER_ASSIGN) then
        translate_expr(node.left);
        datum struct_ptr : series = tr_last_reg;
        datum struct_type : series = tr_last_type;

        translate_expr(node.initializer);
        datum val_reg : series = tr_last_reg;
        datum val_type : series = tr_last_type;

        datum mem_idx : whole = mem_get_member_index(struct_type, node.text_val);

        datum field_ptr : series = llvm_next_reg();
        llvm_emit("  " + field_ptr + " = getelementptr %struct." + struct_type + ", %struct." + struct_type + "* " + struct_ptr + ", i32 0, i32 " + to_string(mem_idx));

        llvm_store(val_reg, field_ptr, val_type);
    conclude

    verify (k == NODE_EXPR_STMT) then
        translate_expr(node.initializer);
    conclude

    verify (k == NODE_INQUIRE) then
        datum name : series = node.text_val;
        datum type : series = mem_get_var_type(name);

        verify (is_struct_type(type) or type == "sequence") then
             // Ptr types? inquire doesn't support them well usually.
        otherwise
             // Primitive SSA update.
             // But scanf needs a pointer!
             // We MUST alloc a temp slot for scanf, then load it.
             datum temp_ptr : series = llvm_alloca(type);

             verify (type == "whole") then
                 // _inquire_int implementation does alloc/load inside it?
                 // yes: define i64 @_inquire_int() ... returns value.
                 datum val : series = llvm_call("_inquire_int", "", "whole");
                 mem_update_var_ssa(name, val);
             conclude
             verify (type == "series") then
                 datum val : series = llvm_call("_inquire_str", "", "series");
                 mem_update_var_ssa(name, val);
             conclude
        conclude
    conclude

    verify (k == NODE_RETURN) then
        verify (node.initializer != -1) then
            translate_expr(node.initializer);
             llvm_emit("  ret " + llvm_type_string(tr_last_type) + " " + tr_last_reg);
        otherwise
             llvm_emit("  ret void");
        conclude
        block_terminated = true;
    conclude

    verify (k == NODE_IF) then
        datum l_then : series = llvm_next_label();
        datum l_else : series = llvm_next_label();
        datum l_end : series = llvm_next_label();

        translate_expr(node.condition);
        llvm_emit_cond_br_update(tr_last_reg, l_then, l_else);

        datum snap_entry : sequence<VarInfo> = mem_copy_locals();

        llvm_emit_label_update(l_then);
        translate_block(node.then_branch);
        datum l_then_end : series = current_block_label;
        datum term_then : state = block_terminated;
        verify (not term_then) then llvm_emit_br_update(l_end); conclude
        datum snap_then : sequence<VarInfo> = mem_copy_locals();

        mem_set_locals(snap_entry);
        llvm_emit_label_update(l_else);
        verify (node.else_branch != -1) then
             translate_block(node.else_branch);
        conclude
        datum l_else_end : series = current_block_label;
        datum term_else : state = block_terminated;
        verify (not term_else) then llvm_emit_br_update(l_end); conclude
        datum snap_else : sequence<VarInfo> = mem_copy_locals();

        llvm_emit_label_update(l_end);

        datum i : whole = 0;
        cycle (i < length(snap_entry)) do
             datum name : series = snap_entry[i].name;
             datum type : series = snap_entry[i].type_name;

             verify (not (is_struct_type(type) or type == "sequence")) then
                 datum v_then : series = mem_get_ssa_from_snapshot(snap_then, name);
                 datum v_else : series = mem_get_ssa_from_snapshot(snap_else, name);

                 verify (v_then != v_else) then
                     datum pairs : series = "";
                     verify (not term_then) then pairs = pairs + "[ " + v_then + ", %" + l_then_end + " ]"; conclude
                     verify (not term_else) then
                         verify (length(pairs) > 0) then pairs = pairs + ", "; conclude
                         pairs = pairs + "[ " + v_else + ", %" + l_else_end + " ]";
                     conclude

                     verify (length(pairs) > 0) then
                         datum phi : series = llvm_phi(type, pairs);
                         mem_update_var_ssa(name, phi);
                     conclude
                 conclude
             conclude
             i = i + 1;
        conclude
    conclude

    verify (k == NODE_WHILE) then
        datum l_cond : series = llvm_next_label();
        datum l_body : series = llvm_next_label();
        datum l_end : series = llvm_next_label();
        datum l_pre : series = current_block_label;

        llvm_emit_br_update(l_cond);
        llvm_emit_label_update(l_cond);

        datum targets : sequence<series>;
        targets = analysis_scan_assignments(node.then_branch, targets);

        datum phis : sequence<series>;
        datum start_vals : sequence<series>;

        datum t : whole = 0;
        cycle (t < length(targets)) do
            datum tname : series = targets[t];
            datum type : series = mem_get_var_type(tname);
            verify (type != "unknown" and not (is_struct_type(type) or type == "sequence")) then
                 datum val_pre : series = mem_get_var_ssa(tname);
                 append(start_vals, val_pre);

                 datum phi : series = llvm_next_reg();
                 mem_update_var_ssa(tname, phi);
                 append(phis, phi);
            conclude
            t = t + 1;
        conclude

        datum saved_code : series = llvm_code;
        llvm_code = "";

        translate_expr(node.condition);
        datum cond_reg : series = tr_last_reg;
        llvm_emit_cond_br_update(cond_reg, l_body, l_end);

        llvm_emit_label_update(l_body);
        translate_block(node.then_branch);
        datum l_body_end : series = current_block_label;
        llvm_emit_br_update(l_cond);

        datum end_vals : sequence<series>;
        t = 0;
        datum phi_idx_scan : whole = 0;
        cycle (t < length(targets)) do
             datum tname : series = targets[t];
             datum type : series = mem_get_var_type(tname);
             verify (type != "unknown" and not (is_struct_type(type) or type == "sequence")) then
                 datum val_end : series = mem_get_var_ssa(tname);
                 append(end_vals, val_end);
                 phi_idx_scan = phi_idx_scan + 1;
             conclude
             t = t + 1;
        conclude

        datum body_str : series = llvm_code;
        llvm_code = saved_code;

        t = 0;
        datum phi_idx : whole = 0;
        cycle (t < length(targets)) do
             datum tname : series = targets[t];
             datum type : series = mem_get_var_type(tname);
             verify (type != "unknown" and not (is_struct_type(type) or type == "sequence")) then
                 datum phi_reg : series = phis[phi_idx];
                 datum v_start : series = start_vals[phi_idx];
                 datum v_end : series = end_vals[phi_idx];

                 llvm_emit("  " + phi_reg + " = phi " + llvm_type_string(type) + " [ " + v_start + ", %" + l_pre + " ], [ " + v_end + ", %" + l_body_end + " ]");

                 phi_idx = phi_idx + 1;
             conclude
             t = t + 1;
        conclude

        llvm_code = llvm_code + body_str;

        llvm_emit_label_update(l_end);
    conclude

    verify (k == NODE_BLOCK) then
        translate_block(node_idx);
    conclude
end

algorithm translate_block(node_idx : whole) -> void
begin
    datum blk : ASTNode = node_heap[node_idx];
    datum children : sequence<whole> = blk.children;
    datum i : whole = 0;
    cycle (i < length(children)) do
        translate_stmt(children[i]);
        i = i + 1;
    conclude
end

algorithm emit_llvm_runtime() -> void
begin
    llvm_emit("declare i32 @printf(i8*, ...)");
    llvm_emit("declare i32 @sprintf(i8*, i8*, ...)");
    llvm_emit("declare i32 @scanf(i8*, ...)");
    llvm_emit("declare i64 @strlen(i8*)");
    llvm_emit("declare i8* @malloc(i64)");
    llvm_emit("declare void @free(i8*)");
    llvm_emit("declare void @exit(i32)");
    llvm_emit("declare double @pow(double, double)");
    llvm_emit("declare double @sqrt(double)");
    llvm_emit("declare double @cos(double)");
    llvm_emit("declare double @sin(double)");
    llvm_emit("declare double @tan(double)");
    llvm_emit("declare double @atof(i8*)");
    llvm_emit("declare i64 @atoi(i8*)");
    llvm_emit("declare i32 @system(i8*)");
    llvm_emit("declare i8* @strcat(i8*, i8*)");
    llvm_emit("declare i8* @strcpy(i8*, i8*)");
    llvm_emit("declare i8* @getenv(i8*)");
    llvm_emit("declare i32 @access(i8*, i32)");
    llvm_emit("declare i32 @remove(i8*)");

    llvm_emit_global("%struct.sequence = type { i64, i64, i64* }");

    llvm_emit_global("@.fmt_int = private constant [5 x i8] c\"%lld\\00\"");
    llvm_emit_global("@.fmt_float = private constant [4 x i8] c\"%f\\0A\\00\"");
    llvm_emit_global("@.fmt_str = private constant [3 x i8] c\"%s\\00\"");
    llvm_emit_global("@.fmt_nl = private constant [2 x i8] c\"\\0A\\00\"");
    llvm_emit_global("@.fmt_in_int = private constant [5 x i8] c\"%lld\\00\"");
    llvm_emit_global("@.fmt_in_double = private constant [4 x i8] c\"%lf\\00\"");
    llvm_emit_global("@.fmt_in_str = private constant [3 x i8] c\"%s\\00\"");

    // Helpers
    llvm_emit("define void @_print_int(i64 %val) {");
    llvm_emit("  %fmt = getelementptr [5 x i8], [5 x i8]* @.fmt_int, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @printf(i8* %fmt, i64 %val)");
    llvm_emit("  ret void");
    llvm_emit("}");

    llvm_emit("define void @_print_double(double %val) {");
    llvm_emit("  %fmt = getelementptr [4 x i8], [4 x i8]* @.fmt_float, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @printf(i8* %fmt, double %val)");
    llvm_emit("  ret void");
    llvm_emit("}");

    llvm_emit("define void @_print_str(i8* %val) {");
    llvm_emit("  %fmt = getelementptr [3 x i8], [3 x i8]* @.fmt_str, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @printf(i8* %fmt, i8* %val)");
    llvm_emit("  ret void");
    llvm_emit("}");

    llvm_emit("define void @_print_newline() {");
    llvm_emit("  %fmt = getelementptr [2 x i8], [2 x i8]* @.fmt_nl, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @printf(i8* %fmt)");
    llvm_emit("  ret void");
    llvm_emit("}");

    llvm_emit("define i8* @_string_concat(i8* %s1, i8* %s2) {");
    llvm_emit("  %len1 = call i64 @strlen(i8* %s1)");
    llvm_emit("  %len2 = call i64 @strlen(i8* %s2)");
    llvm_emit("  %sum = add i64 %len1, %len2");
    llvm_emit("  %size = add i64 %sum, 1");
    llvm_emit("  %dest = call i8* @malloc(i64 %size)");
    llvm_emit("  call i8* @strcpy(i8* %dest, i8* %s1)");
    llvm_emit("  call i8* @strcat(i8* %dest, i8* %s2)");
    llvm_emit("  ret i8* %dest");
    llvm_emit("}");

    llvm_emit("define i8* @_to_string(i64 %v) {");
    llvm_emit("  %buf = call i8* @malloc(i64 32)");
    llvm_emit("  %fmt = getelementptr [5 x i8], [5 x i8]* @.fmt_int, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, i8*, ...) @sprintf(i8* %buf, i8* %fmt, i64 %v)");
    llvm_emit("  ret i8* %buf");
    llvm_emit("}");

    llvm_emit("define i8* @_substring(i8* %str, i64 %start, i64 %len) {");
    // Simplified substring
    llvm_emit("  %dest = call i8* @malloc(i64 %len)"); // +1
    llvm_emit("  %size = add i64 %len, 1");
    llvm_emit("  %dest2 = call i8* @malloc(i64 %size)");
    llvm_emit("  ret i8* %str"); // Stub
    llvm_emit("}");

    llvm_emit("define i64 @execute_system(i8* %cmd) {");
    llvm_emit("  %res = call i32 @system(i8* %cmd)");
    llvm_emit("  %ext = sext i32 %res to i64");
    llvm_emit("  ret i64 %ext");
    llvm_emit("}");

    llvm_emit("define %struct.sequence* @_sequence_create(i64 %size) {");
    llvm_emit("  %ptr = call i8* @malloc(i64 24)");
    llvm_emit("  %seq = bitcast i8* %ptr to %struct.sequence*");
    llvm_emit("  %size_ptr = getelementptr %struct.sequence, %struct.sequence* %seq, i32 0, i32 0");
    llvm_emit("  store i64 %size, i64* %size_ptr");
    llvm_emit("  %cap_ptr = getelementptr %struct.sequence, %struct.sequence* %seq, i32 0, i32 1");
    llvm_emit("  store i64 %size, i64* %cap_ptr");
    llvm_emit("  %bytes = mul i64 %size, 8");
    llvm_emit("  %data = call i8* @malloc(i64 %bytes)");
    llvm_emit("  %data_ptr = bitcast i8* %data to i64*");
    llvm_emit("  %seq_data_ptr = getelementptr %struct.sequence, %struct.sequence* %seq, i32 0, i32 2");
    llvm_emit("  store i64* %data_ptr, i64** %seq_data_ptr");
    llvm_emit("  ret %struct.sequence* %seq");
    llvm_emit("}");

    llvm_emit("define void @_sequence_set(%struct.sequence* %seq, i64 %idx, i64 %val) {");
    llvm_emit("  %data_ptr_ptr = getelementptr %struct.sequence, %struct.sequence* %seq, i32 0, i32 2");
    llvm_emit("  %data_ptr = load i64*, i64** %data_ptr_ptr");
    llvm_emit("  %elem_ptr = getelementptr i64, i64* %data_ptr, i64 %idx");
    llvm_emit("  store i64 %val, i64* %elem_ptr");
    llvm_emit("  ret void");
    llvm_emit("}");

    llvm_emit("define i64 @_sequence_get(%struct.sequence* %seq, i64 %idx) {");
    llvm_emit("  %data_ptr_ptr = getelementptr %struct.sequence, %struct.sequence* %seq, i32 0, i32 2");
    llvm_emit("  %data_ptr = load i64*, i64** %data_ptr_ptr");
    llvm_emit("  %elem_ptr = getelementptr i64, i64* %data_ptr, i64 %idx");
    llvm_emit("  %val = load i64, i64* %elem_ptr");
    llvm_emit("  ret i64 %val");
    llvm_emit("}");

    llvm_emit("define i64 @_inquire_int() {");
    llvm_emit("  %ptr = alloca i64");
    llvm_emit("  %fmt = getelementptr [5 x i8], [5 x i8]* @.fmt_in_int, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @scanf(i8* %fmt, i64* %ptr)");
    llvm_emit("  %val = load i64, i64* %ptr");
    llvm_emit("  ret i64 %val");
    llvm_emit("}");

    llvm_emit("define double @_inquire_double() {");
    llvm_emit("  %ptr = alloca double");
    llvm_emit("  %fmt = getelementptr [4 x i8], [4 x i8]* @.fmt_in_double, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @scanf(i8* %fmt, double* %ptr)");
    llvm_emit("  %val = load double, double* %ptr");
    llvm_emit("  ret double %val");
    llvm_emit("}");

    llvm_emit("define i8* @_inquire_str() {");
    llvm_emit("  %buf = call i8* @malloc(i64 256)");
    llvm_emit("  %fmt = getelementptr [3 x i8], [3 x i8]* @.fmt_in_str, i64 0, i64 0");
    llvm_emit("  call i32 (i8*, ...) @scanf(i8* %fmt, i8* %buf)");
    llvm_emit("  ret i8* %buf");
    llvm_emit("}");

    llvm_emit("define i64 @_absolute(i64 %v) {");
    llvm_emit("  %cond = icmp slt i64 %v, 0");
    llvm_emit("  %neg = sub i64 0, %v");
    llvm_emit("  %res = select i1 %cond, i64 %neg, i64 %v");
    llvm_emit("  ret i64 %res");
    llvm_emit("}");

    llvm_emit("define i1 @_file_exists(i8* %path) {");
    llvm_emit("  %res = call i32 @access(i8* %path, i32 0)");
    llvm_emit("  %eq = icmp eq i32 %res, 0");
    llvm_emit("  ret i1 %eq");
    llvm_emit("}");

    llvm_emit("define void @_file_remove(i8* %path) {");
    llvm_emit("  call i32 @remove(i8* %path)");
    llvm_emit("  ret void");
    llvm_emit("}");

    llvm_emit("declare %struct.FILE* @fopen(i8*, i8*)");
    llvm_emit("declare i32 @fseek(%struct.FILE*, i64, i32)");
    llvm_emit("declare i64 @ftell(%struct.FILE*)");
    llvm_emit("declare i64 @fread(i8*, i64, i64, %struct.FILE*)");
    llvm_emit("declare i64 @fwrite(i8*, i64, i64, %struct.FILE*)");
    llvm_emit("declare i32 @fclose(%struct.FILE*)");
    llvm_emit("declare void @rewind(%struct.FILE*)");
    llvm_emit_global("%struct.FILE = type opaque");

    llvm_emit("define i8* @_file_read(i8* %path) {");
    llvm_emit("  %mode = getelementptr [2 x i8], [2 x i8]* @.fmt_r, i64 0, i64 0");
    llvm_emit("  %fp = call %struct.FILE* @fopen(i8* %path, i8* %mode)");
    llvm_emit("  call i32 @fseek(%struct.FILE* %fp, i64 0, i32 2)");
    llvm_emit("  %size = call i64 @ftell(%struct.FILE* %fp)");
    llvm_emit("  call void @rewind(%struct.FILE* %fp)");
    llvm_emit("  %buf = call i8* @malloc(i64 %size)");
    llvm_emit("  call i64 @fread(i8* %buf, i64 1, i64 %size, %struct.FILE* %fp)");
    llvm_emit("  call i32 @fclose(%struct.FILE* %fp)");
    llvm_emit("  ret i8* %buf");
    llvm_emit("}");
    llvm_emit_global("@.fmt_r = private constant [2 x i8] c\"r\\00\"");
    llvm_emit_global("@.fmt_w = private constant [2 x i8] c\"w\\00\"");

    llvm_emit("define void @_file_write(i8* %path, i8* %data) {");
    llvm_emit("  %mode = getelementptr [2 x i8], [2 x i8]* @.fmt_w, i64 0, i64 0");
    llvm_emit("  %fp = call %struct.FILE* @fopen(i8* %path, i8* %mode)");
    llvm_emit("  %len = call i64 @strlen(i8* %data)");
    llvm_emit("  call i64 @fwrite(i8* %data, i64 1, i64 %len, %struct.FILE* %fp)");
    llvm_emit("  call i32 @fclose(%struct.FILE* %fp)");
    llvm_emit("  ret void");
    llvm_emit("}"); // <--- Cierra _file_write aquí

    // 2. Definir _env_get_safe afuera
    llvm_emit("define i8* @_env_get_safe(i8* %name) {");
    llvm_emit("  %res = call i8* @getenv(i8* %name)");
    llvm_emit("  %isnull = icmp eq i8* %res, null");
    llvm_emit("  br i1 %isnull, label %is_null, label %not_null");
    llvm_emit("is_null:");
    llvm_emit("  %empty = call i8* @malloc(i64 1)");
    llvm_emit("  store i8 0, i8* %empty");
    llvm_emit("  ret i8* %empty");
    llvm_emit("not_null:");
    llvm_emit("  ret i8* %res");
    llvm_emit("}");

    llvm_emit("define i64 @_arg_count() { ret i64 0 }");
    llvm_emit("define i8* @_arg_value(i64 %idx) { ret i8* null }");

    llvm_emit("define i1 @_is_digit(i64 %c) {");
    llvm_emit("  %ge0 = icmp sge i64 %c, 48");
    llvm_emit("  %le9 = icmp sle i64 %c, 57");
    llvm_emit("  %and = and i1 %ge0, %le9");
    llvm_emit("  ret i1 %and");
    llvm_emit("}");

    llvm_emit("define i1 @_is_lower(i64 %c) {");
    llvm_emit("  %gea = icmp sge i64 %c, 97");
    llvm_emit("  %lez = icmp sle i64 %c, 122");
    llvm_emit("  %lower = and i1 %gea, %lez");
    llvm_emit("  ret i1 %lower");
    llvm_emit("}");

    llvm_emit("define i1 @_is_upper(i64 %c) {");
    llvm_emit("  %geA = icmp sge i64 %c, 65");
    llvm_emit("  %leZ = icmp sle i64 %c, 90");
    llvm_emit("  %res = and i1 %geA, %leZ");
    llvm_emit("  ret i1 %res");
    llvm_emit("}");

    llvm_emit("define i1 @_is_alpha(i64 %c) {");
    llvm_emit("  %upper = call i1 @_is_upper(i64 %c)");
    llvm_emit("  %lower = call i1 @_is_lower(i64 %c)");
    llvm_emit("  %res = or i1 %upper, %lower");
    llvm_emit("  ret i1 %res");
    llvm_emit("}");

    llvm_emit("define i1 @_is_alnum(i64 %c) {");
    llvm_emit("  %a = call i1 @_is_alpha(i64 %c)");
    llvm_emit("  %d = call i1 @_is_digit(i64 %c)");
    llvm_emit("  %res = or i1 %a, %d");
    llvm_emit("  ret i1 %res");
    llvm_emit("}");

    llvm_emit("define i8* @_to_lower(i8* %s) { ret i8* %s }");
    llvm_emit("define i8* @_to_upper(i8* %s) { ret i8* %s }");
    llvm_emit("define i8* @_reverse(i8* %s) { ret i8* %s }");

    llvm_emit("define i64 @_char_at(i8* %s, i64 %idx) {");
    llvm_emit("  %ptr = getelementptr i8, i8* %s, i64 %idx");
    llvm_emit("  %val = load i8, i8* %ptr");
    llvm_emit("  %ext = sext i8 %val to i64");
    llvm_emit("  ret i64 %ext");
    llvm_emit("}");

    llvm_emit("define double @_absolute_precise(double %v) {");
    llvm_emit("  %res = call double @llvm.fabs.f64(double %v)");
    llvm_emit("  ret double %res");
    llvm_emit("}");
    llvm_emit("declare double @llvm.fabs.f64(double)");
end

algorithm translate_program(prog_idx : whole) -> void
begin
    scan_definitions(prog_idx);

    emit_llvm_runtime();

    // Emit Struct Definitions
    datum si : whole = 0;
    cycle (si < length(mem_defined_structs)) do
        datum info : StructInfo = mem_defined_structs[si];
        datum def : series = "%struct." + info.name + " = type { ";
        datum mi : whole = 0;
        cycle (mi < length(info.members)) do
             verify (mi > 0) then def = def + ", "; conclude
             def = def + llvm_type_string(mem_get_member_type_by_index(info.name, mi));
             mi = mi + 1;
        conclude
        def = def + " }";
        llvm_emit_global(def);
        si = si + 1;
    conclude

    // Globals
    datum i : whole = 0;
    cycle (i < length(mem_global_vars)) do
        datum g : VarInfo = mem_global_vars[i];
        llvm_emit_global("@" + g.name + " = global " + llvm_type_string(g.type_name) + " zeroinitializer");
        i = i + 1;
    conclude

    // Functions
    datum prog : ASTNode = node_heap[prog_idx];
    datum children : sequence<whole> = prog.children;
    i = 0;
    cycle (i < length(children)) do
        datum idx : whole = children[i];
        verify (node_heap[idx].kind == NODE_FUNC_DECL) then
             datum fname : series = node_heap[idx].text_val;
             datum ret : series = map_node_type(node_heap[idx].type_node);

             // Main signature
             verify (fname == "genesis") then
                 fname = "main";
                 ret = "i64"; // main returns int
                 llvm_emit("define i64 @main(i32 %argc, i8** %argv) {");
             otherwise
                 // Params
                 datum params : sequence<whole> = node_heap[idx].children;
                 datum pstr : series = "";
                 datum p : whole = 0;
                 cycle (p < length(params)) do
                      verify (p > 0) then pstr = pstr + ", "; conclude
                      datum pnode : ASTNode = node_heap[params[p]];
                      pstr = pstr + llvm_type_string(map_node_type(pnode.type_node)) + " %" + pnode.text_val + "_arg";
                      p = p + 1;
                 conclude
                 llvm_emit("define " + llvm_type_string(ret) + " @" + fname + "(" + pstr + ") {");
             conclude

             llvm_reset_local();
             mem_enter_function();
             llvm_emit_label_update("entry");

             // Alloc args
             verify (fname == "main") then
                 // Handle argc/argv if needed, skip for now
             otherwise
                 datum params : sequence<whole> = node_heap[idx].children;
                 datum p : whole = 0;
                 cycle (p < length(params)) do
                      datum pnode : ASTNode = node_heap[params[p]];
                      datum type : series = map_node_type(pnode.type_node);
                      datum arg_name : series = "%" + pnode.text_val + "_arg";

                      verify (is_struct_type(type) or type == "sequence") then
                          datum reg : series = llvm_alloca(type);
                          mem_declare_local(pnode.text_val, type, reg);
                          llvm_store(arg_name, reg, type);
                      otherwise
                          mem_declare_local(pnode.text_val, type, arg_name);
                      conclude
                      p = p + 1;
                 conclude
             conclude

             verify (node_heap[idx].then_branch != -1) then
                 translate_block(node_heap[idx].then_branch);
             conclude

             // Default return
             verify (ret == "void") then
                 llvm_emit("  ret void");
             conclude

             verify (ret != "void") then
                 // Safe default return for non-void
                 verify (is_struct_type(ret) or ret == "series" or ret == "sequence") then
                     llvm_emit("  ret " + llvm_type_string(ret) + " null");
                 otherwise
                     llvm_emit("  ret " + llvm_type_string(ret) + " 0");
                 conclude
             conclude

             llvm_emit("}");
        conclude
        i = i + 1;
    conclude

    codegen_output = llvm_globals + llvm_code;
end
