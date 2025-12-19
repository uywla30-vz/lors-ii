from lors.src.tokens import Token, TokenType
from lors.src.ast_nodes import *

class Lexer:
    def __init__(self, source_code: str):
        self.source = source_code
        self.length = len(source_code)
        self.pos = 0
        self.line = 1
        self.column = 1
        self.keywords = {
            "datum": TokenType.DATUM,
            "verify": TokenType.VERIFY,
            "then": TokenType.THEN,
            "otherwise": TokenType.OTHERWISE,
            "conclude": TokenType.CONCLUDE,
            "cycle": TokenType.CYCLE,
            "do": TokenType.DO,
            "algorithm": TokenType.ALGORITHM,
            "begin": TokenType.BEGIN,
            "end": TokenType.END,
            "result": TokenType.RESULT,
            "reveal": TokenType.REVEAL,
            "inquire": TokenType.INQUIRE,
            "incorporate": TokenType.INCORPORATE,
            "structure": TokenType.STRUCTURE,
            "and": TokenType.AND,
            "or": TokenType.OR,
            "not": TokenType.NOT,
            "whole": TokenType.TYPE_WHOLE,
            "precise": TokenType.TYPE_PRECISE,
            "series": TokenType.TYPE_SERIES,
            "state": TokenType.TYPE_STATE,
            "sequence": TokenType.TYPE_SEQUENCE,
            "true": TokenType.BOOLEAN_LITERAL,
            "false": TokenType.BOOLEAN_LITERAL
        }

    def tokenize(self):
        tokens = []
        while self.pos < self.length:
            char = self.source[self.pos]

            # Skip whitespace
            if char.isspace():
                if char == '\n':
                    self.line += 1
                    self.column = 1
                else:
                    self.column += 1
                self.pos += 1
                continue

            # Skip comments
            if char == '/' and self.peek() == '/':
                while self.pos < self.length and self.source[self.pos] != '\n':
                    self.pos += 1
                continue

            # Numbers
            if char.isdigit():
                tokens.append(self.read_number())
                continue

            # Identifiers and Keywords
            if char.isalpha() or char == '_':
                tokens.append(self.read_identifier())
                continue

            # String Literals
            if char == '"':
                tokens.append(self.read_string())
                continue

            # Operators and Punctuation
            token = self.read_operator(char)
            if token:
                tokens.append(token)
                continue

            raise SyntaxError(f"Unexpected character '{char}' at line {self.line}, column {self.column}")

        tokens.append(Token(TokenType.EOF, "", self.line, self.column))
        return tokens

    def peek(self, offset=1):
        if self.pos + offset < self.length:
            return self.source[self.pos + offset]
        return None

    def read_number(self):
        start_pos = self.pos
        start_col = self.column
        dot_seen = False

        while self.pos < self.length:
            char = self.source[self.pos]
            if char.isdigit():
                self.pos += 1
                self.column += 1
            elif char == '.':
                if dot_seen:
                    break
                dot_seen = True
                self.pos += 1
                self.column += 1
            else:
                break

        value = self.source[start_pos:self.pos]
        type_ = TokenType.FLOAT_LITERAL if dot_seen else TokenType.INTEGER_LITERAL
        return Token(type_, value, self.line, start_col)

    def read_identifier(self):
        start_pos = self.pos
        start_col = self.column

        while self.pos < self.length:
            char = self.source[self.pos]
            if char.isalnum() or char == '_':
                self.pos += 1
                self.column += 1
            else:
                break

        value = self.source[start_pos:self.pos]
        type_ = self.keywords.get(value, TokenType.IDENTIFIER)
        return Token(type_, value, self.line, start_col)

    def read_string(self):
        start_col = self.column
        self.pos += 1 # Skip opening quote
        self.column += 1
        start_pos = self.pos

        while self.pos < self.length and self.source[self.pos] != '"':
            if self.source[self.pos] == '\n':
                self.line += 1
                self.column = 1
            else:
                self.column += 1
            self.pos += 1

        if self.pos >= self.length:
            raise SyntaxError(f"Unterminated string literal at line {self.line}")

        value = self.source[start_pos:self.pos]
        self.pos += 1 # Skip closing quote
        self.column += 1
        return Token(TokenType.STRING_LITERAL, value, self.line, start_col)

    def read_operator(self, char):
        start_col = self.column
        # Check for two-character operators
        next_char = self.peek()

        if char == '-' and next_char == '>':
            self.pos += 2
            self.column += 2
            return Token(TokenType.ARROW, "->", self.line, start_col)

        if char == '=' and next_char == '=':
            self.pos += 2
            self.column += 2
            return Token(TokenType.EQ, "==", self.line, start_col)

        if char == '>' and next_char == '=':
            self.pos += 2
            self.column += 2
            return Token(TokenType.GE, ">=", self.line, start_col)

        if char == '<' and next_char == '=':
            self.pos += 2
            self.column += 2
            return Token(TokenType.LE, "<=", self.line, start_col)

        if char == '!' and next_char == '=':
            self.pos += 2
            self.column += 2
            return Token(TokenType.NEQ, "!=", self.line, start_col)

        # Single character operators
        self.pos += 1
        self.column += 1

        match char:
            case '+': return Token(TokenType.PLUS, "+", self.line, start_col)
            case '-': return Token(TokenType.MINUS, "-", self.line, start_col)
            case '*': return Token(TokenType.STAR, "*", self.line, start_col)
            case '/': return Token(TokenType.SLASH, "/", self.line, start_col)
            case '%': return Token(TokenType.MODULO, "%", self.line, start_col)
            case '=': return Token(TokenType.ASSIGN, "=", self.line, start_col)
            case '.': return Token(TokenType.DOT, ".", self.line, start_col)
            case ':': return Token(TokenType.COLON, ":", self.line, start_col)
            case ';': return Token(TokenType.SEMICOLON, ";", self.line, start_col)
            case ',': return Token(TokenType.COMMA, ",", self.line, start_col)
            case '(': return Token(TokenType.LPAREN, "(", self.line, start_col)
            case ')': return Token(TokenType.RPAREN, ")", self.line, start_col)
            case '[': return Token(TokenType.LBRACKET, "[", self.line, start_col)
            case ']': return Token(TokenType.RBRACKET, "]", self.line, start_col)
            case '>': return Token(TokenType.GT, ">", self.line, start_col)
            case '<': return Token(TokenType.LT, "<", self.line, start_col)

        # If we get here, it wasn't a recognized operator, revert and return None (let caller handle error)
        self.pos -= 1
        self.column -= 1
        return None
