// AST Node Kinds
datum NODE_PROGRAM : whole = 1;
datum NODE_VAR_DECL : whole = 2;
datum NODE_FUNC_DECL : whole = 3;
datum NODE_STRUCT_DECL : whole = 4;
datum NODE_BLOCK : whole = 5;
datum NODE_IF : whole = 6;
datum NODE_WHILE : whole = 7;
datum NODE_RETURN : whole = 8;
datum NODE_EXPR_STMT : whole = 9;
datum NODE_ASSIGN : whole = 10;
datum NODE_ARRAY_ASSIGN : whole = 11;
datum NODE_MEMBER_ASSIGN : whole = 12;

datum NODE_BINARY_OP : whole = 20;
datum NODE_LITERAL : whole = 21;
datum NODE_IDENTIFIER : whole = 22;
datum NODE_FUNC_CALL : whole = 23;
datum NODE_ARRAY_LITERAL : whole = 24;
datum NODE_ARRAY_ACCESS : whole = 25;
datum NODE_MEMBER_ACCESS : whole = 26;
datum NODE_INQUIRE : whole = 27;

datum NODE_TYPE : whole = 30;
datum NODE_PARAM : whole = 31;

structure ASTNode begin
    datum kind : whole = 0;

    // For Identifier, Literal (series), FuncCall name, Struct name, etc.
    datum text_val : series = "";

    // For Literals
    datum int_val : whole = 0;
    datum float_val : precise = 0.0;
    datum bool_val : state = false;
    datum value_type : series = ""; // "whole", "precise" etc.

    // BinaryOp, Assignment
    datum left : whole = -1;
    datum right : whole = -1;
    datum op : series = ""; // Renamed from operator

    // If, While
    datum condition : whole = -1;
    datum then_branch : whole = -1; // Also used for Block in FuncDecl
    datum else_branch : whole = -1;

    // Declarations
    datum type_node : whole = -1; // Var type, Return type
    datum initializer : whole = -1;

    // TypeNode
    datum subtype : whole = -1;

    // Children list (Statements in Block, Params in Func, Fields in Struct, Args in Call, Elements in Array)
    datum children : sequence<whole>;
end

// Global Heap for AST Nodes
datum node_heap : sequence<ASTNode>;

algorithm create_node(kind : whole) -> whole
begin
    datum idx : whole = length(node_heap);
    // Initialize a new node
    datum node : ASTNode;
    node.kind = kind;
    append(node_heap, node);
    result idx;
end
