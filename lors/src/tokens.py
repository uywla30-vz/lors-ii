from enum import Enum, auto
from dataclasses import dataclass

class TokenType(Enum):
    # Keywords
    DATUM = auto()      # datum
    VERIFY = auto()     # verify
    THEN = auto()       # then
    OTHERWISE = auto()  # otherwise
    CONCLUDE = auto()   # conclude
    CYCLE = auto()      # cycle
    DO = auto()         # do
    ALGORITHM = auto()  # algorithm
    BEGIN = auto()      # begin
    END = auto()        # end
    RESULT = auto()     # result
    REVEAL = auto()     # reveal
    INQUIRE = auto()    # inquire
    INCORPORATE = auto() # incorporate
    STRUCTURE = auto()  # structure
    AND = auto()        # and
    OR = auto()         # or
    NOT = auto()        # not

    # Types
    TYPE_WHOLE = auto()    # whole
    TYPE_PRECISE = auto()  # precise
    TYPE_SERIES = auto()   # series
    TYPE_STATE = auto()    # state
    TYPE_SEQUENCE = auto() # sequence

    # Literals
    INTEGER_LITERAL = auto()
    FLOAT_LITERAL = auto()
    STRING_LITERAL = auto()
    BOOLEAN_LITERAL = auto() # true/false

    # Identifiers
    IDENTIFIER = auto()

    # Operators and Punctuation
    PLUS = auto()       # +
    MINUS = auto()      # -
    STAR = auto()       # *
    SLASH = auto()      # /
    MODULO = auto()     # %
    ASSIGN = auto()     # =
    DOT = auto()        # .
    COLON = auto()      # :
    SEMICOLON = auto()  # ;
    COMMA = auto()      # ,
    LPAREN = auto()     # (
    RPAREN = auto()     # )
    LBRACKET = auto()   # [
    RBRACKET = auto()   # ]
    GT = auto()         # >
    LT = auto()         # <
    EQ = auto()         # ==
    GE = auto()         # >=
    LE = auto()         # <=
    NEQ = auto()        # !=
    ARROW = auto()      # ->

    EOF = auto()

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    column: int
