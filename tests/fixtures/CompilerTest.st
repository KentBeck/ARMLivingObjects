TestCase subclass: #CompilerTest instanceVariableNames: ''

!CompilerTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runCompilerTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 4.
    selectors at: 1 put: #testCompileExpressionLiteralShape.
    selectors at: 2 put: #testCompileExpressionBinarySendShape.
    selectors at: 3 put: #testCompileMethodBinaryReturnShape.
    selectors at: 4 put: #testCompileExpressionKeywordSendShape.
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
    self assert: generator bytecodeCount equals: 20.
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
