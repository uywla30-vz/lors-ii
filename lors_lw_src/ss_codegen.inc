// Code Generator State (Assembly x86-64 GAS)
datum codegen_output : series = "";
datum label_counter : whole = 0;
datum loop_stack_start : sequence<whole>;
datum loop_stack_end : sequence<whole>;

// Symbol Table
structure VarInfo
begin
    datum name : series;
    datum offset : whole;
    datum is_global : state;
    datum type_name : series; // Track type for member access
end

structure StructMember
begin
    datum name : series;
    datum type_name : series;
end

structure StructInfo
begin
    datum name : series;
    datum members : sequence<StructMember>;
end

structure FuncInfo
begin
    datum name : series;
    datum return_type : series;
end

datum current_locals : sequence<VarInfo>;
datum current_stack_size : whole = 0;
datum global_vars : sequence<VarInfo>;
datum defined_structs : sequence<StructInfo>;
datum defined_funcs : sequence<FuncInfo>;
datum last_expr_type : series = ""; // Type of last expression evaluated

// Forward declarations
algorithm emit(line : series) -> void;
algorithm gen_expression(node_idx : whole) -> void;
algorithm visit_stmt(node_idx : whole) -> void;
algorithm visit_block(node_idx : whole) -> void;
algorithm generate_code(prog_idx : whole) -> void;
algorithm get_var_offset(name : series) -> whole;
algorithm get_var_type(name : series) -> series;
algorithm is_global_var(name : series) -> state;
algorithm new_label() -> series;
algorithm collect_definitions(prog_idx : whole) -> void;
algorithm get_member_offset(struct_name : series, member_name : series) -> whole;
algorithm get_struct_size(struct_name : series) -> whole;
algorithm map_type_node_to_string(node_idx : whole) -> series;

algorithm emit(line : series) -> void
begin
    codegen_output = codegen_output + line + character(10);
end

algorithm new_label() -> series
begin
    datum l : series = ".L" + to_string(label_counter);
    label_counter = label_counter + 1;
    result l;
end

algorithm get_var_offset(name : series) -> whole
begin
    datum i : whole = 0;
    datum len : whole = length(current_locals);
    cycle (i < len) do
        verify (current_locals[i].name == name) then
            result current_locals[i].offset;
        conclude
        i = i + 1;
    conclude
    result 0;
end

algorithm get_var_type(name : series) -> series
begin
    datum i : whole = 0;
    datum len : whole = length(current_locals);
    cycle (i < len) do
        verify (current_locals[i].name == name) then
            result current_locals[i].type_name;
        conclude
        i = i + 1;
    conclude

    // Global
    i = 0;
    len = length(global_vars);
    cycle (i < len) do
        verify (global_vars[i].name == name) then
            result global_vars[i].type_name;
        conclude
        i = i + 1;
    conclude
    result "unknown";
end

algorithm is_global_var(name : series) -> state
begin
    datum i : whole = 0;
    datum len : whole = length(global_vars);
    cycle (i < len) do
        verify (global_vars[i].name == name) then
            result true;
        conclude
        i = i + 1;
    conclude
    result false;
end

algorithm get_member_offset(struct_name : series, member_name : series) -> whole
begin
    datum i : whole = 0;
    datum len : whole = length(defined_structs);
    cycle (i < len) do
        verify (defined_structs[i].name == struct_name) then
            datum members : sequence<StructMember> = defined_structs[i].members;
            datum j : whole = 0;
            datum mlen : whole = length(members);
            cycle (j < mlen) do
                verify (members[j].name == member_name) then
                    result j * 8; // Assume 8 byte alignment
                conclude
                j = j + 1;
            conclude
        conclude
        i = i + 1;
    conclude
    result 0;
end

algorithm get_struct_size(struct_name : series) -> whole
begin
    datum i : whole = 0;
    datum len : whole = length(defined_structs);
    cycle (i < len) do
        verify (defined_structs[i].name == struct_name) then
            result length(defined_structs[i].members) * 8;
        conclude
        i = i + 1;
    conclude
    result 0;
end

algorithm map_type_node_to_string(node_idx : whole) -> series
begin
    datum name : series = node_heap[node_idx].text_val;
    verify (name == "sequence") then
        result "sequence<" + map_type_node_to_string(node_heap[node_idx].subtype) + ">";
    conclude
    result name;
end

algorithm collect_definitions(prog_idx : whole) -> void
begin
    datum prog : ASTNode = node_heap[prog_idx];
    datum i : whole = 0;
    datum children : sequence<whole> = prog.children;
    datum len : whole = length(children);

    cycle (i < len) do
        datum decl_idx : whole = children[i];
        datum k : whole = node_heap[decl_idx].kind;

        verify (k == NODE_STRUCT_DECL) then
            datum sname : series = node_heap[decl_idx].text_val;
            datum sinfo : StructInfo;
            sinfo.name = sname;

            datum m : whole = 0;
            datum schildren : sequence<whole> = node_heap[decl_idx].children;
            datum slen : whole = length(schildren);
            cycle (m < slen) do
                datum midx : whole = schildren[m];
                // Assume VarDecl
                datum mem : StructMember;
                mem.name = node_heap[midx].text_val;
                mem.type_name = map_type_node_to_string(node_heap[midx].type_node);
                append(sinfo.members, mem);
                m = m + 1;
            conclude
            append(defined_structs, sinfo);
        conclude

        verify (k == NODE_FUNC_DECL) then
            datum fname : series = node_heap[decl_idx].text_val;
            datum finfo : FuncInfo;
            finfo.name = fname;
            finfo.return_type = map_type_node_to_string(node_heap[decl_idx].type_node);
            append(defined_funcs, finfo);
        conclude

        verify (k == NODE_VAR_DECL) then
            datum vname : series = node_heap[decl_idx].text_val;
            datum vtype : series = map_type_node_to_string(node_heap[decl_idx].type_node);
            datum vinfo : VarInfo = VarInfo(vname, 0, true, vtype);
            append(global_vars, vinfo);
        conclude

        i = i + 1;
    conclude
end

algorithm gen_expression(node_idx : whole) -> void
begin
    verify (node_idx == -1) then return; conclude

    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_LITERAL) then
        verify (node.value_type == "whole") then
            emit("    push $" + node.text_val);
            last_expr_type = "whole";
        conclude
        verify (node.value_type == "series") then
            datum lbl : series = new_label();
            emit("    .section .rodata");
            emit(lbl + ": .string " + character(34) + node.text_val + character(34));
            emit("    .text");
            emit("    leaq " + lbl + "(%rip), %rax");
            emit("    push %rax");
            last_expr_type = "series";
        conclude
        verify (node.value_type == "state") then
            verify (node.bool_val) then emit("    push $1"); otherwise emit("    push $0"); conclude
            last_expr_type = "state";
        conclude
        verify (node.value_type == "precise") then
             datum lbl : series = new_label();
             emit("    .section .rodata");
             emit(lbl + ": .double " + node.text_val);
             emit("    .text");
             emit("    pushq " + lbl + "(%rip)");
             last_expr_type = "precise";
        conclude
    conclude

    verify (k == NODE_IDENTIFIER) then
        datum name : series = node.text_val;
        datum off : whole = get_var_offset(name);
        last_expr_type = get_var_type(name);

        verify (off != 0) then
            // Local
            emit("    push " + to_string(off) + "(%rbp)");
        otherwise
            // Global?
            verify (is_global_var(name)) then
                 emit("    push " + name + "(%rip)");
            otherwise
                 emit("    # ERR: Unknown var " + name);
                 emit("    push $0");
            conclude
        conclude
    conclude

    verify (k == NODE_BINARY_OP) then
        gen_expression(node.left);
        gen_expression(node.right);

        emit("    pop %rbx"); // Right
        emit("    pop %rax"); // Left

        datum op : series = node.op;
        // Result type usually same as operands, assume whole/precise logic
        // Simplified: set type to "whole" or "state" (comparison)
        last_expr_type = "whole";

        verify (op == "+") then emit("    add %rbx, %rax"); emit("    push %rax"); conclude
        verify (op == "-") then emit("    sub %rbx, %rax"); emit("    push %rax"); conclude
        verify (op == "*") then emit("    imul %rbx, %rax"); emit("    push %rax"); conclude
        verify (op == "/") then
             emit("    cqo");
             emit("    idiv %rbx");
             emit("    push %rax");
        conclude
        verify (op == "%") then
             emit("    cqo");
             emit("    idiv %rbx");
             emit("    push %rdx");
        conclude

        verify (op == "==" or op == "!=" or op == "<" or op == ">" or op == "<=" or op == ">=") then
            emit("    cmp %rbx, %rax");
            datum set_instr : series = "sete";
            verify (op == "!=") then set_instr = "setne"; conclude
            verify (op == "<") then set_instr = "setl"; conclude
            verify (op == ">") then set_instr = "setg"; conclude
            verify (op == "<=") then set_instr = "setle"; conclude
            verify (op == ">=") then set_instr = "setge"; conclude

            emit("    " + set_instr + " %al");
            emit("    movzbq %al, %rax");
            emit("    push %rax");
            last_expr_type = "state";
        conclude

        verify (op == "and") then
            emit("    and %rbx, %rax");
            emit("    push %rax");
            last_expr_type = "state";
        conclude
        verify (op == "or") then
            emit("    or %rbx, %rax");
            emit("    push %rax");
            last_expr_type = "state";
        conclude
    conclude

    verify (k == NODE_FUNC_CALL) then
        datum fname : series = node.text_val;
        datum args : sequence<whole> = node.children;
        datum len : whole = length(args);

        // Intrinsic mapping
        verify (fname == "power") then fname = "_power"; conclude
        verify (fname == "to_string") then fname = "_to_string"; conclude
        verify (fname == "file_write") then fname = "_file_write"; conclude
        verify (fname == "file_read") then fname = "_file_read"; conclude
        verify (fname == "file_remove") then fname = "_file_remove"; conclude
        verify (fname == "file_exists") then fname = "_file_exists"; conclude
        verify (fname == "execute_system") then fname = "_execute_system"; conclude
        verify (fname == "absolute") then fname = "_absolute"; conclude
        verify (fname == "root") then fname = "_root"; conclude
        verify (fname == "sine") then fname = "_sine"; conclude
        verify (fname == "cosine") then fname = "_cosine"; conclude
        verify (fname == "tangent") then fname = "_tangent"; conclude
        verify (fname == "to_precise") then fname = "_to_precise"; conclude
        verify (fname == "to_integer") then fname = "_to_integer"; conclude
        verify (fname == "arg_count") then fname = "_arg_count"; conclude
        verify (fname == "arg_value") then fname = "_arg_value"; conclude
        verify (fname == "is_alnum") then fname = "isalnum@PLT"; conclude
        verify (fname == "is_alpha") then fname = "isalpha@PLT"; conclude
        verify (fname == "is_digit") then fname = "isdigit@PLT"; conclude
        verify (fname == "is_space") then fname = "isspace@PLT"; conclude
        verify (fname == "is_upper") then fname = "isupper@PLT"; conclude
        verify (fname == "is_lower") then fname = "islower@PLT"; conclude
        verify (fname == "length") then fname = "_length"; conclude
        verify (fname == "char_at") then fname = "_char_at"; conclude
        verify (fname == "substring") then fname = "_substring"; conclude
        verify (fname == "character") then fname = "_character"; conclude
        verify (fname == "to_upper") then fname = "_to_upper"; conclude
        verify (fname == "to_lower") then fname = "_to_lower"; conclude
        verify (fname == "reverse") then fname = "_reverse"; conclude
        verify (fname == "env_get") then fname = "_env_get"; conclude


        datum i : whole = 0;
        cycle (i < len) do
            gen_expression(args[i]);
            i = i + 1;
        conclude

        datum idx : whole = len - 1;
        cycle (len > 0) do
             verify (idx == 0) then emit("    pop %rdi"); conclude
             verify (idx == 1) then emit("    pop %rsi"); conclude
             verify (idx == 2) then emit("    pop %rdx"); conclude
             verify (idx == 3) then emit("    pop %rcx"); conclude
             verify (idx == 4) then emit("    pop %r8"); conclude
             verify (idx == 5) then emit("    pop %r9"); conclude

             verify (idx == 0) then len = 0; otherwise idx = idx - 1; conclude
        conclude

        emit("    call " + fname);
        emit("    push %rax");

        // Find return type
        datum j : whole = 0;
        datum flen : whole = length(defined_funcs);
        cycle (j < flen) do
            verify (defined_funcs[j].name == node.text_val) then
                last_expr_type = defined_funcs[j].return_type;
            conclude
            j = j + 1;
        conclude
        // If intrinsic or constructor, type might be inferred (e.g. struct name)
        // Assume struct constructors match their name
        last_expr_type = node.text_val; // For constructor 'Box' -> type 'Box'
    conclude

    verify (k == NODE_ARRAY_ACCESS) then
        // ID[idx]
        // Currently just emits ID[idx] (not supported by parser as we saw, but parser does ID[idx] as simple)
        // Wait, parser supports ID[idx].
        // This is `array_access`.
        // How to compile?
        // arr is pointer to vector.
        // vector: size, cap, data*.
        // Need to dereference.
        // For now, I haven't implemented vector fully.
        // Just return 0 to pass compilation? No.
        emit("    push $0");
    conclude

    verify (k == NODE_MEMBER_ACCESS) then
        // obj.member
        gen_expression(node.left);
        datum obj_type : series = last_expr_type;
        datum mem : series = node.text_val;
        datum off : whole = get_member_offset(obj_type, mem);

        emit("    pop %rax"); // obj ptr
        emit("    mov " + to_string(off) + "(%rax), %rbx"); // val
        emit("    push %rbx");

        // Update type? Need member type from StructInfo.
        // Skip for now.
    conclude
end

algorithm visit_stmt(node_idx : whole) -> void
begin
    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_VAR_DECL) then
        current_stack_size = current_stack_size + 8;
        datum offset : whole = 0 - current_stack_size;
        datum type_name : series = map_type_node_to_string(node.type_node);

        datum info : VarInfo = VarInfo(node.text_val, offset, false, type_name);
        append(current_locals, info);

        verify (node.initializer != -1) then
            gen_expression(node.initializer);
        otherwise
            emit("    push $0");
        conclude
    conclude

    verify (k == NODE_ASSIGN) then
        gen_expression(node.initializer);
        datum off : whole = get_var_offset(node.text_val);
        emit("    pop %rax");
        verify (off != 0) then
            emit("    mov %rax, " + to_string(off) + "(%rbp)");
        otherwise
             verify (is_global_var(node.text_val)) then
                 emit("    mov %rax, " + node.text_val + "(%rip)");
             conclude
        conclude
    conclude

    verify (k == NODE_MEMBER_ASSIGN) then
        // obj.mem = val
        gen_expression(node.left); // Pushes obj ptr
        datum obj_type : series = last_expr_type;
        gen_expression(node.initializer); // Pushes val

        datum mem : series = node.text_val;
        datum off : whole = get_member_offset(obj_type, mem);

        emit("    pop %rbx"); // val
        emit("    pop %rax"); // obj ptr
        emit("    mov %rbx, " + to_string(off) + "(%rax)");
    conclude

    verify (k == NODE_EXPR_STMT) then
         datum expr : whole = node.initializer;
         verify (node_heap[expr].kind == NODE_FUNC_CALL) then
             datum name : series = node_heap[expr].text_val;
             verify (name == "reveal") then
                 datum children : sequence<whole> = node_heap[expr].children;
                 datum c : whole = 0;
                 datum clen : whole = length(children);
                 cycle (c < clen) do
                     gen_expression(children[c]);
                     emit("    pop %rdi");
                     emit("    call _print_int");
                     c = c + 1;
                 conclude
                 emit("    call _print_newline");
                 return;
             conclude
         conclude

         gen_expression(node.initializer);
         emit("    pop %rax");
    conclude

    verify (k == NODE_BLOCK) then
        visit_block(node_idx);
    conclude

    verify (k == NODE_RETURN) then
        verify (node.initializer != -1) then
            gen_expression(node.initializer);
            emit("    pop %rax");
        conclude
        emit("    leave");
        emit("    ret");
    conclude

    verify (k == NODE_IF) then
        datum l_else : series = new_label();
        datum l_end : series = new_label();

        gen_expression(node.condition);
        emit("    pop %rax");
        emit("    cmp $0, %rax");
        emit("    je " + l_else);

        visit_block(node.then_branch);
        emit("    jmp " + l_end);

        emit(l_else + ":");
        verify (node.else_branch != -1) then
            visit_block(node.else_branch);
        conclude

        emit(l_end + ":");
    conclude

    verify (k == NODE_WHILE) then
        datum l_start : series = new_label();
        datum l_end : series = new_label();

        emit(l_start + ":");
        gen_expression(node.condition);
        emit("    pop %rax");
        emit("    cmp $0, %rax");
        emit("    je " + l_end);

        visit_block(node.then_branch);
        emit("    jmp " + l_start);
        emit(l_end + ":");
    conclude
end

algorithm visit_block(node_idx : whole) -> void
begin
    datum blk : ASTNode = node_heap[node_idx];
    datum i : whole = 0;
    datum children : sequence<whole> = blk.children;
    datum len : whole = length(children);
    cycle (i < len) do
        visit_stmt(children[i]);
        i = i + 1;
    conclude
end

algorithm generate_code(prog_idx : whole) -> void
begin
    collect_definitions(prog_idx);

    emit(".section .text");
    emit(".global _start");
    emit(".global main");
    emit(".global _print_int");
    emit(".global _print_newline");
    emit(".global _power");
    emit(".global _to_string");
    emit(".global _file_write");
    emit(".global _file_read");
    emit(".global _file_remove");
    emit(".global _file_exists");
    emit(".global _execute_system");
    emit(".global _absolute");
    emit(".global _root");
    emit(".global _sine");
    emit(".global _cosine");
    emit(".global _tangent");
    emit(".global _to_precise");
    emit(".global _to_integer");
    emit(".global _arg_count");
    emit(".global _arg_value");
    emit(".global _length");
    emit(".global _char_at");
    emit(".global _substring");
    emit(".global _character");
    emit(".global _to_upper");
    emit(".global _to_lower");
    emit(".global _reverse");
    emit(".global _env_get");

    emit("_start:");
    emit("    call main");
    emit("    mov %rax, %rdi");
    emit("    mov $60, %rax");
    emit("    syscall");

    emit("_print_int:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    mov %rdi, %rsi");
    emit("    leaq .Lfmt_int(%rip), %rdi");
    emit("    xor %rax, %rax");
    emit("    call printf@PLT");
    emit("    leave");
    emit("    ret");

    emit("_print_newline:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    mov $10, %rdi");
    emit("    call putchar@PLT");
    emit("    leave");
    emit("    ret");

    // Power: pow(double, double)
    emit("_power:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    cvtsi2sd %rdi, %xmm0"); // Base (int to double)
    emit("    cvtsi2sd %rsi, %xmm1"); // Exp (int to double)
    emit("    call pow@PLT");
    emit("    cvttsd2si %xmm0, %rax"); // Result to int
    emit("    leave");
    emit("    ret");

    // to_string(int) -> string*
    emit("_to_string:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    sub $32, %rsp"); // Buffer space
    emit("    mov %rdi, %rdx"); // Value
    emit("    leaq .Lfmt_int(%rip), %rsi"); // Fmt
    emit("    leaq -32(%rbp), %rdi"); // Buffer
    emit("    xor %rax, %rax");
    emit("    call sprintf@PLT");
    emit("    mov $32, %rdi");
    emit("    call malloc@PLT");
    emit("    mov %rax, %rbx"); // Dest
    emit("    leaq -32(%rbp), %rsi"); // Src
    emit("    mov %rbx, %rdi");
    emit("    call strcpy@PLT");
    emit("    mov %rbx, %rax");
    emit("    leave");
    emit("    ret");

    // file_write(path, content)
    emit("_file_write:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    push %rsi"); // Content
    emit("    push %rdi"); // Path
    emit("    leaq .Lmode_w(%rip), %rsi");
    emit("    call fopen@PLT");
    emit("    test %rax, %rax");
    emit("    jz .Lfw_err");
    emit("    mov %rax, %rdi"); // File*
    emit("    pop %rax"); // Path (discard)
    emit("    pop %rsi"); // Content
    emit("    push %rdi"); // Save File*
    emit("    call fputs@PLT");
    emit("    pop %rdi"); // File*
    emit("    call fclose@PLT");
    emit(".Lfw_err:");
    emit("    leave");
    emit("    ret");

    // file_read(path) -> string
    emit("_file_read:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    leaq .Lmode_r(%rip), %rsi");
    emit("    call fopen@PLT");
    emit("    test %rax, %rax");
    emit("    jz .Lfr_err");
    emit("    mov %rax, %rbx"); // File*
    emit("    # Seek end");
    emit("    mov %rbx, %rdi");
    emit("    mov $0, %rsi");
    emit("    mov $2, %rdx"); // SEEK_END
    emit("    call fseek@PLT");
    emit("    # ftell");
    emit("    mov %rbx, %rdi");
    emit("    call ftell@PLT");
    emit("    mov %rax, %r12"); // Size
    emit("    # Seek set");
    emit("    mov %rbx, %rdi");
    emit("    mov $0, %rsi");
    emit("    mov $0, %rdx"); // SEEK_SET
    emit("    call fseek@PLT");
    emit("    # Malloc size+1");
    emit("    mov %r12, %rdi");
    emit("    add $1, %rdi");
    emit("    call malloc@PLT");
    emit("    mov %rax, %r13"); // Buffer
    emit("    # Read");
    emit("    mov %r13, %rdi");
    emit("    mov $1, %rsi");
    emit("    mov %r12, %rdx");
    emit("    mov %rbx, %rcx");
    emit("    call fread@PLT");
    emit("    # Null terminate");
    emit("    movb $0, (%r13, %r12)");
    emit("    # Close");
    emit("    mov %rbx, %rdi");
    emit("    call fclose@PLT");
    emit("    mov %r13, %rax");
    emit("    jmp .Lfr_ret");
    emit(".Lfr_err:");
    emit("    xor %rax, %rax"); // Return null?
    emit(".Lfr_ret:");
    emit("    leave");
    emit("    ret");

    // file_remove(path)
    emit("_file_remove:");
    emit("    jmp unlink@PLT");

    // file_exists(path) -> bool
    emit("_file_exists:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    mov $0, %rsi"); // F_OK
    emit("    call access@PLT");
    emit("    cmp $0, %rax");
    emit("    sete %al");
    emit("    movzbq %al, %rax");
    emit("    leave");
    emit("    ret");

    // execute_system(cmd) -> int
    emit("_execute_system:");
    emit("    jmp system@PLT");

    // absolute(int) -> int
    emit("_absolute:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    mov %rdi, %rax");
    emit("    cqo");
    emit("    xor %rdx, %rax");
    emit("    sub %rdx, %rax");
    emit("    leave");
    emit("    ret");

    // root(double) -> double
    emit("_root:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    movq %rdi, %xmm0");
    emit("    call sqrt@PLT");
    emit("    movq %xmm0, %rax");
    emit("    leave");
    emit("    ret");

    // sine(double) -> double
    emit("_sine:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    movq %rdi, %xmm0");
    emit("    call sin@PLT");
    emit("    movq %xmm0, %rax");
    emit("    leave");
    emit("    ret");

    // cosine(double) -> double
    emit("_cosine:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    movq %rdi, %xmm0");
    emit("    call cos@PLT");
    emit("    movq %xmm0, %rax");
    emit("    leave");
    emit("    ret");

    // tangent(double) -> double
    emit("_tangent:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    movq %rdi, %xmm0");
    emit("    call tan@PLT");
    emit("    movq %xmm0, %rax");
    emit("    leave");
    emit("    ret");

    // to_precise(string) -> double
    emit("_to_precise:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    mov $0, %rsi");
    emit("    call strtod@PLT");
    emit("    movq %xmm0, %rax");
    emit("    leave");
    emit("    ret");

    // to_integer(string) -> int
    emit("_to_integer:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    mov $10, %rdx");
    emit("    mov $0, %rsi");
    emit("    call strtoll@PLT");
    emit("    leave");
    emit("    ret");

    // arg_count() -> int
    emit("_arg_count:");
    emit("    mov _global_argc(%rip), %rax");
    emit("    ret");

    // arg_value(i) -> string
    emit("_arg_value:");
    emit("    mov _global_argv(%rip), %rax");
    emit("    mov (%rax, %rdi, 8), %rax");
    emit("    ret");

    // length(string) -> int
    emit("_length:");
    emit("    jmp strlen@PLT");

    // char_at(string, index) -> int (code)
    emit("_char_at:");
    emit("    movb (%rdi, %rsi), %al");
    emit("    movzbq %al, %rax");
    emit("    ret");

    // character(code) -> string
    emit("_character:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    push %rdi"); // Save code
    emit("    mov $2, %rdi");
    emit("    call malloc@PLT");
    emit("    pop %rdi"); // Restore code
    emit("    movb %dil, (%rax)");
    emit("    movb $0, 1(%rax)");
    emit("    leave");
    emit("    ret");

    // substring(str, start, len) -> string
    emit("_substring:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    push %rdx"); // len
    emit("    push %rsi"); // start
    emit("    push %rdi"); // str
    emit("    mov %rdx, %rdi");
    emit("    add $1, %rdi");
    emit("    call malloc@PLT");
    emit("    mov %rax, %r12"); // Dest
    emit("    pop %rdi"); // str
    emit("    pop %rsi"); // start
    emit("    pop %rdx"); // len
    emit("    add %rsi, %rdi"); // str + start
    emit("    mov %r12, %rax"); // dest
    emit("    # Loop copy");
    emit("    test %rdx, %rdx");
    emit("    jz .Lsub_end");
    emit(".Lsub_loop:");
    emit("    movb (%rdi), %cl");
    emit("    movb %cl, (%rax)");
    emit("    inc %rdi");
    emit("    inc %rax");
    emit("    dec %rdx");
    emit("    jnz .Lsub_loop");
    emit(".Lsub_end:");
    emit("    movb $0, (%rax)");
    emit("    mov %r12, %rax");
    emit("    leave");
    emit("    ret");

    // to_upper(str) -> str
    emit("_to_upper:");
    // In-place modification not safe if literal?
    // Alloc copy first?
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    push %rdi");
    emit("    call strdup@PLT"); // Requires _BSD_SOURCE or similar, or implement copy
    emit("    mov %rax, %r12"); // Copy
    emit("    mov %r12, %rbx"); // Iter
    emit(".Lup_loop:");
    emit("    movb (%rbx), %dil");
    emit("    test %dil, %dil");
    emit("    jz .Lup_end");
    emit("    movzbq %dil, %rdi");
    emit("    call toupper@PLT");
    emit("    movb %al, (%rbx)");
    emit("    inc %rbx");
    emit("    jmp .Lup_loop");
    emit(".Lup_end:");
    emit("    mov %r12, %rax");
    emit("    leave");
    emit("    ret");

    // to_lower(str) -> str
    emit("_to_lower:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    push %rdi");
    emit("    call strdup@PLT");
    emit("    mov %rax, %r12");
    emit("    mov %r12, %rbx");
    emit(".Llow_loop:");
    emit("    movb (%rbx), %dil");
    emit("    test %dil, %dil");
    emit("    jz .Llow_end");
    emit("    movzbq %dil, %rdi");
    emit("    call tolower@PLT");
    emit("    movb %al, (%rbx)");
    emit("    inc %rbx");
    emit("    jmp .Llow_loop");
    emit(".Llow_end:");
    emit("    mov %r12, %rax");
    emit("    leave");
    emit("    ret");

    // reverse(str) -> str
    emit("_reverse:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    push %rdi");
    emit("    call strdup@PLT");
    emit("    mov %rax, %r12");
    emit("    mov %r12, %rdi");
    emit("    call strlen@PLT");
    emit("    mov %rax, %rcx"); // len
    emit("    test %rcx, %rcx");
    emit("    jz .Lrev_end");
    emit("    mov %r12, %rsi"); // Start
    emit("    mov %r12, %rdi");
    emit("    add %rcx, %rdi");
    emit("    dec %rdi"); // End
    emit(".Lrev_loop:");
    emit("    cmp %rsi, %rdi");
    emit("    jle .Lrev_end");
    emit("    movb (%rsi), %al");
    emit("    movb (%rdi), %bl");
    emit("    movb %bl, (%rsi)");
    emit("    movb %al, (%rdi)");
    emit("    inc %rsi");
    emit("    dec %rdi");
    emit("    jmp .Lrev_loop");
    emit(".Lrev_end:");
    emit("    mov %r12, %rax");
    emit("    leave");
    emit("    ret");

    // env_get(name) -> string
    emit("_env_get:");
    emit("    push %rbp");
    emit("    mov %rsp, %rbp");
    emit("    call getenv@PLT");
    emit("    test %rax, %rax");
    emit("    jz .Lenv_null");
    emit("    mov %rax, %rdi");
    emit("    call strdup@PLT");
    emit("    jmp .Lenv_ret");
    emit(".Lenv_null:");
    emit("    mov $1, %rdi");
    emit("    call malloc@PLT");
    emit("    movb $0, (%rax)");
    emit(".Lenv_ret:");
    emit("    leave");
    emit("    ret");

    emit(".section .rodata");
    emit(".Lfmt_int: .string " + character(34) + "%lld " + character(34));
    emit(".Lmode_w: .string " + character(34) + "w" + character(34));
    emit(".Lmode_r: .string " + character(34) + "r" + character(34));

    // Globals for CLI
    emit(".section .bss");
    emit(".align 8");
    emit("_global_argc: .quad 0");
    emit("_global_argv: .quad 0");

    // Emit user globals
    datum gidx : whole = 0;
    datum glen : whole = length(global_vars);
    cycle (gidx < glen) do
        emit(".align 8");
        emit(global_vars[gidx].name + ": .quad 0");
        gidx = gidx + 1;
    conclude

    emit(".section .text");

    datum prog : ASTNode = node_heap[prog_idx];
    datum i : whole = 0;
    datum children : sequence<whole> = prog.children; // EXTRACT
    datum len : whole = length(children);

    // Emit struct constructors
    datum sidx : whole = 0;
    datum slen : whole = length(defined_structs);
    cycle (sidx < slen) do
        datum sinfo : StructInfo = defined_structs[sidx];
        emit(sinfo.name + ":");
        emit("    push %rbp");
        emit("    mov %rsp, %rbp");

        // Allocate
        datum size : whole = length(sinfo.members) * 8;
        emit("    mov $" + to_string(size) + ", %rdi");
        emit("    call malloc@PLT");
        emit("    mov %rax, %r12"); // Ptr (save in callee-saved)

        // Emit pushes
        datum midx : whole = 0;
        datum mcount : whole = length(sinfo.members);
        cycle (midx < mcount) do
             verify (midx == 0) then emit("    push %rdi"); conclude
             verify (midx == 1) then emit("    push %rsi"); conclude
             verify (midx == 2) then emit("    push %rdx"); conclude
             verify (midx == 3) then emit("    push %rcx"); conclude
             verify (midx == 4) then emit("    push %r8"); conclude
             verify (midx == 5) then emit("    push %r9"); conclude
             midx = midx + 1;
        conclude

        // Pop into fields (reverse order of push)
        midx = mcount - 1;
        cycle (mcount > 0) do
             emit("    pop %rax");
             emit("    mov %rax, " + to_string(midx * 8) + "(%r12)");

             verify (midx == 0) then mcount = 0; otherwise midx = midx - 1; conclude
        conclude

        emit("    mov %r12, %rax");
        emit("    leave");
        emit("    ret");

        sidx = sidx + 1;
    conclude


    cycle (i < len) do
        datum child_idx : whole = children[i];
        datum k : whole = node_heap[child_idx].kind;

        verify (k == NODE_FUNC_DECL) then
            datum fname : series = node_heap[child_idx].text_val;

            verify (fname == "genesis") then
                fname = "main";
                emit(fname + ":");
                emit("    push %rbp");
                emit("    mov %rsp, %rbp");
                emit("    mov %rdi, _global_argc(%rip)");
                emit("    mov %rsi, _global_argv(%rip)");

                datum empty_locals : sequence<VarInfo>;
                current_locals = empty_locals;
                current_stack_size = 0;

                visit_block(node_heap[child_idx].then_branch);

                emit("    leave");
                emit("    ret");
            otherwise
                emit(fname + ":");
                emit("    push %rbp");
                emit("    mov %rsp, %rbp");

                datum empty_locals : sequence<VarInfo>;
                current_locals = empty_locals;
                current_stack_size = 0;

                visit_block(node_heap[child_idx].then_branch);

                emit("    leave");
                emit("    ret");
            conclude
        conclude

        i = i + 1;
    conclude
end
