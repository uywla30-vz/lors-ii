// calculer.inc - Machine Abstraction Layer (x86-64 GAS)

// Global Output Buffer
datum codegen_output : series = "";

algorithm op_emit(instr : series) -> void
begin
    codegen_output = codegen_output + instr + character(10);
end

algorithm op_label(lbl : series) -> void
begin
    op_emit(lbl + ":");
end

// Entry Point
algorithm op_start() -> void
begin
    op_emit(".section .text");
    op_emit(".global _start");
    op_emit("_start:");

    // Setup argc (rdi) and argv (rsi) from stack
    // Stack at entry: [argc, argv0, argv1, ..., NULL]
    // rsp points to argc
    op_emit("    mov (%rsp), %rdi");      // argc -> rdi
    op_emit("    lea 8(%rsp), %rsi");     // argv ptr -> rsi

    op_emit("    call main");

    // Exit using libc exit() to ensure buffers (stdout) are flushed
    op_emit("    mov %rax, %rdi");
    op_emit("    call exit@PLT");
end

// Function Prologue
algorithm op_prologue(name : series) -> void
begin
    op_emit(".global " + name);
    op_emit(name + ":");
    op_emit("    push %rbp");
    op_emit("    mov %rsp, %rbp");
end

// Function Epilogue (Void)
algorithm op_epilogue() -> void
begin
    op_emit("    leave");
    op_emit("    ret");
end

// Stack Operations
algorithm op_push_imm(val : whole) -> void
begin
    op_emit("    push $" + to_string(val));
end

algorithm op_push_imm_str(lbl : series) -> void
begin
    // Load address of string label
    op_emit("    leaq " + lbl + "(%rip), %rax");
    op_emit("    push %rax");
end

algorithm op_push_imm_float(lbl : series) -> void
begin
    op_emit("    pushq " + lbl + "(%rip)");
end

algorithm op_push_local(offset : whole) -> void
begin
    op_emit("    push " + to_string(offset) + "(%rbp)");
end

algorithm op_push_global(name : series) -> void
begin
    op_emit("    push " + name + "(%rip)");
end

algorithm op_pop_to_local(offset : whole) -> void
begin
    op_emit("    pop %rax");
    op_emit("    mov %rax, " + to_string(offset) + "(%rbp)");
end

algorithm op_pop_to_global(name : series) -> void
begin
    op_emit("    pop %rax");
    op_emit("    mov %rax, " + name + "(%rip)");
end

algorithm op_pop_discard() -> void
begin
    op_emit("    add $8, %rsp");
end

// Arithmetic (Stack: ..., A, B -> Result)
// A op B
algorithm op_add() -> void
begin
    op_emit("    pop %rbx"); // B
    op_emit("    pop %rax"); // A
    op_emit("    add %rbx, %rax");
    op_emit("    push %rax");
end

algorithm op_sub() -> void
begin
    op_emit("    pop %rbx"); // B
    op_emit("    pop %rax"); // A
    op_emit("    sub %rbx, %rax");
    op_emit("    push %rax");
end

algorithm op_mul() -> void
begin
    op_emit("    pop %rbx");
    op_emit("    pop %rax");
    op_emit("    imul %rbx, %rax");
    op_emit("    push %rax");
end

algorithm op_div() -> void
begin
    op_emit("    pop %rbx");
    op_emit("    pop %rax");
    op_emit("    cqo"); // Sign extend rax to rdx:rax
    op_emit("    idiv %rbx");
    op_emit("    push %rax");
end

algorithm op_mod() -> void
begin
    op_emit("    pop %rbx");
    op_emit("    pop %rax");
    op_emit("    cqo");
    op_emit("    idiv %rbx");
    op_emit("    push %rdx");
end

// Comparisons
algorithm op_cmp(op_sym : series) -> void
begin
    op_emit("    pop %rbx"); // Right
    op_emit("    pop %rax"); // Left
    op_emit("    cmp %rbx, %rax");

    datum set_instr : series = "sete";
    verify (op_sym == "!=") then set_instr = "setne"; conclude
    verify (op_sym == "<") then set_instr = "setl"; conclude
    verify (op_sym == ">") then set_instr = "setg"; conclude
    verify (op_sym == "<=") then set_instr = "setle"; conclude
    verify (op_sym == ">=") then set_instr = "setge"; conclude

    op_emit("    " + set_instr + " %al");
    op_emit("    movzbq %al, %rax");
    op_emit("    push %rax");
end

algorithm op_and() -> void
begin
    op_emit("    pop %rbx");
    op_emit("    pop %rax");
    op_emit("    and %rbx, %rax");
    op_emit("    push %rax");
end

algorithm op_or() -> void
begin
    op_emit("    pop %rbx");
    op_emit("    pop %rax");
    op_emit("    or %rbx, %rax");
    op_emit("    push %rax");
end

// Control Flow
algorithm op_jmp(lbl : series) -> void
begin
    op_emit("    jmp " + lbl);
end

algorithm op_jmp_false(lbl : series) -> void
begin
    op_emit("    pop %rax");
    op_emit("    cmp $0, %rax");
    op_emit("    je " + lbl);
end

// Function Calls
// Safe Call Wrapper with Stack Alignment
algorithm op_call(name : series, num_args : whole) -> void
begin
    // 1. Pop args into registers (System V ABI)
    datum i : whole = num_args - 1;
    cycle (num_args > 0) do
        verify (i == 0) then op_emit("    pop %rdi"); conclude
        verify (i == 1) then op_emit("    pop %rsi"); conclude
        verify (i == 2) then op_emit("    pop %rdx"); conclude
        verify (i == 3) then op_emit("    pop %rcx"); conclude
        verify (i == 4) then op_emit("    pop %r8"); conclude
        verify (i == 5) then op_emit("    pop %r9"); conclude

        verify (i == 0) then num_args = 0; otherwise i = i - 1; conclude
    conclude

    // 2. Safe Call Sequence
    // Preserve RBP to restore stack frame after alignment
    op_emit("    push %rbp");
    op_emit("    mov %rsp, %rbp");

    // Align stack
    op_emit("    and $-16, %rsp");

    // Call
    op_emit("    call " + name);

    // Restore stack
    op_emit("    mov %rbp, %rsp");
    op_emit("    pop %rbp");
end

algorithm op_push_result() -> void
begin
    op_emit("    push %rax");
end

// Data Definitions
algorithm op_section_rodata() -> void
begin
    op_emit(".section .rodata");
end

algorithm op_section_data() -> void
begin
    op_emit(".section .data");
end

algorithm op_section_bss() -> void
begin
    op_emit(".section .bss");
end

algorithm op_section_text() -> void
begin
    op_emit(".section .text");
end

algorithm op_define_string(lbl : series, val : series) -> void
begin
    op_emit(lbl + ": .string " + character(34) + val + character(34));
end

algorithm op_define_global(name : series) -> void
begin
    op_emit(".align 8");
    op_emit(name + ": .quad 0");
end

algorithm op_define_double(lbl : series, val : series) -> void
begin
    op_emit(lbl + ": .double " + val);
end
