// llvm_algebra.inc - LLVM IR Generation

// Forward declarations
algorithm to_string_hex_byte(c : whole) -> series;

datum llvm_globals : series = "";
datum llvm_code : series = "";
datum codegen_output : series = "";

datum llvm_reg_counter : whole = 0;
datum llvm_lbl_counter : whole = 0;
datum llvm_str_counter : whole = 0;

// Internal Helpers
algorithm llvm_emit(s : series) -> void
begin
    llvm_code = llvm_code + s + character(10);
end

algorithm llvm_emit_global(s : series) -> void
begin
    llvm_globals = llvm_globals + s + character(10);
end

algorithm llvm_next_reg() -> series
begin
    llvm_reg_counter = llvm_reg_counter + 1;
    result "%r" + to_string(llvm_reg_counter);
end

algorithm llvm_next_label() -> series
begin
    llvm_lbl_counter = llvm_lbl_counter + 1;
    result "L" + to_string(llvm_lbl_counter);
end

algorithm llvm_reset_local() -> void
begin
    llvm_reg_counter = 0;
end

// Types
algorithm llvm_type(t : series) -> series
begin
    verify (t == "whole" or t == "i64") then result "i64"; conclude
    verify (t == "precise" or t == "double") then result "double"; conclude
    verify (t == "state" or t == "i1") then result "i1"; conclude
    verify (t == "series" or t == "i8*") then result "i8*"; conclude
    verify (t == "void") then result "void"; conclude

    datum len : whole = length(t);
    verify (len >= 8) then
        verify (substring(t, 0, 8) == "sequence") then result "%struct.sequence*"; conclude
    conclude

    // Struct pointer
    result "%struct." + t + "*";
end

// Operations
algorithm llvm_add(lhs : series, rhs : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    verify (type == "precise" or type == "double") then
        llvm_emit("  " + r + " = fadd double " + lhs + ", " + rhs);
    otherwise
        llvm_emit("  " + r + " = add " + llvm_type(type) + " " + lhs + ", " + rhs);
    conclude
    result r;
end

algorithm llvm_sub(lhs : series, rhs : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    verify (type == "precise" or type == "double") then
        llvm_emit("  " + r + " = fsub double " + lhs + ", " + rhs);
    otherwise
        llvm_emit("  " + r + " = sub " + llvm_type(type) + " " + lhs + ", " + rhs);
    conclude
    result r;
end

algorithm llvm_mul(lhs : series, rhs : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    verify (type == "precise" or type == "double") then
        llvm_emit("  " + r + " = fmul double " + lhs + ", " + rhs);
    otherwise
        llvm_emit("  " + r + " = mul " + llvm_type(type) + " " + lhs + ", " + rhs);
    conclude
    result r;
end

algorithm llvm_div(lhs : series, rhs : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    verify (type == "precise" or type == "double") then
        llvm_emit("  " + r + " = fdiv double " + lhs + ", " + rhs);
    otherwise
        llvm_emit("  " + r + " = sdiv " + llvm_type(type) + " " + lhs + ", " + rhs);
    conclude
    result r;
end

algorithm llvm_mod(lhs : series, rhs : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = srem " + llvm_type(type) + " " + lhs + ", " + rhs);
    result r;
end

algorithm llvm_cmp(op : series, lhs : series, rhs : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    datum code : series = "";

    verify (type == "precise" or type == "double") then
        // Ordered floating point comparison
        verify (op == "==") then code = "oeq"; conclude
        verify (op == "!=") then code = "une"; conclude
        verify (op == "<") then code = "olt"; conclude
        verify (op == "<=") then code = "ole"; conclude
        verify (op == ">") then code = "ogt"; conclude
        verify (op == ">=") then code = "oge"; conclude
        llvm_emit("  " + r + " = fcmp " + code + " double " + lhs + ", " + rhs);
    otherwise
        verify (op == "==") then code = "eq"; conclude
        verify (op == "!=") then code = "ne"; conclude
        verify (op == "<") then code = "slt"; conclude
        verify (op == "<=") then code = "sle"; conclude
        verify (op == ">") then code = "sgt"; conclude
        verify (op == ">=") then code = "sge"; conclude
        llvm_emit("  " + r + " = icmp " + code + " " + llvm_type(type) + " " + lhs + ", " + rhs);
    conclude
    result r;
end

algorithm llvm_and(lhs : series, rhs : series) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = and i1 " + lhs + ", " + rhs);
    result r;
end

algorithm llvm_or(lhs : series, rhs : series) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = or i1 " + lhs + ", " + rhs);
    result r;
end

algorithm llvm_call(fname : series, args_code : series, ret_type : series) -> series
begin
    datum r : series = "";
    verify (ret_type != "void") then
        r = llvm_next_reg();
        llvm_emit("  " + r + " = call " + llvm_type(ret_type) + " @" + fname + "(" + args_code + ")");
    otherwise
        llvm_emit("  call void @" + fname + "(" + args_code + ")");
    conclude
    result r;
end

algorithm llvm_alloca(type : series) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = alloca " + llvm_type(type));
    result r;
end

algorithm llvm_load(ptr : series, type : series) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = load " + llvm_type(type) + ", " + llvm_type(type) + "* " + ptr);
    result r;
end

algorithm llvm_store(val : series, ptr : series, type : series) -> void
begin
    llvm_emit("  store " + llvm_type(type) + " " + val + ", " + llvm_type(type) + "* " + ptr);
end

algorithm llvm_phi(type : series, pairs : series) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = phi " + llvm_type(type) + " " + pairs);
    result r;
end

algorithm llvm_br(target : series) -> void
begin
    llvm_emit("  br label %" + target);
end

algorithm llvm_cond_br(cond : series, true_lbl : series, false_lbl : series) -> void
begin
    llvm_emit("  br i1 " + cond + ", label %" + true_lbl + ", label %" + false_lbl);
end

algorithm llvm_label(l : series) -> void
begin
    llvm_emit(l + ":");
end

algorithm to_string_hex_byte(c : whole) -> series
begin
    // Convert byte to 2 char hex
    datum hex : series = "0123456789ABCDEF";
    datum h1 : whole = (c / 16) % 16;
    datum h2 : whole = c % 16;
    result character(char_at(hex, h1)) + character(char_at(hex, h2));
end

algorithm llvm_define_global_string(val : series) -> series
begin
    llvm_str_counter = llvm_str_counter + 1;
    datum gname : series = "@.str." + to_string(llvm_str_counter);
    datum len : whole = length(val) + 1; // +1 for null terminator

    datum esc : series = "";
    datum i : whole = 0;
    cycle (i < length(val)) do
        datum c : whole = char_at(val, i);
        datum h : series = to_string_hex_byte(c);
        esc = esc + "\\" + h;
        i = i + 1;
    conclude
    esc = esc + "\\00";

    llvm_emit_global(gname + " = private unnamed_addr constant [" + to_string(len) + " x i8] c\"" + esc + "\", align 1");

    result gname;
end

algorithm llvm_gep_string(gname : series, len : whole) -> series
begin
    datum r : series = llvm_next_reg();
    llvm_emit("  " + r + " = getelementptr inbounds [" + to_string(len) + " x i8], [" + to_string(len) + " x i8]* " + gname + ", i64 0, i64 0");
    result r;
end

algorithm llvm_cast(val : series, from_type : series, to_type : series) -> series
begin
    verify (from_type == to_type) then result val; conclude
    verify (llvm_type(from_type) == llvm_type(to_type)) then result val; conclude

    datum r : series = llvm_next_reg();

    verify ((from_type == "whole" or from_type == "i64") and (to_type == "precise" or to_type == "double")) then
        llvm_emit("  " + r + " = sitofp i64 " + val + " to double");
        result r;
    conclude
    verify ((from_type == "precise" or from_type == "double") and (to_type == "whole" or to_type == "i64")) then
        llvm_emit("  " + r + " = fptosi double " + val + " to i64");
        result r;
    conclude

    // Bool conversions
    verify ((from_type == "state" or from_type == "i1") and (to_type == "whole" or to_type == "i64")) then
        llvm_emit("  " + r + " = zext i1 " + val + " to i64");
        result r;
    conclude

    datum lt_from : series = llvm_type(from_type);
    datum lt_to : series = llvm_type(to_type);

    // Ptr to Int
    verify ((lt_from == "i8*" or substring(lt_from, 0, 1) == "%") and lt_to == "i64") then
         llvm_emit("  " + r + " = ptrtoint " + lt_from + " " + val + " to i64");
         result r;
    conclude

    // Int to Ptr
    verify (lt_from == "i64" and (lt_to == "i8*" or substring(lt_to, 0, 1) == "%")) then
         llvm_emit("  " + r + " = inttoptr i64 " + val + " to " + lt_to);
         result r;
    conclude

    // Ptr bitcasts
    llvm_emit("  " + r + " = bitcast " + lt_from + " " + val + " to " + lt_to);
    result r;
end
