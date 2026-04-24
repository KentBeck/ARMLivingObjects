"CodeGenerator — Emits bytecodes from AST nodes"
"Instance variables: bytecodes, bytecodeCount, literals, literalCount, temps, tempCount, args, argCount, instVarNames, instVarCount, inBlock"

Object subclass: #CodeGenerator instanceVariableNames: 'bytecodes bytecodeCount literals literalCount temps tempCount args argCount instVarNames instVarCount inBlock'

!CodeGenerator class methodsFor: 'instance creation'!
new
    ^ self basicNew initialize
!

forBlock
    ^ self basicNew initializeForBlock
!

!CodeGenerator methodsFor: 'initialization'!
initialize
    bytecodes := String new: 256.
    bytecodeCount := 0.
    literals := Array new: 16.
    literalCount := 0.
    temps := Array new: 16.
    tempCount := 0.
    args := Array new: 8.
    argCount := 0.
    instVarNames := Array new: 16.
    instVarCount := 0.
    inBlock := false.
    ^ self
!

initializeForBlock
    self initialize.
    inBlock := true.
    ^ self
!

!CodeGenerator methodsFor: 'accessing'!
bytecodeCount
    ^ bytecodeCount
!

literalCount
    ^ literalCount
!

tempCount
    ^ tempCount
!

bytecodeAt: index
    ^ bytecodes at: index
!

literalAt: index
    ^ literals at: index
!

!CodeGenerator methodsFor: 'configuration'!
arguments: argNames
    args := argNames.
    argCount := argNames size.
    ^ self
!

temporaries: tempNames
    temps := tempNames.
    tempCount := tempNames size.
    ^ self
!

!CodeGenerator methodsFor: 'emitting'!
emitByte: value
    bytecodeCount := bytecodeCount + 1.
    bytecodes at: bytecodeCount put: value.
    ^ self
!

emitU32: value
    self emitByte: value.
    self emitByte: 0.
    self emitByte: 0.
    self emitByte: 0.
    ^ self
!

patchU32At: index with: value
    bytecodes at: index put: value.
    bytecodes at: index + 1 put: 0.
    bytecodes at: index + 2 put: 0.
    bytecodes at: index + 3 put: 0.
    ^ self
!

emitPushLiteral: index
    self emitByte: 0.
    self emitU32: index.
    ^ self
!

emitPushInstVar: index
    self emitByte: 1.
    self emitU32: index.
    ^ self
!

emitPushTemp: index
    self emitByte: 2.
    self emitU32: index.
    ^ self
!

emitPushSelf
    self emitByte: 3.
    ^ self
!

emitStoreInstVar: index
    self emitByte: 4.
    self emitU32: index.
    ^ self
!

emitStoreTemp: index
    self emitByte: 5.
    self emitU32: index.
    ^ self
!

emitSendMessage: selectorIndex argc: argc
    self emitByte: 6.
    self emitU32: selectorIndex.
    self emitU32: argc.
    ^ self
!

emitReturn
    self emitByte: 7.
    ^ self
!

emitJumpTo: offset
    self emitByte: 8.
    self emitU32: offset.
    ^ self
!

emitJumpIfFalsePlaceholder
    | patchIndex |
    self emitByte: 10.
    patchIndex := bytecodeCount + 1.
    self emitU32: 0.
    ^ patchIndex
!

emitPop
    self emitByte: 11.
    ^ self
!

emitDuplicate
    self emitByte: 12.
    ^ self
!

emitPushClosure: blockIndex
    self emitByte: 14.
    self emitU32: blockIndex.
    ^ self
!

emitPushArg: index
    self emitByte: 15.
    self emitU32: index.
    ^ self
!

emitReturnNonLocal
    self emitByte: 16.
    ^ self
!

emitPushThisContext
    self emitByte: 17.
    ^ self
!

emitPushGlobal: index
    self emitByte: 18.
    self emitU32: index.
    ^ self
!

!CodeGenerator methodsFor: 'literals'!
addLiteral: anObject
    ^ self findLiteral: anObject from: 1
!

findLiteral: anObject from: index
    index > literalCount
        ifTrue: [
            literalCount := literalCount + 1.
            literals at: literalCount put: anObject.
            ^ literalCount - 1
        ]
        ifFalse: [
            ((literals at: index) == anObject)
                ifTrue: [^ index - 1]
                ifFalse: [^ self findLiteral: anObject from: index + 1]
        ]
!

addSelectorLiteral: aString
    ^ self addLiteral: aString asSymbol
!

!CodeGenerator methodsFor: 'variable resolution'!
resolveVariable: name
    (name = 'self') ifTrue: [self emitPushSelf. ^ self].
    (name = 'super') ifTrue: [self emitPushSelf. ^ self].
    (name = 'thisContext') ifTrue: [self emitPushThisContext. ^ self].
    (name = 'nil') ifTrue: [self emitPushLiteral: (self addLiteral: nil). ^ self].
    (name = 'true') ifTrue: [self emitPushLiteral: (self addLiteral: true). ^ self].
    (name = 'false') ifTrue: [self emitPushLiteral: (self addLiteral: false). ^ self].
    self resolveNamedVariable: name
!

resolveNamedVariable: name
    | tempIndex argIndex |
    tempIndex := self tempIndexOf: name.
    tempIndex >= 0
        ifTrue: [self emitPushTemp: tempIndex. ^ self]
        ifFalse: [0].
    argIndex := self argIndexOf: name.
    argIndex >= 0
        ifTrue: [self emitPushArg: argIndex. ^ self]
        ifFalse: [0].
    self emitPushSelf
!

tempIndexOf: name
    ^ self searchArray: temps for: name from: 1 max: tempCount
!

argIndexOf: name
    ^ self searchArray: args for: name from: 1 max: argCount
!

searchArray: anArray for: name from: index max: max
    index > max
        ifTrue: [^ 0 - 1]
        ifFalse: [
            ((anArray at: index) = name)
                ifTrue: [^ index - 1]
                ifFalse: [^ self searchArray: anArray for: name from: index + 1 max: max]
        ]
!

storeVariable: name
    | tempIndex |
    tempIndex := self tempIndexOf: name.
    tempIndex >= 0
        ifTrue: [self emitStoreTemp: tempIndex. ^ self]
        ifFalse: [0].
    self emitPushSelf
!

!CodeGenerator methodsFor: 'visiting'!
visitNode: aNode
    aNode isLiteral ifTrue: [^ self visitLiteral: aNode].
    aNode isVariable ifTrue: [^ self visitVariable: aNode].
    aNode isMessage ifTrue: [^ self visitMessage: aNode].
    aNode isBlock ifTrue: [^ self visitBlock: aNode].
    aNode isAssignment ifTrue: [^ self visitAssignment: aNode].
    aNode isReturn ifTrue: [^ self visitReturn: aNode].
    aNode isSequence ifTrue: [^ self visitSequence: aNode].
    ^ self
!

visitLiteral: aNode
    | index |
    index := self addLiteral: aNode value.
    self emitPushLiteral: index.
    ^ self
!

visitVariable: aNode
    self resolveVariable: aNode name.
    ^ self
!

visitMessage: aNode
    | selectorIndex |
    (self isWhileTrueMessage: aNode)
        ifTrue: [self visitWhileTrue: aNode]
        ifFalse: [
            self visitNode: aNode receiver.
            aNode arguments size > 0
                ifTrue: [self visitMessageArgs: aNode arguments from: 1]
                ifFalse: [0].
            selectorIndex := self addSelectorLiteral: aNode selector.
            self emitSendMessage: selectorIndex argc: aNode arguments size
        ].
    ^ self
!

isWhileTrueMessage: aNode
    (aNode selector = 'whileTrue:')
        ifTrue: [
            aNode receiver isBlock
                ifTrue: [
                    aNode arguments size = 1
                        ifTrue: [^ (aNode arguments at: 1) isBlock]
                        ifFalse: [^ false]
                ]
                ifFalse: [^ false]
        ]
        ifFalse: [
            (aNode selector = 'whileTrue')
                ifTrue: [
                    aNode receiver isBlock
                        ifTrue: [
                            aNode arguments size = 1
                                ifTrue: [^ (aNode arguments at: 1) isBlock]
                                ifFalse: [^ false]
                        ]
                        ifFalse: [^ false]
                ]
                ifFalse: [^ false]
        ]
!

visitWhileTrue: aNode
    | loopStart exitPatch body |
    loopStart := bytecodeCount.
    self visitInlineBlock: aNode receiver.
    exitPatch := self emitJumpIfFalsePlaceholder.
    body := aNode arguments at: 1.
    self visitInlineBlock: body.
    self emitPop.
    self emitJumpTo: loopStart.
    self patchU32At: exitPatch with: bytecodeCount.
    self emitPushLiteral: (self addLiteral: nil).
    ^ self
!

visitInlineBlock: aBlock
    aBlock statements size = 0
        ifTrue: [self emitPushLiteral: (self addLiteral: nil)]
        ifFalse: [
            self visitNode: (aBlock statements at: 1).
            aBlock statements size > 1
                ifTrue: [
                    self emitPop.
                    self visitStatements: aBlock statements from: 2 last: aBlock statements size
                ]
                ifFalse: [0]
        ].
    ^ self
!

visitMessageArgs: args from: index
    index > args size
        ifTrue: [^ self]
        ifFalse: [
            self visitNode: (args at: index).
            ^ self visitMessageArgs: args from: index + 1
        ]
!

visitBlock: aNode
    | blockGen blockIndex |
    blockGen := CodeGenerator forBlock.
    blockGen arguments: aNode arguments.
    blockGen temporaries: aNode temporaries.
    aNode statements size > 0
        ifTrue: [blockGen visitStatements: aNode statements from: 1 last: aNode statements size]
        ifFalse: [blockGen emitPushLiteral: (blockGen addLiteral: nil)].
    blockGen emitReturn.
    blockIndex := self addLiteral: blockGen.
    self emitPushClosure: blockIndex.
    ^ self
!

visitAssignment: aNode
    self visitNode: aNode value.
    self emitDuplicate.
    self storeVariable: aNode variable name.
    ^ self
!

visitReturn: aNode
    self visitNode: aNode expression.
    inBlock
        ifTrue: [self emitReturnNonLocal]
        ifFalse: [self emitReturn].
    ^ self
!

visitSequence: aNode
    self temporaries: aNode temporaries.
    aNode statements size > 0
        ifTrue: [self visitStatements: aNode statements from: 1 last: aNode statements size]
        ifFalse: [0].
    ^ self
!

visitStatements: stmts from: index last: lastIndex
    | stmt |
    stmt := stmts at: index.
    self visitNode: stmt.
    index = lastIndex
        ifTrue: [^ self]
        ifFalse: [
            self emitPop.
            ^ self visitStatements: stmts from: index + 1 last: lastIndex
        ]
!

!CodeGenerator methodsFor: 'compiling'!
compileExpression: aString
    | parser expr |
    parser := Parser on: aString.
    expr := parser parseExpression.
    self visitNode: expr.
    self emitReturn.
    ^ self
!

compileMethod: aString
    | parser method |
    parser := Parser on: aString.
    method := parser parseMethod.
    self arguments: method arguments.
    self visitSequence: method body.
    self emitReturn.
    ^ self
!
