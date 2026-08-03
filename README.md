# Mid C++ Compiler (mcppc)

A *very very* early WIP C++ compiler written in C23.

I'm currently planning on only supporting C++11 because I would like to finish
this project before I die.

Also worth mentioning this codebase has absolutely no AI written code because
I would never rely on a clanker to write my code for me.

## Language Support

While I'm planning on fully supporting C++11, right now the compiler only
partially / fully supports:

* Data Types
    * Every Built-In Data Type
    * Every Built-In Literal Type
    * Function Pointers
    * auto
    * typedef
    * LValue and RValue References
* Variables
    * Every Allowed Type (even wacky wonky ones like `void (*const *)(int (*)(), ...)`)
    * Declarator Lists (meaning: `int x, *y, *const *z;`)
    * Constructors
* Functions
    * Argument Dependent Lookup
    * Overload Resolution
    * Operator Overloading
    * Default Parameters
    * Variadic Functions
    * Forward Declaration
* Classes
    * Static and Non-Static Member Functions
    * Constructors
    * Destructors
    * public, private and protected
    * 2-Pass Parsing
    * Forward Declaration
    * Trailing Declarator Lists (meaing: `class A { .. } x, *y, *const *z`)
* Namespaces

### C Support

I might eventually add support for parsing C as well but right now the focus
is on C++.

# Building

This compiler has no external dependencies so if you have a C23 compatible
compiler you should be able to run the following in the project directory to
build the mcppc binary:

```
cmake .
make
```

The binary should then be in the "bin/" folder:

```
bin/mcppc --help
```

Note that I have only tested this codebase with CLang 20.1.8 on Linux Mint,
but it should be completely cross platform.

# Project Structure

* src/lexer/          - Stuff related to the lexing pass
* src/parser/         - Stuff related to the AST construction pass
* src/sema/           - Stuff related to semantic analysis (eg. typechecking, overload resolution, etc.)
* src/generics/       - Generic data structures
* tests/              - C++ test programs go here

The include directory mirrors the src directory.

# Modules

There are several modules within the compiler for various tasks.
Here's a brief overfiew of all of them:

mid_*           - Top level prefix, everything not in its own module goes in
                  here.
midflt_*        - Stuff related to mid_APFloat
midint_*        - Stuff related to mid_APInt
midllvm_*       - Stuff related to LLVM, like generating LLVM IR for example
midcmd_*        - Stuff related to the command line, like parsing command line
                  arguments for example
middiag_*       - Stuff related to compiler diagnostics
midstr_*        - Stuff related to mid_Dynstr
midgen_*        - Generic data types
midlex_*        - The lexer
midlit_*        - Stuff related to literals
midpar_*        - The parser
midsema_*       - Semantic Analysis
midsymb_*       - The symbol table
midtype_*       - Info about C++ data types
midutf8_*       - UTF8 stuff
