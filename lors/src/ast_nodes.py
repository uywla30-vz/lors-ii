from dataclasses import dataclass
from typing import List, Optional, Union
from enum import Enum

# Base class for all AST nodes
class ASTNode:
    pass

@dataclass
class Program(ASTNode):
    declarations: List[ASTNode] # Can be variable or function declarations

@dataclass
class TypeNode(ASTNode):
    name: str
    subtype: Optional['TypeNode'] = None # For arrays

@dataclass
class VariableDeclaration(ASTNode):
    name: str
    var_type: TypeNode
    initializer: ASTNode

@dataclass
class StructDeclaration(ASTNode):
    name: str
    fields: List[VariableDeclaration]

@dataclass
class ArrayLiteral(ASTNode):
    elements: List[ASTNode]

@dataclass
class ArrayAccess(ASTNode):
    array_name: str
    index: ASTNode

@dataclass
class MemberAccess(ASTNode):
    object: ASTNode
    member_name: str

@dataclass
class Block(ASTNode):
    statements: List[ASTNode]

@dataclass
class FunctionDeclaration(ASTNode):
    name: str
    params: List['Parameter']
    return_type: TypeNode
    body: Optional[Block] # Changed to Optional for forward declarations

@dataclass
class Parameter(ASTNode):
    name: str
    param_type: TypeNode

@dataclass
class IfStatement(ASTNode):
    condition: ASTNode
    then_branch: Block
    else_branch: Optional[Block]

@dataclass
class WhileStatement(ASTNode):
    condition: ASTNode
    body: Block

@dataclass
class ReturnStatement(ASTNode):
    value: ASTNode

@dataclass
class ExpressionStatement(ASTNode):
    expression: ASTNode

@dataclass
class Assignment(ASTNode):
    name: str
    value: ASTNode

@dataclass
class ArrayAssignment(ASTNode):
    name: str
    index: ASTNode
    value: ASTNode

@dataclass
class MemberAssignment(ASTNode):
    object: ASTNode
    member_name: str
    value: ASTNode

# Expressions
@dataclass
class BinaryOp(ASTNode):
    left: ASTNode
    operator: str
    right: ASTNode

@dataclass
class Literal(ASTNode):
    value: Union[int, float, str, bool]
    value_type: str # 'whole', 'precise', 'series', 'state'

@dataclass
class Identifier(ASTNode):
    name: str

@dataclass
class FunctionCall(ASTNode):
    name: str
    arguments: List[ASTNode]

@dataclass
class InquireExpression(ASTNode):
    pass
