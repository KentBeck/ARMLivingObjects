TestCase subclass: #ContextTest instanceVariableNames: ''

!ContextTest class methodsFor: 'running'!
selfTest
    | result case1 case2 |
    result := TestResult new.
    case1 := self new.
    (case1 testThisContextReceiverIsSelf)
        ifTrue: [result recordPass: case1 selector: #testThisContextReceiverIsSelf]
        ifFalse: [result recordFailure: case1 selector: #testThisContextReceiverIsSelf reason: #assertionFailed backtrace: (Array new: 0)].
    case2 := self new.
    (case2 testSenderReceiverIsSelf)
        ifTrue: [result recordPass: case2 selector: #testSenderReceiverIsSelf]
        ifFalse: [result recordFailure: case2 selector: #testSenderReceiverIsSelf reason: #assertionFailed backtrace: (Array new: 0)].
    ^ result
!

testSelectors
    ^ Array
        with: #testThisContextReceiverIsSelf
        with: #testSenderReceiverIsSelf
!

!ContextTest methodsFor: 'helpers'!
captureCallerContext
    ^ thisContext sender
!

!ContextTest methodsFor: 'tests'!
testThisContextReceiverIsSelf
    ^ thisContext receiver == self
!

testSenderReceiverIsSelf
    | caller |
    caller := self captureCallerContext.
    ^ caller receiver == self
!
