"ASTNodes — Abstract Syntax Tree node classes for Smalltalk parser"

Object subclass: #ASTNode instanceVariableNames: ''
ASTNode subclass: #LiteralNode instanceVariableNames: 'value token'
ASTNode subclass: #LiteralArrayNode instanceVariableNames: 'elements'
ASTNode subclass: #VariableNode instanceVariableNames: 'name'
ASTNode subclass: #MessageNode instanceVariableNames: 'receiver selector arguments'
ASTNode subclass: #CascadeNode instanceVariableNames: 'receiver messages'
ASTNode subclass: #BlockNode instanceVariableNames: 'arguments temporaries statements'
ASTNode subclass: #AssignmentNode instanceVariableNames: 'variable value'
ASTNode subclass: #ReturnNode instanceVariableNames: 'expression'
ASTNode subclass: #SequenceNode instanceVariableNames: 'temporaries statements'
ASTNode subclass: #MethodNode instanceVariableNames: 'selector arguments body'

!ASTNode methodsFor: 'testing'!
isLiteral
    ^ false
!

isVariable
    ^ false
!

isMessage
    ^ false
!

isCascade
    ^ false
!

isBlock
    ^ false
!

isAssignment
    ^ false
!

isReturn
    ^ false
!

isSequence
    ^ false
!

isMethod
    ^ false
!

isLiteralArray
    ^ false
!


!LiteralNode class methodsFor: 'instance creation'!
value: anObject
    ^ self new initializeValue: anObject token: nil
!

value: anObject token: aToken
    ^ self new initializeValue: anObject token: aToken
!

!LiteralNode methodsFor: 'initialization'!
initializeValue: anObject token: aToken
    value := anObject.
    token := aToken.
    ^ self
!

!LiteralNode methodsFor: 'accessing'!
value
    ^ value
!

token
    ^ token
!

!LiteralNode methodsFor: 'testing'!
isLiteral
    ^ true
!


!LiteralArrayNode class methodsFor: 'instance creation'!
elements: anArray
    ^ self new initializeElements: anArray
!

!LiteralArrayNode methodsFor: 'initialization'!
initializeElements: anArray
    elements := anArray.
    ^ self
!

!LiteralArrayNode methodsFor: 'accessing'!
elements
    ^ elements
!

!LiteralArrayNode methodsFor: 'testing'!
isLiteralArray
    ^ true
!

isLiteral
    ^ true
!


!VariableNode class methodsFor: 'instance creation'!
name: aString
    ^ self new initializeName: aString
!

!VariableNode methodsFor: 'initialization'!
initializeName: aString
    name := aString.
    ^ self
!

!VariableNode methodsFor: 'accessing'!
name
    ^ name
!

!VariableNode methodsFor: 'testing'!
isVariable
    ^ true
!

isSelf
    ^ name = 'self'
!

isSuper
    ^ name = 'super'
!

isThisContext
    ^ name = 'thisContext'
!

isNil
    ^ name = 'nil'
!

isTrue
    ^ name = 'true'
!

isFalse
    ^ name = 'false'
!

isPseudoVariable
    self isSelf ifTrue: [^ true] ifFalse: [0].
    self isSuper ifTrue: [^ true] ifFalse: [0].
    self isThisContext ifTrue: [^ true] ifFalse: [0].
    self isNil ifTrue: [^ true] ifFalse: [0].
    self isTrue ifTrue: [^ true] ifFalse: [0].
    ^ self isFalse
!


!MessageNode class methodsFor: 'instance creation'!
receiver: rcvr selector: sel arguments: args
    ^ self new initializeReceiver: rcvr selector: sel arguments: args
!

receiver: rcvr selector: sel
    ^ self receiver: rcvr selector: sel arguments: (Array new: 0)
!

!MessageNode methodsFor: 'initialization'!
initializeReceiver: rcvr selector: sel arguments: args
    receiver := rcvr.
    selector := sel.
    arguments := args.
    ^ self
!

!MessageNode methodsFor: 'accessing'!
receiver
    ^ receiver
!

selector
    ^ selector
!

arguments
    ^ arguments
!

!MessageNode methodsFor: 'testing'!
isMessage
    ^ true
!

isUnary
    ^ arguments size = 0
!

isBinary
    ^ arguments size = 1
!

isKeyword
    ^ arguments size > 1
!


!CascadeNode class methodsFor: 'instance creation'!
receiver: rcvr messages: msgs
    ^ self new initializeReceiver: rcvr messages: msgs
!

!CascadeNode methodsFor: 'initialization'!
initializeReceiver: rcvr messages: msgs
    receiver := rcvr.
    messages := msgs.
    ^ self
!

!CascadeNode methodsFor: 'accessing'!
receiver
    ^ receiver
!

messages
    ^ messages
!

!CascadeNode methodsFor: 'testing'!
isCascade
    ^ true
!


!BlockNode class methodsFor: 'instance creation'!
arguments: args temporaries: temps statements: stmts
    ^ self new initializeArguments: args temporaries: temps statements: stmts
!

statements: stmts
    ^ self arguments: (Array new: 0) temporaries: (Array new: 0) statements: stmts
!

!BlockNode methodsFor: 'initialization'!
initializeArguments: args temporaries: temps statements: stmts
    arguments := args.
    temporaries := temps.
    statements := stmts.
    ^ self
!

!BlockNode methodsFor: 'accessing'!
arguments
    ^ arguments
!

temporaries
    ^ temporaries
!

statements
    ^ statements
!

!BlockNode methodsFor: 'testing'!
isBlock
    ^ true
!


!AssignmentNode class methodsFor: 'instance creation'!
variable: aVariableNode value: anExpression
    ^ self new initializeVariable: aVariableNode value: anExpression
!

!AssignmentNode methodsFor: 'initialization'!
initializeVariable: aVariableNode value: anExpression
    variable := aVariableNode.
    value := anExpression.
    ^ self
!

!AssignmentNode methodsFor: 'accessing'!
variable
    ^ variable
!

value
    ^ value
!

!AssignmentNode methodsFor: 'testing'!
isAssignment
    ^ true
!


!ReturnNode class methodsFor: 'instance creation'!
expression: anExpression
    ^ self new initializeExpression: anExpression
!

!ReturnNode methodsFor: 'initialization'!
initializeExpression: anExpression
    expression := anExpression.
    ^ self
!

!ReturnNode methodsFor: 'accessing'!
expression
    ^ expression
!

!ReturnNode methodsFor: 'testing'!
isReturn
    ^ true
!


!SequenceNode class methodsFor: 'instance creation'!
temporaries: temps statements: stmts
    ^ self new initializeTemporaries: temps statements: stmts
!

statements: stmts
    ^ self temporaries: (Array new: 0) statements: stmts
!

!SequenceNode methodsFor: 'initialization'!
initializeTemporaries: temps statements: stmts
    temporaries := temps.
    statements := stmts.
    ^ self
!

!SequenceNode methodsFor: 'accessing'!
temporaries
    ^ temporaries
!

statements
    ^ statements
!

!SequenceNode methodsFor: 'testing'!
isSequence
    ^ true
!


!MethodNode class methodsFor: 'instance creation'!
selector: sel arguments: args body: aSequenceNode
    ^ self new initializeSelector: sel arguments: args body: aSequenceNode
!

!MethodNode methodsFor: 'initialization'!
initializeSelector: sel arguments: args body: aSequenceNode
    selector := sel.
    arguments := args.
    body := aSequenceNode.
    ^ self
!

!MethodNode methodsFor: 'accessing'!
selector
    ^ selector
!

arguments
    ^ arguments
!

body
    ^ body
!

temporaries
    ^ body temporaries
!

statements
    ^ body statements
!

!MethodNode methodsFor: 'testing'!
isMethod
    ^ true
!

isUnary
    ^ arguments size = 0
!

isBinary
    ^ arguments size = 1
!

isKeyword
    ^ arguments size > 1
!
