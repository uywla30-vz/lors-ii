// Lexer State
datum lexer_source : series = "";
datum lexer_len : whole = 0;
datum lexer_pos : whole = 0;
datum lexer_line : whole = 1;
datum lexer_col : whole = 1;

datum token_stream : sequence<Token>;

algorithm lexer_init(source : series) -> void
begin
    lexer_source = source;
    lexer_len = length(source);
    lexer_pos = 0;
    lexer_line = 1;
    lexer_col = 1;
end

algorithm peek_char(offset : whole) -> whole
begin
    verify (lexer_pos + offset < lexer_len) then
        result char_at(lexer_source, lexer_pos + offset);
    otherwise
        result 0; // EOF
    conclude
end

algorithm advance_char() -> whole
begin
    datum c : whole = peek_char(0);
    lexer_pos = lexer_pos + 1;
    verify (c == 10) then // Newline
        lexer_line = lexer_line + 1;
        lexer_col = 1;
    otherwise
        lexer_col = lexer_col + 1;
    conclude
    result c;
end

algorithm add_token(type : whole, val : series) -> void
begin
    datum t : Token;
    t.type = type;
    t.value = val;
    t.line = lexer_line;
    t.column = lexer_col; // Approximation
    append(token_stream, t);
end

algorithm read_while_digit() -> series
begin
    datum start : whole = lexer_pos;
    cycle (is_digit(peek_char(0))) do
        advance_char();
    conclude
    result substring(lexer_source, start, lexer_pos - start);
end

algorithm lex_number() -> void
begin
    datum start_col : whole = lexer_col;
    datum val : series = read_while_digit();
    datum type : whole = TOKEN_INTEGER_LITERAL;

    verify (peek_char(0) == 46) then // '.'
        advance_char(); // skip dot
        val = val + "." + read_while_digit();
        type = TOKEN_FLOAT_LITERAL;
    conclude

    add_token(type, val);
end

algorithm lex_identifier_or_keyword() -> void
begin
    datum start : whole = lexer_pos;
    // FIX: Allow _ (95) in identifier body
    cycle (is_alnum(peek_char(0)) or peek_char(0) == 95) do
        advance_char();
    conclude
    datum text : series = substring(lexer_source, start, lexer_pos - start);

    datum type : whole = TOKEN_IDENTIFIER;

    // Keyword Check
    verify (text == "datum") then type = TOKEN_DATUM; conclude
    verify (text == "verify") then type = TOKEN_VERIFY; conclude
    verify (text == "then") then type = TOKEN_THEN; conclude
    verify (text == "otherwise") then type = TOKEN_OTHERWISE; conclude
    verify (text == "conclude") then type = TOKEN_CONCLUDE; conclude
    verify (text == "cycle") then type = TOKEN_CYCLE; conclude
    verify (text == "do") then type = TOKEN_DO; conclude
    verify (text == "algorithm") then type = TOKEN_ALGORITHM; conclude
    verify (text == "begin") then type = TOKEN_BEGIN; conclude
    verify (text == "end") then type = TOKEN_END; conclude
    verify (text == "result") then type = TOKEN_RESULT; conclude
    verify (text == "reveal") then type = TOKEN_REVEAL; conclude
    verify (text == "inquire") then type = TOKEN_INQUIRE; conclude
    verify (text == "incorporate") then type = TOKEN_INCORPORATE; conclude
    verify (text == "structure") then type = TOKEN_STRUCTURE; conclude
    verify (text == "and") then type = TOKEN_AND; conclude
    verify (text == "or") then type = TOKEN_OR; conclude
    verify (text == "not") then type = TOKEN_NOT; conclude

    verify (text == "whole") then type = TOKEN_TYPE_WHOLE; conclude
    verify (text == "precise") then type = TOKEN_TYPE_PRECISE; conclude
    verify (text == "series") then type = TOKEN_TYPE_SERIES; conclude
    verify (text == "state") then type = TOKEN_TYPE_STATE; conclude
    verify (text == "sequence") then type = TOKEN_TYPE_SEQUENCE; conclude

    verify (text == "true") then type = TOKEN_BOOLEAN_LITERAL; conclude
    verify (text == "false") then type = TOKEN_BOOLEAN_LITERAL; conclude

    add_token(type, text);
end

algorithm lex_string() -> void
begin
    advance_char(); // Skip opening quote
    datum start : whole = lexer_pos;
    datum running : state = true;

    // Handle escapes manually without using \" literal in this source code
    // 34 is quote, 92 is backslash
    cycle (running) do
        datum c : whole = peek_char(0);

        verify (c == 0) then
            reveal("Error: Unterminated string literal");
            exit_program(1);
        conclude

        verify (c == 34) then // quote
            running = false;
        otherwise
            verify (c == 92) then // backslash
                advance_char(); // Skip backslash
                advance_char(); // Skip escaped char
            otherwise
                advance_char();
            conclude
        conclude
    conclude

    datum text : series = substring(lexer_source, start, lexer_pos - start);
    advance_char(); // Skip closing quote
    add_token(TOKEN_STRING_LITERAL, text);
end

algorithm tokenize() -> void
begin
    cycle (lexer_pos < lexer_len) do
        datum c : whole = peek_char(0);

        // Whitespace
        verify (is_space(c)) then
            advance_char();
        // Comment //
        otherwise verify (c == 47 and peek_char(1) == 47) then
            cycle (not (peek_char(0) == 10) and not (peek_char(0) == 0)) do
                advance_char();
            conclude
        // Numbers
        otherwise verify (is_digit(c)) then
            lex_number();
        // Identifiers
        // FIX: Allow _ (95) as start char
        otherwise verify (is_alpha(c) or c == 95) then
            lex_identifier_or_keyword();
        // String
        otherwise verify (c == 34) then // '"'
            lex_string();
        // Operators
        otherwise
            datum handled : state = false;
            datum next : whole = peek_char(1);

            // 2-char operators
            verify (not handled and c == 45 and next == 62) then // ->
                add_token(TOKEN_ARROW, "->"); advance_char(); advance_char(); handled = true;
            conclude
            verify (not handled and c == 61 and next == 61) then // ==
                add_token(TOKEN_EQ, "=="); advance_char(); advance_char(); handled = true;
            conclude
            verify (not handled and c == 33 and next == 61) then // !=
                add_token(TOKEN_NEQ, "!="); advance_char(); advance_char(); handled = true;
            conclude
            verify (not handled and c == 62 and next == 61) then // >=
                add_token(TOKEN_GE, ">="); advance_char(); advance_char(); handled = true;
            conclude
            verify (not handled and c == 60 and next == 61) then // <=
                add_token(TOKEN_LE, "<="); advance_char(); advance_char(); handled = true;
            conclude

            // Single char
            verify (not handled and c == 43) then add_token(TOKEN_PLUS, "+"); advance_char(); handled = true; conclude
            verify (not handled and c == 45) then add_token(TOKEN_MINUS, "-"); advance_char(); handled = true; conclude
            verify (not handled and c == 42) then add_token(TOKEN_STAR, "*"); advance_char(); handled = true; conclude
            verify (not handled and c == 47) then add_token(TOKEN_SLASH, "/"); advance_char(); handled = true; conclude
            verify (not handled and c == 37) then add_token(TOKEN_MODULO, "%"); advance_char(); handled = true; conclude
            verify (not handled and c == 61) then add_token(TOKEN_ASSIGN, "="); advance_char(); handled = true; conclude
            verify (not handled and c == 46) then add_token(TOKEN_DOT, "."); advance_char(); handled = true; conclude
            verify (not handled and c == 58) then add_token(TOKEN_COLON, ":"); advance_char(); handled = true; conclude
            verify (not handled and c == 59) then add_token(TOKEN_SEMICOLON, ";"); advance_char(); handled = true; conclude
            verify (not handled and c == 44) then add_token(TOKEN_COMMA, ","); advance_char(); handled = true; conclude
            verify (not handled and c == 40) then add_token(TOKEN_LPAREN, "("); advance_char(); handled = true; conclude
            verify (not handled and c == 41) then add_token(TOKEN_RPAREN, ")"); advance_char(); handled = true; conclude
            verify (not handled and c == 91) then add_token(TOKEN_LBRACKET, "["); advance_char(); handled = true; conclude
            verify (not handled and c == 93) then add_token(TOKEN_RBRACKET, "]"); advance_char(); handled = true; conclude
            verify (not handled and c == 62) then add_token(TOKEN_GT, ">"); advance_char(); handled = true; conclude
            verify (not handled and c == 60) then add_token(TOKEN_LT, "<"); advance_char(); handled = true; conclude

            verify (handled == false) then
                reveal("Error: Unexpected character: " + character(c) + " (" + to_string(c) + ") at line " + to_string(lexer_line));
                exit_program(1);
            conclude
        conclude // End of verify(string)
        conclude // End of verify(alpha)
        conclude // End of verify(digit)
        conclude // End of verify(comment)
        conclude // End of verify(space)
    conclude // End of cycle
    add_token(TOKEN_EOF, "");
end

algorithm preprocess_includes(code : series, base_dir : series) -> series
begin
    datum result_code : series = "";
    datum len : whole = length(code);
    datum i : whole = 0;

    cycle (i < len) do
        datum line_start : whole = i;
        datum line_end : whole = i;
        cycle (line_end < len and not (char_at(code, line_end) == 10)) do
            line_end = line_end + 1;
        conclude

        datum line_str : series = substring(code, line_start, line_end - line_start);

        verify (starts_with(line_str, "incorporate")) then
            // Extract filename. Format: incorporate "file.inc"
            // Find first quote
            datum q1 : whole = -1;
            datum q2 : whole = -1;
            datum k : whole = 0;
            cycle (k < length(line_str)) do
                verify (char_at(line_str, k) == 34) then
                    verify (q1 == -1) then q1 = k;
                    otherwise verify (q2 == -1) then q2 = k; conclude
                    conclude
                conclude
                k = k + 1;
            conclude

            verify (not (q1 == -1) and not (q2 == -1)) then
                datum inc_path : series = substring(line_str, q1 + 1, q2 - q1 - 1);
                datum full_path : series = path_join(base_dir, inc_path);

                // Read included file
                verify (file_exists(full_path)) then
                    datum inc_content : series = file_read(full_path);
                    datum inc_dir : series = get_dirname(full_path);
                    // Recursively process
                    result_code = result_code + preprocess_includes(inc_content, inc_dir) + character(10);
                otherwise
                    reveal("Error: Include file not found: " + inc_path);
                    exit_program(1);
                conclude
            otherwise
                result_code = result_code + line_str + character(10);
            conclude
        otherwise
            result_code = result_code + line_str + character(10);
        conclude

        i = line_end + 1;
    conclude

    result result_code;
end
