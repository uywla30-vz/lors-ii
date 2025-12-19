// Code Generator State
datum codegen_output : series = "";
datum indent_level : whole = 0;

// Forward declarations
algorithm emit(line : series) -> void;
algorithm map_type_node(node_idx : whole) -> series;
algorithm gen_expression(node_idx : whole) -> series;
algorithm visit_stmt(node_idx : whole) -> void;
algorithm visit_block(node_idx : whole) -> void;
algorithm generate_code(prog_idx : whole) -> void;

algorithm emit(line : series) -> void
begin
    datum ind : series = "";
    datum i : whole = 0;
    cycle (i < indent_level) do
        ind = ind + "    ";
        i = i + 1;
    conclude
    codegen_output = codegen_output + ind + line + character(10);
end

algorithm map_type_node(node_idx : whole) -> series
begin
    datum name : series = node_heap[node_idx].text_val;
    verify (name == "whole") then result "long long"; conclude
    verify (name == "precise") then result "double"; conclude
    verify (name == "series") then result "std::string"; conclude
    verify (name == "state") then result "bool"; conclude
    verify (name == "void") then result "void"; conclude
    verify (name == "sequence") then
        datum sub : series = map_type_node(node_heap[node_idx].subtype);
        result "std::vector<" + sub + ">";
    conclude
    result name;
end

algorithm gen_expression(node_idx : whole) -> series
begin
    // --- AIRBAG ANTI-CRASH ---
    verify (node_idx == -1) then result "/*_ERROR_NULL_NODE_*/"; conclude

    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_LITERAL) then
        verify (node.value_type == "series") then
            result character(34) + node.text_val + character(34);
        conclude
        verify (node.value_type == "state") then
            verify (node.bool_val) then result "true"; otherwise result "false"; conclude
        conclude
        result node.text_val;
    conclude

    verify (k == NODE_IDENTIFIER) then
        result node.text_val;
    conclude

    verify (k == NODE_BINARY_OP) then
        datum op : series = node.op;

        // Unary NOT
        verify (op == "not") then result "(!" + gen_expression(node.right) + ")"; conclude

        // Unary MINUS
        verify (op == "-" and node.left == -1) then result "(-" + gen_expression(node.right) + ")"; conclude

        // Binary Protection
        verify (node.left == -1) then
            result "(/*ERR_MISSING_LEFT*/ " + op + " " + gen_expression(node.right) + ")";
        conclude

        datum l_str : series = gen_expression(node.left);
        datum r_str : series = gen_expression(node.right);

        verify (op == "and") then op = "&&"; conclude
        verify (op == "or") then op = "||"; conclude
        result "(" + l_str + " " + op + " " + r_str + ")";
    conclude

    verify (k == NODE_FUNC_CALL) then
        datum fname : series = node.text_val;
        datum args : sequence<whole> = node.children;

        verify (fname == "length") then result "((long long)" + gen_expression(args[0]) + ".size())"; conclude
        verify (fname == "to_string") then result "std::to_string(" + gen_expression(args[0]) + ")"; conclude
        verify (fname == "append") then result "invalid_expression_append_is_void"; conclude
        verify (fname == "arg_count") then result "((long long)global_argc)"; conclude
        verify (fname == "arg_value") then result "std::string(global_argv[" + gen_expression(args[0]) + "])"; conclude
        verify (fname == "root") then fname = "std::sqrt"; conclude

        // --- ADDED MATH INTRINSICS ---
        verify (fname == "power") then fname = "std::pow"; conclude
        verify (fname == "absolute") then fname = "std::abs"; conclude
        verify (fname == "sine") then fname = "std::sin"; conclude
        verify (fname == "cosine") then fname = "std::cos"; conclude
        verify (fname == "tangent") then fname = "std::tan"; conclude

        // --- ADDED STRING/CHAR INTRINSICS ---
        verify (fname == "to_upper") then result "str_upper_helper(" + gen_expression(args[0]) + ")"; conclude
        verify (fname == "to_lower") then result "str_lower_helper(" + gen_expression(args[0]) + ")"; conclude
        verify (fname == "reverse") then result "str_reverse_helper(" + gen_expression(args[0]) + ")"; conclude

        // FIX: Remove [0] as input is integer char code
        verify (fname == "is_digit") then result "((bool)isdigit((unsigned char)" + gen_expression(args[0]) + "))"; conclude
        verify (fname == "is_alpha") then result "((bool)isalpha((unsigned char)" + gen_expression(args[0]) + "))"; conclude
        verify (fname == "is_alnum") then result "((bool)isalnum((unsigned char)" + gen_expression(args[0]) + "))"; conclude
        verify (fname == "is_space") then result "((bool)isspace((unsigned char)" + gen_expression(args[0]) + "))"; conclude
        verify (fname == "is_upper") then result "((bool)isupper((unsigned char)" + gen_expression(args[0]) + "))"; conclude
        verify (fname == "is_lower") then result "((bool)islower((unsigned char)" + gen_expression(args[0]) + "))"; conclude

        verify (fname == "character") then result "std::string(1, (char)" + gen_expression(args[0]) + ")"; conclude

        // --- ADDED SYSTEM INTRINSICS ---
        verify (fname == "env_get") then
            // FIX: Wrap in std::string to handle literals safely
            datum arg : series = gen_expression(args[0]);
            result "(std::getenv(std::string(" + arg + ").c_str()) ? std::string(std::getenv(std::string(" + arg + ").c_str())) : std::string())";
        conclude

        // -----------------------

        // ADDED BUILT-INS
        verify (fname == "char_at") then result "((long long)" + gen_expression(args[0]) + "[" + gen_expression(args[1]) + "])"; conclude
        verify (fname == "substring") then result "str_substr_helper(" + gen_expression(args[0]) + ", " + gen_expression(args[1]) + ", " + gen_expression(args[2]) + ")"; conclude
        verify (fname == "to_integer") then result "std::stoll(" + gen_expression(args[0]) + ")"; conclude
        verify (fname == "to_precise") then result "std::stod(" + gen_expression(args[0]) + ")"; conclude
        verify (fname == "file_exists") then result "file_exists_helper(" + gen_expression(args[0]) + ")"; conclude
        verify (fname == "file_remove") then result "file_remove_helper(" + gen_expression(args[0]) + ")"; conclude


        datum args_code : series = "";
        datum i : whole = 0;
        datum len : whole = length(args);
        cycle (i < len) do
            verify (i > 0) then args_code = args_code + ", "; conclude
            args_code = args_code + gen_expression(args[i]);
            i = i + 1;
        conclude
        result fname + "(" + args_code + ")";
    conclude

    verify (k == NODE_ARRAY_ACCESS) then
        result node.text_val + "[" + gen_expression(node.condition) + "]";
    conclude

    verify (k == NODE_MEMBER_ACCESS) then
        result gen_expression(node.left) + "." + node.text_val;
    conclude

    verify (k == NODE_ARRAY_LITERAL) then
        datum elems : series = "";
        datum children : sequence<whole> = node.children;
        datum i : whole = 0;
        datum len : whole = length(children);
        cycle (i < len) do
             verify (i > 0) then elems = elems + ", "; conclude
             elems = elems + gen_expression(children[i]);
             i = i + 1;
        conclude
        result "{ " + elems + " }";
    conclude

    verify (k == NODE_INQUIRE) then
        result "inquire()";
    conclude

    result "";
end

algorithm visit_stmt(node_idx : whole) -> void
begin
    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_VAR_DECL) then
        datum type_str : series = map_type_node(node.type_node);
        datum init : series = "";
        verify (not (node.initializer == -1)) then
            init = " = " + gen_expression(node.initializer);
        conclude
        emit(type_str + " " + node.text_val + init + ";");
    conclude

    verify (k == NODE_EXPR_STMT) then
        datum expr : whole = node.initializer;
        datum handled : state = false;

        verify (node_heap[expr].kind == NODE_FUNC_CALL) then
             datum name : series = node_heap[expr].text_val;
             datum children : sequence<whole> = node_heap[expr].children;

             verify (name == "reveal") then
                  datum args : series = "";
                  datum i : whole = 0;
                  datum len : whole = length(children);
                  verify (len == 0) then
                      emit("std::cout << std::endl;");
                  otherwise
                      cycle (i < len) do
                          verify (i > 0) then args = args + " << "; conclude
                          args = args + gen_expression(children[i]);
                          i = i + 1;
                      conclude
                      emit("std::cout << " + args + " << std::endl;");
                  conclude
                  handled = true;
             conclude

             verify (not handled and name == "append") then
                  datum seq : series = gen_expression(children[0]);
                  datum item : series = gen_expression(children[1]);
                  emit(seq + ".push_back(" + item + ");");
                  handled = true;
             conclude

             verify (not handled and name == "file_write") then
                  emit("file_write(" + gen_expression(children[0]) + ", " + gen_expression(children[1]) + ");");
                  handled = true;
             conclude

             verify (not handled and name == "execute_system") then
                  emit("execute_system(" + gen_expression(children[0]) + ");");
                  handled = true;
             conclude

             verify (not handled and name == "exit_program") then
                  emit("std::exit(" + gen_expression(children[0]) + ");");
                  handled = true;
             conclude
        conclude

        verify (not handled) then
             emit(gen_expression(expr) + ";");
        conclude
    conclude

    verify (k == NODE_ASSIGN) then
        emit(node.text_val + " = " + gen_expression(node.initializer) + ";");
    conclude

    verify (k == NODE_ARRAY_ASSIGN) then
        emit(node.text_val + "[" + gen_expression(node.condition) + "] = " + gen_expression(node.initializer) + ";");
    conclude

    verify (k == NODE_MEMBER_ASSIGN) then
        emit(gen_expression(node.left) + "." + node.text_val + " = " + gen_expression(node.initializer) + ";");
    conclude

    verify (k == NODE_RETURN) then
        verify (not (node.initializer == -1)) then
            emit("return " + gen_expression(node.initializer) + ";");
        otherwise
            emit("return;");
        conclude
    conclude

    verify (k == NODE_IF) then
        emit("if (" + gen_expression(node.condition) + ") {");
        indent_level = indent_level + 1;
        visit_block(node.then_branch);
        indent_level = indent_level - 1;

        verify (not (node.else_branch == -1)) then
            emit("} else {");
            indent_level = indent_level + 1;
            visit_block(node.else_branch);
            indent_level = indent_level - 1;
        conclude
        emit("}");
    conclude

    verify (k == NODE_WHILE) then
        emit("while (" + gen_expression(node.condition) + ") {");
        indent_level = indent_level + 1;
        visit_block(node.then_branch);
        indent_level = indent_level - 1;
        emit("}");
    conclude

    verify (k == NODE_BLOCK) then
        visit_block(node_idx);
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
    emit("#include <iostream>");
    emit("#include <string>");
    emit("#include <vector>");
    emit("#include <cmath>");
    emit("#include <fstream>");
    emit("#include <cstdlib>");
    emit("#include <cstdio>");
    emit("#include <sstream>");
    emit("#include <algorithm>");
    emit("");
    emit("// Generated by Self-Hosted Lors Compiler");
    emit("int global_argc;");
    emit("char** global_argv;");
    emit("");
    emit("struct InquireProxy { template<typename T> operator T() { T val; std::cin >> val; return val; } };");
    emit("InquireProxy inquire() { return InquireProxy(); }");
    emit("void file_write(std::string path, std::string content) { std::ofstream f(path); f << content; f.close(); }");
    emit("std::string file_read(std::string path) { std::ifstream f(path); std::stringstream buffer; buffer << f.rdbuf(); return buffer.str(); }");
    // FIX: execute_system returns int now
    emit("int execute_system(std::string cmd) { return std::system(cmd.c_str()); }");
    // ADDED HELPERS
    emit("bool file_exists_helper(std::string path) { std::ifstream f(path); return f.good(); }");
    emit("void file_remove_helper(std::string path) { std::remove(path.c_str()); }");

    emit("");

    emit("std::string str_reverse_helper(std::string s) { std::string rev = s; std::reverse(rev.begin(), rev.end()); return rev; }");
    emit("std::string str_upper_helper(std::string s) { std::string res = s; for(auto &c : res) c = toupper(c); return res; }");
    emit("std::string str_lower_helper(std::string s) { std::string res = s; for(auto &c : res) c = tolower(c); return res; }");
    emit("std::string str_substr_helper(std::string s, long long start, long long len) { if (start < 0 || start >= s.length()) return std::string(); return s.substr(start, len); }");
    emit("");

    datum prog : ASTNode = node_heap[prog_idx];
    datum i : whole = 0;
    datum children : sequence<whole> = prog.children;
    datum len : whole = length(children);

    cycle (i < len) do
        datum decl_idx : whole = children[i];
        datum k : whole = node_heap[decl_idx].kind;

        verify (k == NODE_STRUCT_DECL) then
            datum sname : series = node_heap[decl_idx].text_val;
            emit("struct " + sname + " {");
            indent_level = indent_level + 1;

            datum j : whole = 0;
            datum schildren : sequence<whole> = node_heap[decl_idx].children;
            datum flen : whole = length(schildren);
            cycle (j < flen) do
                visit_stmt(schildren[j]);
                j = j + 1;
            conclude

             verify (flen > 0) then
                emit(sname + "() {}");
                datum params_s : series = "";
                datum init_s : series = "";
                j = 0;
                cycle (j < flen) do
                     datum fidx : whole = schildren[j];
                     datum fname : series = node_heap[fidx].text_val;
                     datum ftype : series = map_type_node(node_heap[fidx].type_node);

                     verify (j > 0) then
                        params_s = params_s + ", ";
                        init_s = init_s + ", ";
                     conclude
                     params_s = params_s + ftype + " _" + fname;
                     init_s = init_s + fname + "(_" + fname + ")";

                     j = j + 1;
                conclude
                emit(sname + "(" + params_s + ") : " + init_s + " {}");
             conclude

            indent_level = indent_level - 1;
            emit("};");
            emit("");
        conclude

        verify (k == NODE_VAR_DECL) then
            visit_stmt(decl_idx);
        conclude

        verify (k == NODE_FUNC_DECL) then
            datum fname : series = node_heap[decl_idx].text_val;
            datum rtype : series = map_type_node(node_heap[decl_idx].type_node);
            datum params : series = "";

            verify (fname == "genesis") then
                fname = "main";
                rtype = "int";
                params = "int argc, char* argv[]";
                emit(rtype + " " + fname + "(" + params + ") {");
                emit("    global_argc = argc;");
                emit("    global_argv = argv;");

                // GENESIS BODY FIX
                indent_level = indent_level + 1;
                visit_block(node_heap[decl_idx].then_branch);
                indent_level = indent_level - 1;
                emit("}");
            otherwise
                datum j : whole = 0;
                datum fchildren : sequence<whole> = node_heap[decl_idx].children;
                datum plen : whole = length(fchildren);
                cycle (j < plen) do
                    verify (j > 0) then params = params + ", "; conclude
                    datum pidx : whole = fchildren[j];
                    params = params + map_type_node(node_heap[pidx].type_node) + " " + node_heap[pidx].text_val;
                    j = j + 1;
                conclude

                verify (not (node_heap[decl_idx].then_branch == -1)) then
                    emit(rtype + " " + fname + "(" + params + ") {");
                    indent_level = indent_level + 1;
                    visit_block(node_heap[decl_idx].then_branch);
                    indent_level = indent_level - 1;
                    emit("}");
                otherwise
                    emit(rtype + " " + fname + "(" + params + ");");
                conclude
            conclude

            emit("");
        conclude

        i = i + 1;
    conclude
end
