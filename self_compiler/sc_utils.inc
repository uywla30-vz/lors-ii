// String Utilities

algorithm is_space(c : whole) -> state
begin
    // Space, Tab, Newline, CR
    verify (c == 32 or c == 9 or c == 10 or c == 13) then
        result true;
    otherwise
        result false;
    conclude
end

algorithm is_digit(c : whole) -> state
begin
    verify (c >= 48 and c <= 57) then
        result true;
    otherwise
        result false;
    conclude
end

algorithm is_alpha(c : whole) -> state
begin
    // Check ranges explicitamente para evitar ambiguedades de precedencia
    // A-Z (65-90)
    verify (c >= 65 and c <= 90) then result true; conclude
    // a-z (97-122)
    verify (c >= 97 and c <= 122) then result true; conclude
    // _ (95)
    verify (c == 95) then result true; conclude

    result false;
end

algorithm is_alnum(c : whole) -> state
begin
    // Combinar is_alpha y is_digit directamente
    verify (is_alpha(c)) then result true; conclude
    verify (is_digit(c)) then result true; conclude
    result false;
end

// Check if string starts with prefix
algorithm starts_with(s : series, prefix : series) -> state
begin
    datum s_len : whole = length(s);
    datum p_len : whole = length(prefix);
    verify (s_len < p_len) then
        result false;
    conclude

    datum sub : series = substring(s, 0, p_len);
    verify (sub == prefix) then
        result true;
    otherwise
        result false;
    conclude
end

// Helper to get directory name
algorithm get_dirname(path : series) -> series
begin
    datum len : whole = length(path);
    datum i : whole = len - 1;
    cycle (i >= 0) do
        datum c : whole = char_at(path, i);
        verify (c == 47) then // '/'
            result substring(path, 0, i);
        conclude
        i = i - 1;
    conclude
    result ".";
end

// Join paths
algorithm path_join(dir : series, file : series) -> series
begin
    verify (dir == ".") then
        result file;
    conclude
    // Check if dir ends with /
    datum dir_len : whole = length(dir);
    verify (dir_len > 0) then
        datum last : whole = char_at(dir, dir_len - 1);
        verify (last == 47) then
            result dir + file;
        otherwise
            result dir + "/" + file;
        conclude
    conclude
    result file;
end
