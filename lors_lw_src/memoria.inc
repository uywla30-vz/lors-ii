// memoria.inc - Data Management Layer

// Structures
structure VarInfo
begin
    datum name : series;
    datum type_name : series;
    datum is_global : state;
    datum ssa_name : series; // Current SSA register holding the value
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

// Global State
datum mem_local_vars : sequence<VarInfo>;
datum mem_global_vars : sequence<VarInfo>;
datum mem_defined_structs : sequence<StructInfo>;
datum mem_defined_funcs : sequence<FuncInfo>;

datum mem_label_counter : whole = 0;

// Label Generation
algorithm mem_new_label() -> series
begin
    datum l : series = "L" + to_string(mem_label_counter);
    mem_label_counter = mem_label_counter + 1;
    result l;
end

// Scope Management
algorithm mem_enter_function() -> void
begin
    // Clear locals
    datum empty : sequence<VarInfo>;
    mem_local_vars = empty;
end

algorithm mem_copy_locals() -> sequence<VarInfo>
begin
    datum copy : sequence<VarInfo>;
    datum i : whole = 0;
    cycle (i < length(mem_local_vars)) do
        datum v : VarInfo = mem_local_vars[i];
        append(copy, v);
        i = i + 1;
    conclude
    result copy;
end

algorithm mem_set_locals(vars : sequence<VarInfo>) -> void
begin
    mem_local_vars = vars;
end

algorithm mem_get_ssa_from_snapshot(snap : sequence<VarInfo>, name : series) -> series
begin
    datum i : whole = 0;
    cycle (i < length(snap)) do
        verify (snap[i].name == name) then result snap[i].ssa_name; conclude
        i = i + 1;
    conclude
    result "";
end

algorithm mem_declare_local(name : series, type_name : series, reg : series) -> void
begin
    datum v : VarInfo;
    v.name = name;
    v.type_name = type_name;
    v.is_global = false;
    v.ssa_name = reg;

    append(mem_local_vars, v);
end

algorithm mem_declare_global(name : series, type_name : series) -> void
begin
    datum v : VarInfo;
    v.name = name;
    v.type_name = type_name;
    v.is_global = true;
    v.ssa_name = "@" + name; // Global variable pointer

    append(mem_global_vars, v);
end

algorithm mem_register_struct(info : StructInfo) -> void
begin
    append(mem_defined_structs, info);
end

algorithm mem_register_func(info : FuncInfo) -> void
begin
    append(mem_defined_funcs, info);
end

// Lookups
algorithm mem_get_var_ssa(name : series) -> series
begin
    // Search locals (reverse order to find most recent shadowing?)
    datum i : whole = length(mem_local_vars) - 1;
    datum len : whole = length(mem_local_vars);
    cycle (len > 0) do
        verify (mem_local_vars[i].name == name) then
            result mem_local_vars[i].ssa_name;
        conclude
        verify (i == 0) then len = 0; otherwise i = i - 1; conclude
    conclude

    // Globals
    i = 0;
    len = length(mem_global_vars);
    cycle (i < len) do
        verify (mem_global_vars[i].name == name) then
            result mem_global_vars[i].ssa_name;
        conclude
        i = i + 1;
    conclude

    result ""; // Not found
end

algorithm mem_update_var_ssa(name : series, new_reg : series) -> void
begin
    // Update local
    datum i : whole = 0;
    datum len : whole = length(mem_local_vars);
    cycle (i < len) do
        verify (mem_local_vars[i].name == name) then
            mem_local_vars[i].ssa_name = new_reg;
            return;
        conclude
        i = i + 1;
    conclude
end

algorithm mem_is_global(name : series) -> state
begin
    datum i : whole = 0;
    datum len : whole = length(mem_global_vars);
    cycle (i < len) do
        verify (mem_global_vars[i].name == name) then
            result true;
        conclude
        i = i + 1;
    conclude
    result false;
end

algorithm mem_get_var_type(name : series) -> series
begin
    datum i : whole = 0;
    datum len : whole = length(mem_local_vars);
    cycle (i < len) do
        verify (mem_local_vars[i].name == name) then
            result mem_local_vars[i].type_name;
        conclude
        i = i + 1;
    conclude

    i = 0;
    len = length(mem_global_vars);
    cycle (i < len) do
        verify (mem_global_vars[i].name == name) then
            result mem_global_vars[i].type_name;
        conclude
        i = i + 1;
    conclude

    result "unknown";
end

algorithm mem_get_func_return_type(name : series) -> series
begin
    // Intrinsics
    verify (name == "arg_count") then result "whole"; conclude
    verify (name == "length") then result "whole"; conclude
    verify (name == "char_at") then result "whole"; conclude
    verify (name == "to_integer") then result "whole"; conclude
    verify (name == "to_precise") then result "precise"; conclude
    verify (name == "power") then result "precise"; conclude
    verify (name == "root") then result "precise"; conclude
    verify (name == "sine") then result "precise"; conclude
    verify (name == "cosine") then result "precise"; conclude
    verify (name == "tangent") then result "precise"; conclude
    verify (name == "absolute") then result "whole"; conclude
    verify (name == "substring") then result "series"; conclude
    verify (name == "to_string") then result "series"; conclude
    verify (name == "character") then result "series"; conclude
    verify (name == "to_upper") then result "series"; conclude
    verify (name == "to_lower") then result "series"; conclude
    verify (name == "reverse") then result "series"; conclude
    verify (name == "arg_value") then result "series"; conclude
    verify (name == "env_get") then result "series"; conclude
    verify (name == "file_read") then result "series"; conclude
    verify (name == "file_write") then result "void"; conclude
    verify (name == "file_exists") then result "state"; conclude
    verify (name == "file_remove") then result "void"; conclude
    verify (name == "is_digit") then result "state"; conclude
    verify (name == "is_alpha") then result "state"; conclude
    verify (name == "is_alnum") then result "state"; conclude
    verify (name == "is_space") then result "state"; conclude
    verify (name == "is_upper") then result "state"; conclude
    verify (name == "is_lower") then result "state"; conclude
    verify (name == "execute_system") then result "whole"; conclude

    // Defined functions
    datum i : whole = 0;
    datum len : whole = length(mem_defined_funcs);
    cycle (i < len) do
        verify (mem_defined_funcs[i].name == name) then
            result mem_defined_funcs[i].return_type;
        conclude
        i = i + 1;
    conclude
    result "void";
end

algorithm mem_get_struct_size(struct_name : series) -> whole
begin
    datum i : whole = 0;
    datum len : whole = length(mem_defined_structs);
    cycle (i < len) do
        verify (mem_defined_structs[i].name == struct_name) then
            result length(mem_defined_structs[i].members) * 8;
        conclude
        i = i + 1;
    conclude
    result 0;
end

algorithm mem_get_member_index(struct_name : series, member_name : series) -> whole
begin
    datum i : whole = 0;
    datum len : whole = length(mem_defined_structs);
    cycle (i < len) do
        verify (mem_defined_structs[i].name == struct_name) then
            datum members : sequence<StructMember> = mem_defined_structs[i].members;
            datum j : whole = 0;
            datum mlen : whole = length(members);
            cycle (j < mlen) do
                verify (members[j].name == member_name) then
                    result j;
                conclude
                j = j + 1;
            conclude
        conclude
        i = i + 1;
    conclude
    result -1;
end

algorithm mem_get_member_type_by_index(struct_name : series, index : whole) -> series
begin
    datum i : whole = 0;
    datum len : whole = length(mem_defined_structs);
    cycle (i < len) do
        verify (mem_defined_structs[i].name == struct_name) then
             datum members : sequence<StructMember> = mem_defined_structs[i].members;
             verify (index < length(members)) then
                 result members[index].type_name;
             conclude
        conclude
        i = i + 1;
    conclude
    result "whole";
end
