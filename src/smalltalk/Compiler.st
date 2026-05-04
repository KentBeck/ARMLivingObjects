"Compiler — Wires tokenizer, parser, and code generator"

Object subclass: #Compiler instanceVariableNames: ''

!Compiler class methodsFor: 'compiling'!
compileExpression: aString
    ^ (CodeGenerator new) compileExpression: aString
!

compileMethod: aString
    ^ self compileMethod: aString forClass: nil
!

compileMethod: aString forClass: aClass
    | generator ivars |
    generator := CodeGenerator new.
    aClass == nil
        ifTrue: [0]
        ifFalse: [
            ivars := aClass instanceVariableNames.
            ivars == nil
                ifTrue: [generator instanceVariableNames: (Array new: 0)]
                ifFalse: [generator instanceVariableNames: ivars]
        ].
    ^ generator compileMethod: aString
!
