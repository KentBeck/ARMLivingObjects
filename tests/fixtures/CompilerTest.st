TestCase subclass: #CompilerTest instanceVariableNames: ''

!CompilerTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runCompilerTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 18.
    selectors at: 1 put: #testCompileExpressionLiteralShape.
    selectors at: 2 put: #testCompileExpressionBinarySendShape.
    selectors at: 3 put: #testCompileMethodBinaryReturnShape.
    selectors at: 4 put: #testCompileExpressionKeywordSendShape.
    selectors at: 5 put: #testCompileMethodUnarySendReturnShape.
    selectors at: 6 put: #testCompileMethodConditionalNonLocalReturnShape.
    selectors at: 7 put: #testCompileMethodPrimitiveShape.
    selectors at: 8 put: #testCompileMethodPrimitiveFallbackBodyShape.
    selectors at: 9 put: #testCompileMethodTemporariesShape.
    selectors at: 10 put: #testCompileMethodGlobalReferenceShape.
    selectors at: 11 put: #testCompileMethodRealWriteStreamNextPutShape.
    selectors at: 12 put: #testCompileMethodRealTestSuiteRunOnShape.
    selectors at: 13 put: #testCompileMethodRealTestSuiteAddShape.
    selectors at: 14 put: #testCompileMethodRealSmalltalkSelfTestSuiteAddTestsShape.
    selectors at: 15 put: #testCompileMethodRealParserExpressionOrAssignmentShape.
    selectors at: 16 put: #testCompileMethodRealCodeGeneratorVisitAssignmentShape.
    selectors at: 17 put: #testCompileMethodRealParserTemporariesShape.
    selectors at: 18 put: #testCompileMethodRealCodeGeneratorVisitStatementsShape.
    ^ selectors
!

runCompilerTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runCompilerTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!CompilerTest methodsFor: 'helpers'!
assertLiteral: index of: aGenerator equals: expected
    ^ self assert: (aGenerator literalAt: index) equals: expected
!

assert: aGenerator bytecodeAt: index equals: expected
    ^ self assert: (aGenerator bytecodeAt: index) equals: expected
!

!CompilerTest methodsFor: 'tests'!
testCompileExpressionLiteralShape
    | generator |
    generator := Compiler compileExpression: '1'.
    self assert: generator literalCount equals: 1.
    self assert: generator bytecodeCount equals: 6.
    ^ self assertLiteral: 1 of: generator equals: 1
!

testCompileExpressionBinarySendShape
    | generator |
    generator := Compiler compileExpression: '1 + 2'.
    self assert: generator literalCount equals: 3.
    self assert: generator bytecodeCount equals: 20.
    self assertLiteral: 1 of: generator equals: 1.
    self assertLiteral: 2 of: generator equals: 2.
    ^ self assertLiteral: 3 of: generator equals: #+
!

testCompileMethodBinaryReturnShape
    | generator |
    generator := Compiler compileMethod: 'answer ^ 1 + 2'.
    self assert: generator literalCount equals: 3.
    self assert: generator bytecodeCount equals: 19.
    self assertLiteral: 1 of: generator equals: 1.
    self assertLiteral: 2 of: generator equals: 2.
    ^ self assertLiteral: 3 of: generator equals: #+
!

testCompileExpressionKeywordSendShape
    | generator |
    generator := Compiler compileExpression: 'self perform: #yourself'.
    self assert: generator literalCount equals: 2.
    self assert: generator bytecodeCount equals: 16.
    self assertLiteral: 1 of: generator equals: #yourself.
    ^ self assertLiteral: 2 of: generator equals: #perform:
!

testCompileMethodUnarySendReturnShape
    | generator |
    generator := Compiler compileMethod: 'bar ^ self foo'.
    self assert: generator tempCount equals: 0.
    self assert: generator literalCount equals: 1.
    self assert: generator bytecodeCount equals: 11.
    ^ self assertLiteral: 1 of: generator equals: #foo
!

testCompileMethodConditionalNonLocalReturnShape
    | generator |
    generator := Compiler compileMethod: 'handlesSignalClass: aClass
    aClass == nil
        ifTrue: [^ false]
        ifFalse: [
            self == aClass
                ifTrue: [^ true]
                ifFalse: [^ self handlesSignalClass: aClass superclass]
        ]'.
    self assert: generator literalCount equals: 5.
    ^ self assert: generator bytecodeCount equals: 39
!

testCompileMethodPrimitiveShape
    | generator |
    generator := Compiler compileMethod: 'identity: anObject
    <primitive: 12>
    ^ false'.
    self assert: generator primitiveIndex equals: 12.
    self assert: generator literalCount equals: 1.
    self assert: generator bytecodeCount equals: 6.
    ^ self assertLiteral: 1 of: generator equals: false
!

testCompileMethodPrimitiveFallbackBodyShape
    | generator |
    generator := Compiler compileMethod: 'basicNew
    <primitive: 8>
    self error: ''basicNew primitive failed'''.
    self assert: generator primitiveIndex equals: 8.
    self assert: generator literalCount equals: 2.
    self assert: generator bytecodeCount equals: 16.
    self assertLiteral: 1 of: generator equals: 'basicNew primitive failed'.
    ^ self assertLiteral: 2 of: generator equals: #error:
!

testCompileMethodTemporariesShape
    | generator |
    generator := Compiler compileMethod: 'with: a
    | arr |
    arr := Array new: 1.
    arr at: 1 put: a.
    ^ arr'.
    self assert: generator tempCount equals: 1.
    self assert: generator literalCount equals: 3.
    ^ self assert: generator bytecodeCount equals: 54
!

testCompileMethodGlobalReferenceShape
    | generator association |
    generator := Compiler compileMethod: 'smalltalkDictionary ^ Dictionary smalltalk'.
    self assert: generator literalCount equals: 1.
    self assert: generator bytecodeAt: 1 equals: 18.
    association := generator literalAt: 1.
    ^ self assert: (association == nil) equals: false
!

testCompileMethodRealWriteStreamNextPutShape
    | generator |
    generator := Compiler compileMethod: 'nextPut: aByte
    | one |
    one := String new: 1.
    one at: 1 put: aByte.
    collection := collection , one.
    ^ aByte'.
    self assert: generator tempCount equals: 1.
    self assert: generator bytecodeAt: 1 equals: 18.
    ^ true
!

testCompileMethodRealTestSuiteRunOnShape
    | generator |
    generator := Compiler compileMethod: 'runOn: aResult
    | index |
    index := 1.
    [index < (tally + 1)] whileTrue: [
        (tests at: index) runOn: aResult.
        index := index + 1
    ].
    ^ aResult'.
    self assert: generator tempCount equals: 1.
    ^ true
!

testCompileMethodRealTestSuiteAddShape
    | generator |
    generator := Compiler compileMethod: 'add: aTest
    tests == nil
        ifTrue: [self initialize]
        ifFalse: [self].
    tally = tests size
        ifTrue: [self growStorage]
        ifFalse: [self].
    tally := tally + 1.
    tests at: tally put: aTest.
    ^ aTest'.
    self assert: generator tempCount equals: 0.
    ^ true
!

testCompileMethodRealSmalltalkSelfTestSuiteAddTestsShape
    | generator |
    generator := Compiler compileMethod: 'addTestsFrom: aTestClass selectors: selectors to: aSuite startingAt: index
    | currentIndex testCase |
    currentIndex := index.
    [currentIndex < (selectors size + 1)] whileTrue: [
        testCase := aTestClass new.
        testCase selector: (selectors at: currentIndex).
        aSuite add: testCase.
        currentIndex := currentIndex + 1
    ].
    ^ aSuite'.
    self assert: generator tempCount equals: 2.
    ^ true
!

testCompileMethodRealParserExpressionOrAssignmentShape
    | generator |
    generator := Compiler compileMethod: 'parseExpressionOrAssignment
    | expr nextToken |
    expr := self parseExpression.
    expr isVariable
        ifTrue: [0]
        ifFalse: [^ expr].

    nextToken := tokenizer peek.
    (nextToken isSpecialText: '':='')
        ifTrue: [0]
        ifFalse: [^ expr].

    tokenizer next.
    ^ AssignmentNode variable: expr value: self parseExpression'.
    self assert: generator tempCount equals: 2.
    ^ true
!

testCompileMethodRealCodeGeneratorVisitAssignmentShape
    | generator |
    generator := Compiler compileMethod: 'visitAssignment: aNode
    self visitNode: aNode value.
    self emitDuplicate.
    self storeVariable: aNode variable name.
    ^ self'.
    self assert: generator tempCount equals: 0.
    ^ true
!

testCompileMethodRealParserTemporariesShape
    | generator |
    generator := Compiler compileMethod: 'parseTemporaries
    | token temps |
    token := tokenizer next.
    (token text = ''|'')
        ifTrue: [
            temps := Array new: 16.
            ^ self collectTemporaries: temps at: 1
        ]
        ifFalse: [
            tokenizer unread: token.
            ^ Array new: 0
        ]'.
    self assert: generator tempCount equals: 2.
    ^ true
!

testCompileMethodRealCodeGeneratorVisitStatementsShape
    | generator |
    generator := Compiler compileMethod: 'visitStatements: stmts from: index last: lastIndex
    | stmt |
    stmt := stmts at: index.
    self visitNode: stmt.
    index = lastIndex
        ifTrue: [^ self]
        ifFalse: [
            self emitPop.
            ^ self visitStatements: stmts from: index + 1 last: lastIndex
        ]'.
    self assert: generator tempCount equals: 1.
    ^ true
!
