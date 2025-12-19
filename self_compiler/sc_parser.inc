// Parser State
datum parser_pos : whole = 0;
datum parser_len : whole = 0;

algorithm parser_init() -> void
begin
    parser_pos = 0;
    parser_len = length(token_stream);
end

algorithm peek_token() -> Token
begin
    verify (parser_pos < parser_len) then
        result token_stream[parser_pos];
    otherwise
        // Return EOF token safely
        datum t : Token; t.type = TOKEN_EOF; result t;
    conclude
end

algorithm advance_token() -> Token
begin
    verify (parser_pos < parser_len) then
        datum t : Token = token_stream[parser_pos];
        parser_pos = parser_pos + 1;
        result t;
    otherwise
        datum t : Token; t.type = TOKEN_EOF; result t;
    conclude
end

algorithm match_token(type : whole) -> state
begin
    datum t : Token = peek_token();
    verify (t.type == type) then
        advance_token();
        result true;
    otherwise
        result false;
    conclude
end

algorithm check_token(type : whole) -> state
begin
    datum t : Token = peek_token();
    result t.type == type;
end

algorithm consume_token(type : whole, err_msg : series) -> Token
begin
    verify (check_token(type)) then
        result advance_token();
    otherwise
        reveal("Syntax Error: " + err_msg);
        datum t : Token = peek_token();
        reveal("Got token type: " + to_string(t.type) + " at line " + to_string(t.line));
        exit_program(1);
        // Return dummy to satisfy return type
        datum dummy : Token; result dummy;
    conclude
end

// Forward declarations
algorithm parse_program() -> whole;
algorithm parse_declaration() -> whole;
algorithm parse_var_decl() -> whole;
algorithm parse_func_decl() -> whole;
algorithm parse_struct_decl() -> whole;
algorithm parse_block() -> whole;
algorithm parse_statement() -> whole;
algorithm parse_expression() -> whole;
algorithm parse_logic_or() -> whole;
algorithm parse_logic_and() -> whole;
algorithm parse_comparison() -> whole;
algorithm parse_term() -> whole;
algorithm parse_factor() -> whole;
algorithm parse_unary() -> whole;
algorithm parse_primary() -> whole;
algorithm parse_type() -> whole;

algorithm parse_program() -> whole
begin
    datum prog : whole = create_node(NODE_PROGRAM);

    cycle (check_token(TOKEN_EOF) == false) do
        // FIX: Vector invalidation
        datum decl : whole = parse_declaration();
        append(node_heap[prog].children, decl);
    conclude

    result prog;
end

algorithm parse_declaration() -> whole
begin
    verify (match_token(TOKEN_DATUM)) then
        result parse_var_decl();
    otherwise verify (match_token(TOKEN_ALGORITHM)) then
        result parse_func_decl();
    otherwise verify (match_token(TOKEN_STRUCTURE)) then
        result parse_struct_decl();
    otherwise
        reveal("Error: Expected declaration at line " + to_string(peek_token().line));
        exit_program(1);
        result -1;
    conclude // structure
    conclude // algorithm
    conclude // datum
end

algorithm parse_var_decl() -> whole
begin
    // datum name : type = value ;
    datum name_tok : Token = consume_token(TOKEN_IDENTIFIER, "Expected variable name");
    consume_token(TOKEN_COLON, "Expected ':'");

    datum type_node : whole = parse_type();
    datum init_expr : whole = -1;

    verify (match_token(TOKEN_ASSIGN)) then
        init_expr = parse_expression();
    conclude

    consume_token(TOKEN_SEMICOLON, "Expected ';'");

    datum node_idx : whole = create_node(NODE_VAR_DECL);
    node_heap[node_idx].text_val = name_tok.value;
    node_heap[node_idx].type_node = type_node;
    node_heap[node_idx].initializer = init_expr;

    result node_idx;
end

algorithm parse_func_decl() -> whole
begin
    // algorithm name ( params ) -> type begin ... end
    datum name_tok : Token = consume_token(TOKEN_IDENTIFIER, "Expected function name");
    consume_token(TOKEN_LPAREN, "Expected '('");

    datum node_idx : whole = create_node(NODE_FUNC_DECL);
    node_heap[node_idx].text_val = name_tok.value;

    // Params
    verify (check_token(TOKEN_RPAREN) == false) then
        // First param
        datum pname : Token = consume_token(TOKEN_IDENTIFIER, "Expected param name");
        consume_token(TOKEN_COLON, "Expected ':'");
        datum ptype : whole = parse_type();
        datum param_node : whole = create_node(NODE_PARAM);
        node_heap[param_node].text_val = pname.value;
        node_heap[param_node].type_node = ptype;
        append(node_heap[node_idx].children, param_node);

        cycle (match_token(TOKEN_COMMA)) do
            datum pname2 : Token = consume_token(TOKEN_IDENTIFIER, "Expected param name");
            consume_token(TOKEN_COLON, "Expected ':'");
            datum ptype2 : whole = parse_type();
            datum param_node2 : whole = create_node(NODE_PARAM);
            node_heap[param_node2].text_val = pname2.value;
            node_heap[param_node2].type_node = ptype2;
            append(node_heap[node_idx].children, param_node2);
        conclude
    conclude

    consume_token(TOKEN_RPAREN, "Expected ')'");

    // Return Type
    datum ret_type : whole = create_node(NODE_TYPE);
    node_heap[ret_type].text_val = "void"; // Default

    verify (match_token(TOKEN_ARROW)) then
        ret_type = parse_type();
    conclude
    node_heap[node_idx].type_node = ret_type;

    // FIX: Handle forward declaration (semicolon instead of body)
    verify (match_token(TOKEN_SEMICOLON)) then
        node_heap[node_idx].then_branch = -1;
    otherwise
        consume_token(TOKEN_BEGIN, "Expected 'begin' or ';'");
        node_heap[node_idx].then_branch = parse_block(); // reusing then_branch for body
        consume_token(TOKEN_END, "Expected 'end' after function body");
    conclude

    result node_idx;
end

algorithm parse_struct_decl() -> whole
begin
    datum name : Token = consume_token(TOKEN_IDENTIFIER, "Structure name");
    consume_token(TOKEN_BEGIN, "begin");

    datum node_idx : whole = create_node(NODE_STRUCT_DECL);
    node_heap[node_idx].text_val = name.value;

    cycle (check_token(TOKEN_END) == false and check_token(TOKEN_EOF) == false) do
        verify (match_token(TOKEN_DATUM)) then
            // FIX: Vector invalidation
            datum decl : whole = parse_var_decl();
            append(node_heap[node_idx].children, decl);
        otherwise
            reveal("Expected datum in struct");
            exit_program(1);
        conclude
    conclude

    consume_token(TOKEN_END, "end");
    result node_idx;
end

algorithm parse_block() -> whole
begin
    datum blk : whole = create_node(NODE_BLOCK);
    cycle (check_token(TOKEN_END) == false and
           check_token(TOKEN_OTHERWISE) == false and
           check_token(TOKEN_CONCLUDE) == false and
           check_token(TOKEN_EOF) == false) do
        // FIX: Vector invalidation
        datum stmt : whole = parse_statement();
        append(node_heap[blk].children, stmt);
    conclude
    result blk;
end

algorithm parse_statement() -> whole
begin
    verify (match_token(TOKEN_VERIFY)) then
        // Verify
        consume_token(TOKEN_LPAREN, "(");
        datum cond : whole = parse_expression();
        consume_token(TOKEN_RPAREN, ")");
        consume_token(TOKEN_THEN, "then");

        datum if_node : whole = create_node(NODE_IF);
        node_heap[if_node].condition = cond;
        node_heap[if_node].then_branch = parse_block();

        verify (match_token(TOKEN_OTHERWISE)) then
            node_heap[if_node].else_branch = parse_block();
        conclude

        consume_token(TOKEN_CONCLUDE, "conclude");
        result if_node;

    otherwise verify (match_token(TOKEN_CYCLE)) then
        // Cycle
        consume_token(TOKEN_LPAREN, "(");
        datum cond : whole = parse_expression();
        consume_token(TOKEN_RPAREN, ")");
        consume_token(TOKEN_DO, "do");

        datum cycle_node : whole = create_node(NODE_WHILE);
        node_heap[cycle_node].condition = cond;
        node_heap[cycle_node].then_branch = parse_block(); // reusing then_branch for body
        consume_token(TOKEN_CONCLUDE, "conclude");
        result cycle_node;

    otherwise verify (match_token(TOKEN_RESULT)) then
        // Result
        datum ret_node : whole = create_node(NODE_RETURN);
        verify (check_token(TOKEN_SEMICOLON) == false) then
            node_heap[ret_node].initializer = parse_expression(); // reusing initializer for value
        conclude
        consume_token(TOKEN_SEMICOLON, ";");
        result ret_node;

    otherwise verify (match_token(TOKEN_DATUM)) then
        result parse_var_decl();

    otherwise verify (match_token(TOKEN_REVEAL)) then
        // Reveal (syntactic sugar for function call statement)
        consume_token(TOKEN_LPAREN, "(");
        datum call_node : whole = create_node(NODE_FUNC_CALL);
        node_heap[call_node].text_val = "reveal";

        verify (check_token(TOKEN_RPAREN) == false) then
            // FIX: Vector invalidation
            datum arg : whole = parse_expression();
            append(node_heap[call_node].children, arg);
            cycle (match_token(TOKEN_COMMA)) do
                // FIX: Vector invalidation
                datum arg2 : whole = parse_expression();
                append(node_heap[call_node].children, arg2);
            conclude
        conclude
        consume_token(TOKEN_RPAREN, ")");
        consume_token(TOKEN_SEMICOLON, ";");

        datum stmt : whole = create_node(NODE_EXPR_STMT);
        node_heap[stmt].initializer = call_node;
        result stmt;

    otherwise
        // Expr Stmt or Assignment
        datum expr : whole = parse_expression();

        verify (match_token(TOKEN_ASSIGN)) then
            datum val : whole = parse_expression();
            consume_token(TOKEN_SEMICOLON, ";");

            // Check expr type for Assignment target
            datum kind : whole = node_heap[expr].kind;

            verify (kind == NODE_IDENTIFIER) then
                datum assign : whole = create_node(NODE_ASSIGN);
                node_heap[assign].text_val = node_heap[expr].text_val;
                node_heap[assign].initializer = val; // value
                result assign;
            otherwise verify (kind == NODE_ARRAY_ACCESS) then
                datum arr_assign : whole = create_node(NODE_ARRAY_ASSIGN);
                node_heap[arr_assign].text_val = node_heap[expr].text_val; // array name
                node_heap[arr_assign].condition = node_heap[expr].condition; // index
                node_heap[arr_assign].initializer = val;
                result arr_assign;
            otherwise verify (kind == NODE_MEMBER_ACCESS) then
                datum mem_assign : whole = create_node(NODE_MEMBER_ASSIGN);
                node_heap[mem_assign].left = node_heap[expr].left; // object
                node_heap[mem_assign].text_val = node_heap[expr].text_val; // member
                node_heap[mem_assign].initializer = val;
                result mem_assign;
            otherwise
                reveal("Invalid assignment target");
                exit_program(1);
            conclude // assignment target check
            conclude // assignment target check (inner else if)
            conclude // assignment target check (inner else if)
        otherwise
            consume_token(TOKEN_SEMICOLON, ";");
            datum estmt : whole = create_node(NODE_EXPR_STMT);
            node_heap[estmt].initializer = expr;
            result estmt;
        conclude // assign match
    conclude // reveal
    conclude // datum
    conclude // result
    conclude // cycle
    conclude // verify
    result -1; // Should not reach
end

algorithm parse_expression() -> whole
begin
    result parse_logic_or();
end

algorithm parse_logic_or() -> whole
begin
    datum expr : whole = parse_logic_and();
    cycle (match_token(TOKEN_OR)) do
        datum right : whole = parse_logic_and();
        datum new_expr : whole = create_node(NODE_BINARY_OP);
        node_heap[new_expr].left = expr;
        node_heap[new_expr].right = right;
        node_heap[new_expr].op = "or";
        expr = new_expr;
    conclude
    result expr;
end

algorithm parse_logic_and() -> whole
begin
    datum expr : whole = parse_comparison();
    cycle (match_token(TOKEN_AND)) do
        datum right : whole = parse_comparison();
        datum new_expr : whole = create_node(NODE_BINARY_OP);
        node_heap[new_expr].left = expr;
        node_heap[new_expr].right = right;
        node_heap[new_expr].op = "and";
        expr = new_expr;
    conclude
    result expr;
end

algorithm parse_comparison() -> whole
begin
    datum expr : whole = parse_term();
    // FIX: Added TOKEN_GE (47), TOKEN_LE (48), TOKEN_NEQ (49)
    cycle (check_token(TOKEN_GT) or check_token(TOKEN_LT) or check_token(TOKEN_EQ) or check_token(TOKEN_GE) or check_token(TOKEN_LE) or check_token(TOKEN_NEQ)) do
        datum op_tok : Token = advance_token();
        datum right : whole = parse_term();
        datum new_expr : whole = create_node(NODE_BINARY_OP);
        node_heap[new_expr].left = expr;
        node_heap[new_expr].right = right;
        node_heap[new_expr].op = op_tok.value;
        expr = new_expr;
    conclude
    result expr;
end

algorithm parse_term() -> whole
begin
    datum expr : whole = parse_factor();
    cycle (check_token(TOKEN_PLUS) or check_token(TOKEN_MINUS)) do
        datum op_tok : Token = advance_token();
        datum right : whole = parse_factor();
        datum new_expr : whole = create_node(NODE_BINARY_OP);
        node_heap[new_expr].left = expr;
        node_heap[new_expr].right = right;
        node_heap[new_expr].op = op_tok.value;
        expr = new_expr;
    conclude
    result expr;
end

algorithm parse_factor() -> whole
begin
    datum expr : whole = parse_unary();
    cycle (check_token(TOKEN_STAR) or check_token(TOKEN_SLASH) or check_token(TOKEN_MODULO)) do
        datum op_tok : Token = advance_token();
        datum right : whole = parse_unary();
        datum new_expr : whole = create_node(NODE_BINARY_OP);
        node_heap[new_expr].left = expr;
        node_heap[new_expr].right = right;
        node_heap[new_expr].op = op_tok.value;
        expr = new_expr;
    conclude
    result expr;
end

algorithm parse_unary() -> whole
begin
    verify (match_token(TOKEN_NOT)) then
        datum right : whole = parse_unary();
        datum node : whole = create_node(NODE_BINARY_OP);
        node_heap[node].op = "not";
        node_heap[node].right = right;
        result node;
    otherwise verify (match_token(TOKEN_MINUS)) then
        datum right : whole = parse_unary();
        datum node : whole = create_node(NODE_BINARY_OP);
        node_heap[node].op = "-";
        node_heap[node].right = right; // Unary minus only has right in current struct logic or left=null
        result node;
    conclude
    conclude // not
    result parse_primary();
end

algorithm parse_primary() -> whole
begin
    datum expr : whole = -1;

    verify (match_token(TOKEN_INQUIRE)) then
        consume_token(TOKEN_LPAREN, "(");
        consume_token(TOKEN_RPAREN, ")");
        expr = create_node(NODE_INQUIRE);
    otherwise verify (match_token(TOKEN_LBRACKET)) then
        // Array literal
        expr = create_node(NODE_ARRAY_LITERAL);
        verify (check_token(TOKEN_RBRACKET) == false) then
            // Fix vector invalidation
            datum el : whole = parse_expression();
            append(node_heap[expr].children, el);
            cycle (match_token(TOKEN_COMMA)) do
                // Fix vector invalidation
                datum el2 : whole = parse_expression();
                append(node_heap[expr].children, el2);
            conclude
        conclude
        consume_token(TOKEN_RBRACKET, "]");
    otherwise verify (match_token(TOKEN_INTEGER_LITERAL)) then
        datum t : Token = token_stream[parser_pos - 1];
        expr = create_node(NODE_LITERAL);
        node_heap[expr].value_type = "whole";
        node_heap[expr].text_val = t.value;
        node_heap[expr].int_val = to_integer(t.value);
    otherwise verify (match_token(TOKEN_FLOAT_LITERAL)) then
        datum t : Token = token_stream[parser_pos - 1];
        expr = create_node(NODE_LITERAL);
        node_heap[expr].value_type = "precise";
        node_heap[expr].text_val = t.value;
    otherwise verify (match_token(TOKEN_STRING_LITERAL)) then
        datum t : Token = token_stream[parser_pos - 1];
        expr = create_node(NODE_LITERAL);
        node_heap[expr].value_type = "series";
        node_heap[expr].text_val = t.value;
    otherwise verify (match_token(TOKEN_BOOLEAN_LITERAL)) then
        datum t : Token = token_stream[parser_pos - 1];
        expr = create_node(NODE_LITERAL);
        node_heap[expr].value_type = "state";
        verify (t.value == "true") then node_heap[expr].bool_val = true; conclude
    otherwise verify (match_token(TOKEN_IDENTIFIER)) then
        datum name : series = token_stream[parser_pos - 1].value;

        verify (match_token(TOKEN_LPAREN)) then
            // Function Call
            expr = create_node(NODE_FUNC_CALL);
            node_heap[expr].text_val = name;
            verify (check_token(TOKEN_RPAREN) == false) then
                // Fix vector invalidation
                datum arg : whole = parse_expression();
                append(node_heap[expr].children, arg);
                cycle (match_token(TOKEN_COMMA)) do
                    // Fix vector invalidation
                    datum arg2 : whole = parse_expression();
                    append(node_heap[expr].children, arg2);
                conclude
            conclude
            consume_token(TOKEN_RPAREN, ")");
        otherwise verify (match_token(TOKEN_LBRACKET)) then
            // Array Access
            datum index : whole = parse_expression();
            consume_token(TOKEN_RBRACKET, "]");
            expr = create_node(NODE_ARRAY_ACCESS);
            node_heap[expr].text_val = name;
            node_heap[expr].condition = index;
        otherwise
            expr = create_node(NODE_IDENTIFIER);
            node_heap[expr].text_val = name;
        conclude // inner ID chain (LBRACKET)
        conclude // inner ID chain (LPAREN)
    otherwise verify (match_token(TOKEN_LPAREN)) then
        expr = parse_expression();
        consume_token(TOKEN_RPAREN, ")");
    otherwise
        reveal("Unexpected token in primary");
        exit_program(1);
    conclude // lparen
    conclude // id
    conclude // boolean
    conclude // string
    conclude // float
    conclude // integer
    conclude // lbracket
    conclude // inquire

    // Chaining
    cycle (match_token(TOKEN_DOT)) do
        datum mem : Token = consume_token(TOKEN_IDENTIFIER, "member");
        datum parent : whole = expr;
        expr = create_node(NODE_MEMBER_ACCESS);
        node_heap[expr].left = parent;
        node_heap[expr].text_val = mem.value;
    conclude

    result expr;
end

algorithm parse_type() -> whole
begin
    datum node : whole = create_node(NODE_TYPE);

    verify (match_token(TOKEN_TYPE_WHOLE)) then node_heap[node].text_val = "whole";
    otherwise verify (match_token(TOKEN_TYPE_PRECISE)) then node_heap[node].text_val = "precise";
    otherwise verify (match_token(TOKEN_TYPE_SERIES)) then node_heap[node].text_val = "series";
    otherwise verify (match_token(TOKEN_TYPE_STATE)) then node_heap[node].text_val = "state";
    otherwise verify (match_token(TOKEN_TYPE_SEQUENCE)) then
        node_heap[node].text_val = "sequence";
        consume_token(TOKEN_LT, "<");
        node_heap[node].subtype = parse_type();
        consume_token(TOKEN_GT, ">");
    otherwise verify (match_token(TOKEN_IDENTIFIER)) then
        datum t : Token = token_stream[parser_pos - 1];
        node_heap[node].text_val = t.value;
    otherwise
        reveal("Expected type");
        exit_program(1);
    conclude // id
    conclude // sequence
    conclude // state
    conclude // series
    conclude // precise
    conclude // whole
    result node;
end
