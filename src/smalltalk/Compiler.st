"Compiler — Wires tokenizer, parser, and code generator"

Object subclass: #Compiler instanceVariableNames: ''

!Compiler class methodsFor: 'compiling'!
compileExpression: aString
    ^ (CodeGenerator new) compileExpression: aString
!

compileMethod: aString
    ^ (CodeGenerator new) compileMethod: aString
!
