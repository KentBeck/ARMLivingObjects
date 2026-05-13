TestCase subclass: #CompilerTest instanceVariableNames: ''

!CompilerTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runCompilerTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 10.
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
