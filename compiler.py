import sys
import os
import subprocess
from lors.src.lexer import Lexer
from lors.src.parser import Parser
from lors.src.codegen import CodeGenerator

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 compiler <script>.lr")
        sys.exit(1)

    input_file = sys.argv[1]
    if not input_file.endswith(".lr"):
        print("Error: Input file must have .lr extension")
        sys.exit(1)

    if not os.path.exists(input_file):
        print(f"Error: File '{input_file}' not found")
        sys.exit(1)

    # 1. Read Source
    try:
        # Get absolute path of input file to resolve includes correctly relative to it
        abs_input_path = os.path.abspath(input_file)
        input_dir = os.path.dirname(abs_input_path)

        with open(abs_input_path, 'r') as f:
            source_code = f.read()

        # Preprocessor: Handle incorporate
        def process_includes(code, base_dir):
            lines = code.split('\n')
            processed_lines = []
            for line in lines:
                if line.strip().startswith("incorporate"):
                    # Format: incorporate "file.inc"
                    try:
                        # Extract filename
                        parts = line.strip().split('"')
                        if len(parts) >= 2:
                            inc_path = parts[1]

                            # Search strategy:
                            # 1. Relative to the file currently being processed (base_dir)
                            # 2. Relative to CWD (fallback)

                            candidates = [
                                os.path.join(base_dir, inc_path),
                                inc_path
                            ]

                            found_content = None
                            found_path = None

                            for candidate in candidates:
                                if os.path.exists(candidate):
                                    found_path = os.path.abspath(candidate)
                                    with open(candidate, 'r') as inc_f:
                                        found_content = inc_f.read()
                                    break

                            if found_content is not None:
                                # Recursively process the included content,
                                # using its directory as the new base_dir
                                new_base = os.path.dirname(found_path)
                                processed_lines.append(process_includes(found_content, new_base))
                            else:
                                # Error
                                print(f"Error: Could not find included file: '{inc_path}'")
                                print(f"  Searched in: {base_dir}")
                                print(f"  And CWD: {os.getcwd()}")
                                sys.exit(1)
                        else:
                            # Malformed incorporate? Keep as is or error.
                            processed_lines.append(line)
                    except Exception as e:
                        print(f"Preprocessor Error: {e}")
                        sys.exit(1)
                else:
                    processed_lines.append(line)
            return '\n'.join(processed_lines)

        source_code = process_includes(source_code, input_dir)

    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

    # 2. Compile (Lex -> Parse -> CodeGen)
    try:
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()

        parser = Parser(tokens)
        ast = parser.parse()

        codegen = CodeGenerator()
        cpp_code = codegen.generate(ast)

    except Exception as e:
        # Print without stack trace for cleaner user output if it's a syntax error
        # but with stack trace if it's a bug
        if isinstance(e, SyntaxError):
            print(f"Compilation Error: {e}")
        else:
            import traceback
            traceback.print_exc()
            print(f"Internal Compiler Error: {e}")
        sys.exit(1)

    # 3. Write C++ Output
    base_name = os.path.splitext(input_file)[0]
    cpp_file = f"{base_name}.cpp"

    with open(cpp_file, 'w') as f:
        f.write(cpp_code)

    # 4. Invoke g++
    output_bin = base_name

    cmd = ["g++", cpp_file, "-o", output_bin]

    # print(f"Compiling {input_file} -> {output_bin}...")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"C++ Backend Failed for {input_file}:")
        print(result.stderr)
        sys.exit(1)

    # Clean up
    if os.path.exists(cpp_file):
        os.remove(cpp_file)

if __name__ == "__main__":
    main()
