TestCase subclass: #BlockActivationTest instanceVariableNames: ''

!BlockActivationTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runBlockActivationTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 6.
    selectors at: 1 put: #testSimpleBlockActivation.
    selectors at: 2 put: #testNestedBlockActivation.
    selectors at: 3 put: #testBlockStoredInTempInsideBlock.
    selectors at: 4 put: #testNestedBlockUsesOuterTempBlock.
    selectors at: 5 put: #testOneArgumentBlockActivation.
    selectors at: 6 put: #testNestedZeroArgumentBlocks.
    ^ selectors
!

runBlockActivationTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runBlockActivationTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!BlockActivationTest methodsFor: 'helpers'!
activateSimpleBlock
    ^ [1] value
!

activateNestedBlockWith: aValue
    ^ [[aValue] value] value
!

activateTempBlockInsideBlock
    ^ [| tempBlock |
        tempBlock := [33].
        tempBlock value] value
!

activateOuterTempBlockInsideNestedBlock
    | tempBlock |
    tempBlock := [71].
    ^ [[tempBlock value] value] value
!

activateOneArgumentBlockWith: aValue
    ^ [:arg | arg] value: aValue
!

activateNestedZeroArgumentBlocks
    ^ [[2] value] value
!

!BlockActivationTest methodsFor: 'tests'!
testSimpleBlockActivation
    ^ self assert: self activateSimpleBlock equals: 1
!

testNestedBlockActivation
    ^ self assert: (self activateNestedBlockWith: 77) equals: 77
!

testBlockStoredInTempInsideBlock
    ^ self assert: self activateTempBlockInsideBlock equals: 33
!

testNestedBlockUsesOuterTempBlock
    ^ self assert: self activateOuterTempBlockInsideNestedBlock equals: 71
!

testOneArgumentBlockActivation
    ^ self assert: (self activateOneArgumentBlockWith: 19) equals: 19
!

testNestedZeroArgumentBlocks
    ^ self assert: self activateNestedZeroArgumentBlocks equals: 2
!
