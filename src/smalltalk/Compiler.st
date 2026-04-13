"Compiler — Wires tokenizer, parser, and code generator"

!Compiler class methodsFor: 'compiling'!
compileExpression: aString
    | gen |
    gen := CodeGenerator new.
    gen compileExpression: aString.
    ^ gen
!

compileMethod: aString
    | gen |
    gen := CodeGenerator new.
    gen compileMethod: aString.
    ^ gen
!
