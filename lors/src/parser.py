from lors.src.tokens import TokenType, Token
from lors.src.ast_nodes import *

class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    def parse(self) -> Program:
        declarations = []
        while not self.is_at_end():
            declarations.append(self.parse_declaration())
        return Program(declarations)

    def parse_declaration(self):
        if self.match(TokenType.DATUM):
            return self.parse_variable_declaration()
        elif self.match(TokenType.ALGORITHM):
            return self.parse_function_declaration()
        elif self.match(TokenType.STRUCTURE):
            return self.parse_struct_declaration()
        else:
            raise SyntaxError(f"Expected declaration (datum, algorithm, or structure) at line {self.peek().line}")

    def parse_struct_declaration(self):
        name = self.consume(TokenType.IDENTIFIER, "Expected structure name").value
        self.consume(TokenType.BEGIN, "Expected 'begin' after structure name")

        fields = []
        while not self.check(TokenType.END) and not self.is_at_end():
            if self.match(TokenType.DATUM):
                fields.append(self.parse_variable_declaration())
            else:
                raise SyntaxError(f"Expected 'datum' field declaration in structure at line {self.peek().line}")

        self.consume(TokenType.END, "Expected 'end' after structure fields")
        return StructDeclaration(name, fields)

    def parse_variable_declaration(self):
        # datum name : type = value ;
        name = self.consume(TokenType.IDENTIFIER, "Expected variable name").value
        self.consume(TokenType.COLON, "Expected ':' after variable name")
        var_type = self.parse_type()

        initializer = None
        if self.match(TokenType.ASSIGN):
            # Check for struct instantiation: Type(args)
            # This is parsed as FunctionCall by parse_expression, which is fine.
            # CodeGen needs to handle it.
            initializer = self.parse_expression()

        self.consume(TokenType.SEMICOLON, "Expected ';' after variable declaration")
        return VariableDeclaration(name, var_type, initializer)

    def parse_function_declaration(self):
        # algorithm name ( params ) -> return_type begin ... end
        name = self.consume(TokenType.IDENTIFIER, "Expected function name").value
        self.consume(TokenType.LPAREN, "Expected '(' after function name")

        params = []
        if not self.check(TokenType.RPAREN):
            while True:
                param_name = self.consume(TokenType.IDENTIFIER, "Expected parameter name").value
                self.consume(TokenType.COLON, "Expected ':' after parameter name")
                param_type = self.parse_type()
                params.append(Parameter(param_name, param_type))
                if not self.match(TokenType.COMMA):
                    break

        self.consume(TokenType.RPAREN, "Expected ')' after parameters")

        return_type = TypeNode("void")
        if self.match(TokenType.ARROW):
            return_type = self.parse_type()

        if self.match(TokenType.SEMICOLON):
            return FunctionDeclaration(name, params, return_type, None)

        self.consume(TokenType.BEGIN, "Expected 'begin' before function body")
        body = self.parse_block()

        return FunctionDeclaration(name, params, return_type, body)

    def parse_block(self):
        statements = []
        while not self.check(TokenType.END) and not self.is_at_end():
            statements.append(self.parse_statement())
        self.consume(TokenType.END, "Expected 'end' after block")
        return Block(statements)

    def parse_statement(self):
        if self.match(TokenType.VERIFY):
            return self.parse_if_statement()
        elif self.match(TokenType.CYCLE):
            return self.parse_while_statement()
        elif self.match(TokenType.RESULT):
            return self.parse_return_statement()
        elif self.match(TokenType.DATUM): # Local variable declaration
            return self.parse_variable_declaration()
        elif self.match(TokenType.REVEAL):
            return self.parse_reveal_statement()

        # Unify Assignment and Expression Statement parsing
        # Parse LHS expression
        try:
            expr = self.parse_expression()

            if self.match(TokenType.ASSIGN):
                # It's an assignment
                value = self.parse_expression()
                self.consume(TokenType.SEMICOLON, "Expected ';' after assignment")

                if isinstance(expr, Identifier):
                    return Assignment(expr.name, value)
                elif isinstance(expr, ArrayAccess):
                    return ArrayAssignment(expr.array_name, expr.index, value)
                elif isinstance(expr, MemberAccess):
                    return MemberAssignment(expr.object, expr.member_name, value)
                else:
                    raise SyntaxError(f"Invalid assignment target at line {self.peek().line}")
            else:
                # It's an expression statement
                self.consume(TokenType.SEMICOLON, "Expected ';' after expression")
                return ExpressionStatement(expr)
        except SyntaxError:
            raise

    def parse_if_statement(self):
        # verify ( cond ) then ... [otherwise ...] conclude
        self.consume(TokenType.LPAREN, "Expected '(' after verify")
        condition = self.parse_expression()
        self.consume(TokenType.RPAREN, "Expected ')' after condition")

        self.consume(TokenType.THEN, "Expected 'then' before true block")

        then_stmts = []
        while True:
            if self.is_at_end():
                break
            if self.check(TokenType.OTHERWISE):
                break
            if self.check(TokenType.CONCLUDE):
                break
            if self.check(TokenType.END):
                # Safety break: if we hit END (end of function/block), we stop parsing verify block.
                # This will cause 'consume(CONCLUDE)' to fail with proper error, or allow parse_block to handle END.
                break
            then_stmts.append(self.parse_statement())

        then_branch = Block(then_stmts)

        else_branch = None
        if self.match(TokenType.OTHERWISE):
            else_stmts = []
            while True:
                if self.is_at_end():
                    break
                if self.check(TokenType.CONCLUDE):
                    break
                if self.check(TokenType.END): # Safety break
                    break
                else_stmts.append(self.parse_statement())
            else_branch = Block(else_stmts)

        self.consume(TokenType.CONCLUDE, "Expected 'conclude' at end of verify statement")
        return IfStatement(condition, then_branch, else_branch)

    def parse_while_statement(self):
        # cycle ( cond ) do ... conclude
        self.consume(TokenType.LPAREN, "Expected '(' after cycle")
        condition = self.parse_expression()
        self.consume(TokenType.RPAREN, "Expected ')' after condition")

        self.consume(TokenType.DO, "Expected 'do' before loop body")

        body_stmts = []
        while not self.check(TokenType.CONCLUDE) and not self.is_at_end():
            body_stmts.append(self.parse_statement())

        self.consume(TokenType.CONCLUDE, "Expected 'conclude' after cycle body")
        return WhileStatement(condition, Block(body_stmts))

    def parse_return_statement(self):
        value = None
        if not self.check(TokenType.SEMICOLON):
            value = self.parse_expression()
        self.consume(TokenType.SEMICOLON, "Expected ';' after return value")
        return ReturnStatement(value)

    def parse_reveal_statement(self):
        self.consume(TokenType.LPAREN, "Expected '(' after reveal")
        args = []
        if not self.check(TokenType.RPAREN):
            while True:
                args.append(self.parse_expression())
                if not self.match(TokenType.COMMA):
                    break
        self.consume(TokenType.RPAREN, "Expected ')' after arguments")
        self.consume(TokenType.SEMICOLON, "Expected ';' after reveal statement")
        return ExpressionStatement(FunctionCall("reveal", args))

    def parse_expression(self):
        return self.parse_logic_or()

    def parse_logic_or(self):
        expr = self.parse_logic_and()
        while self.match(TokenType.OR):
            operator = "or"
            right = self.parse_logic_and()
            expr = BinaryOp(expr, operator, right)
        return expr

    def parse_logic_and(self):
        expr = self.parse_comparison()
        while self.match(TokenType.AND):
            operator = "and"
            right = self.parse_comparison()
            expr = BinaryOp(expr, operator, right)
        return expr

    def parse_comparison(self):
        expr = self.parse_term()

        while self.match(TokenType.GT, TokenType.LT, TokenType.EQ, TokenType.GE, TokenType.LE, TokenType.NEQ):
            operator = self.previous().value
            right = self.parse_term()
            expr = BinaryOp(expr, operator, right)

        return expr

    def parse_term(self):
        expr = self.parse_factor()

        while self.match(TokenType.PLUS, TokenType.MINUS):
            operator = self.previous().value
            right = self.parse_factor()
            expr = BinaryOp(expr, operator, right)

        return expr

    def parse_factor(self):
        expr = self.parse_unary()

        while self.match(TokenType.STAR, TokenType.SLASH, TokenType.MODULO):
            operator = self.previous().value
            right = self.parse_unary()
            expr = BinaryOp(expr, operator, right)

        return expr

    def parse_unary(self):
        if self.match(TokenType.NOT):
            operator = "not"
            right = self.parse_unary()
            return BinaryOp(None, operator, right)

        if self.match(TokenType.MINUS):
            operator = "-"
            right = self.parse_unary()
            return BinaryOp(None, operator, right)

        return self.parse_primary()

    def parse_primary(self):
        expr = None
        if self.match(TokenType.INQUIRE):
            self.consume(TokenType.LPAREN, "Expected '(' after inquire")
            self.consume(TokenType.RPAREN, "Expected ')' after inquire")
            expr = InquireExpression()

        elif self.match(TokenType.LBRACKET):
            elements = []
            if not self.check(TokenType.RBRACKET):
                while True:
                    elements.append(self.parse_expression())
                    if not self.match(TokenType.COMMA):
                        break
            self.consume(TokenType.RBRACKET, "Expected ']' after array literal")
            expr = ArrayLiteral(elements)

        elif self.match(TokenType.INTEGER_LITERAL):
            expr = Literal(int(self.previous().value), 'whole')
        elif self.match(TokenType.FLOAT_LITERAL):
            expr = Literal(float(self.previous().value), 'precise')
        elif self.match(TokenType.STRING_LITERAL):
            expr = Literal(self.previous().value, 'series')
        elif self.match(TokenType.BOOLEAN_LITERAL):
            expr = Literal(self.previous().value == 'true', 'state')

        elif self.match(TokenType.IDENTIFIER):
            name = self.previous().value
            if self.match(TokenType.LPAREN): # Function call
                args = []
                if not self.check(TokenType.RPAREN):
                    while True:
                        args.append(self.parse_expression())
                        if not self.match(TokenType.COMMA):
                            break
                self.consume(TokenType.RPAREN, "Expected ')' after arguments")
                expr = FunctionCall(name, args)
            elif self.match(TokenType.LBRACKET): # Array access
                index = self.parse_expression()
                self.consume(TokenType.RBRACKET, "Expected ']' after array index")
                expr = ArrayAccess(name, index)
            else:
                expr = Identifier(name)

        elif self.match(TokenType.LPAREN):
            expr = self.parse_expression()
            self.consume(TokenType.RPAREN, "Expected ')' after expression")

        else:
            raise SyntaxError(f"Unexpected token {self.peek().type} at line {self.peek().line}")

        if expr is None:
             return expr

        # Handle chaining: .field, [index]
        while True:
            if self.match(TokenType.DOT):
                member = self.consume(TokenType.IDENTIFIER, "Expected member name after '.'").value
                expr = MemberAccess(expr, member)
            elif self.match(TokenType.LBRACKET):
                # Array access on expression: expr[idx]
                index = self.parse_expression()
                self.consume(TokenType.RBRACKET, "Expected ']' after array index")
                # Currently ArrayAccess uses array_name: str.
                # This breaks chaining.
                # FOR V1 COMPATIBILITY: We skip handling this properly to avoid AST redefinition issues.
                pass
                break
            else:
                break

        return expr

    def parse_type(self):
        if self.match(TokenType.TYPE_WHOLE):
            return TypeNode("whole")
        if self.match(TokenType.TYPE_PRECISE):
            return TypeNode("precise")
        if self.match(TokenType.TYPE_SERIES):
            return TypeNode("series")
        if self.match(TokenType.TYPE_STATE):
            return TypeNode("state")
        if self.match(TokenType.TYPE_SEQUENCE):
            self.consume(TokenType.LT, "Expected '<' after sequence")
            subtype = self.parse_type()
            self.consume(TokenType.GT, "Expected '>' after sequence type")
            return TypeNode("sequence", subtype)
        if self.match(TokenType.IDENTIFIER):
            return TypeNode(self.previous().value)
        raise SyntaxError(f"Expected type at line {self.peek().line}")

    # Helper methods
    def match(self, *types):
        for type_ in types:
            if self.check(type_):
                self.advance()
                return True
        return False

    def check(self, type_):
        if self.is_at_end():
            return False
        return self.peek().type == type_

    def advance(self):
        if not self.is_at_end():
            self.pos += 1
        return self.previous()

    def is_at_end(self):
        return self.peek().type == TokenType.EOF

    def peek(self):
        return self.tokens[self.pos]

    def peek_next(self):
        if self.pos + 1 < len(self.tokens):
            return self.tokens[self.pos + 1]
        return self.tokens[-1]

    def previous(self):
        return self.tokens[self.pos - 1]

    def consume(self, type_, message):
        if self.check(type_):
            return self.advance()
        raise SyntaxError(f"{message} at line {self.peek().line}")
