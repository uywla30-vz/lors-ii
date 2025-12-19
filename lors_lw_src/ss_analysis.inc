// ss_analysis.inc - AST Analysis for SSA Construction

algorithm analysis_scan_assignments(node_idx : whole, target_list : sequence<series>) -> sequence<series>
begin
    verify (node_idx == -1) then result target_list; conclude

    datum node : ASTNode = node_heap[node_idx];
    datum k : whole = node.kind;

    verify (k == NODE_ASSIGN) then
        datum name : series = node.text_val;
        // Add if not present
        datum i : whole = 0;
        datum found : state = false;
        cycle (i < length(target_list)) do
            verify (target_list[i] == name) then found = true; conclude
            i = i + 1;
        conclude
        verify (not found) then append(target_list, name); conclude
    conclude

    // Recurse
    datum children : sequence<whole> = node.children;
    datum j : whole = 0;
    cycle (j < length(children)) do
        target_list = analysis_scan_assignments(children[j], target_list);
        j = j + 1;
    conclude

    // Specific branches
    verify (k == NODE_IF) then
        target_list = analysis_scan_assignments(node.then_branch, target_list);
        target_list = analysis_scan_assignments(node.else_branch, target_list);
    conclude

    verify (k == NODE_WHILE) then
        target_list = analysis_scan_assignments(node.then_branch, target_list);
    conclude

    verify (k == NODE_BLOCK) then
        // Children handled above? No, block children are in .children
        // Wait, AST structure says Block has children.
        // And Loop/If have branch fields.
        // Does generic recursion handle it?
        // If If/While nodes put branches in .children?
        // AST doc says: then_branch, else_branch.
        // So I handled them.
        // But Block puts stmts in children. So generic recursion handles block.
    conclude

    result target_list;
end
