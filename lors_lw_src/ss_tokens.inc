// Token Types (Constants)
datum TOKEN_EOF : whole = 0;

// Keywords
datum TOKEN_DATUM : whole = 1;
datum TOKEN_VERIFY : whole = 2;
datum TOKEN_THEN : whole = 3;
datum TOKEN_OTHERWISE : whole = 4;
datum TOKEN_CONCLUDE : whole = 5;
datum TOKEN_CYCLE : whole = 6;
datum TOKEN_DO : whole = 7;
datum TOKEN_ALGORITHM : whole = 8;
datum TOKEN_BEGIN : whole = 9;
datum TOKEN_END : whole = 10;
datum TOKEN_RESULT : whole = 11;
datum TOKEN_REVEAL : whole = 12;
datum TOKEN_INQUIRE : whole = 13;
datum TOKEN_INCORPORATE : whole = 14;
datum TOKEN_STRUCTURE : whole = 15;
datum TOKEN_AND : whole = 16;
datum TOKEN_OR : whole = 17;
datum TOKEN_NOT : whole = 18;

// Types
datum TOKEN_TYPE_WHOLE : whole = 19;
datum TOKEN_TYPE_PRECISE : whole = 20;
datum TOKEN_TYPE_SERIES : whole = 21;
datum TOKEN_TYPE_STATE : whole = 22;
datum TOKEN_TYPE_SEQUENCE : whole = 23;

// Literals
datum TOKEN_INTEGER_LITERAL : whole = 24;
datum TOKEN_FLOAT_LITERAL : whole = 25;
datum TOKEN_STRING_LITERAL : whole = 26;
datum TOKEN_BOOLEAN_LITERAL : whole = 27;

// Identifier
datum TOKEN_IDENTIFIER : whole = 28;

// Operators & Punctuation
datum TOKEN_PLUS : whole = 29;
datum TOKEN_MINUS : whole = 30;
datum TOKEN_STAR : whole = 31;
datum TOKEN_SLASH : whole = 32;
datum TOKEN_MODULO : whole = 33;
datum TOKEN_ASSIGN : whole = 34;
datum TOKEN_DOT : whole = 35;
datum TOKEN_COLON : whole = 36;
datum TOKEN_SEMICOLON : whole = 37;
datum TOKEN_COMMA : whole = 38;
datum TOKEN_LPAREN : whole = 39;
datum TOKEN_RPAREN : whole = 40;
datum TOKEN_LBRACKET : whole = 41;
datum TOKEN_RBRACKET : whole = 42;
datum TOKEN_GT : whole = 43;
datum TOKEN_LT : whole = 44;
datum TOKEN_EQ : whole = 45;
datum TOKEN_ARROW : whole = 46;
datum TOKEN_GE : whole = 47;
datum TOKEN_LE : whole = 48;
datum TOKEN_NEQ : whole = 49;

structure Token begin
    datum type : whole;
    datum value : series;
    datum line : whole;
    datum column : whole;
end
