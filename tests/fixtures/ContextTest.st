TestCase subclass: #ContextTest instanceVariableNames: ''

!ContextTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runContextTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 6.
    selectors at: 1 put: #testThisContextReceiverIsSelf.
    selectors at: 2 put: #testRepeatedThisContextIsIdentical.
    selectors at: 3 put: #testSenderReceiverIsSelf.
    selectors at: 4 put: #testBlockContextReceiverIsSelf.
    selectors at: 5 put: #testBlockContextHomeReceiverIsSelf.
    selectors at: 6 put: #testBlockContextHasClosure.
    ^ selectors
!

runContextTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runContextTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!ContextTest methodsFor: 'helpers'!
captureCurrentContext
    ^ thisContext
!

captureCallerContext
    ^ thisContext sender
!

captureBlockContext
    ^ [thisContext] value
!

!ContextTest methodsFor: 'tests'!
testThisContextReceiverIsSelf
    ^ self assert: thisContext receiver == self
!

testRepeatedThisContextIsIdentical
    | first second |
    first := thisContext.
    second := thisContext.
    ^ self assert: first == second
!

testSenderReceiverIsSelf
    | caller |
    caller := self captureCallerContext.
    ^ self assert: caller receiver == self
!

testBlockContextReceiverIsSelf
    | blockContext |
    blockContext := self captureBlockContext.
    ^ self assert: blockContext receiver == self
!

testBlockContextHomeReceiverIsSelf
    | blockContext homeContext |
    blockContext := self captureBlockContext.
    homeContext := blockContext home.
    ^ self assert:
        ((homeContext == nil)
            ifTrue: [false]
            ifFalse: [homeContext receiver == self])
!

testBlockContextHasClosure
    | blockContext |
    blockContext := self captureBlockContext.
    ^ self assert: (blockContext closure == nil) == false
!
