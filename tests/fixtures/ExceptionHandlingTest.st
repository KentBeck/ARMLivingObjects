TestCase subclass: #ExceptionHandlingTest instanceVariableNames: ''

!ExceptionHandlingTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runExceptionHandlingTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 2.
    selectors at: 1 put: #testOnDoCatchesSignaledException.
    selectors at: 2 put: #testInnerHandlerWins.
    ^ selectors
!

runExceptionHandlingTestsOn: aResult selectors: selectors startingAt: index
    | selector testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            selector := selectors at: index.
            testCase := self new.
            testCase selector: selector.
            testCase runOn: aResult.
            self runExceptionHandlingTestsOn: aResult selectors: selectors startingAt: index + 1
        ]
        ifFalse: [aResult]
!

!ExceptionHandlingTest methodsFor: 'helpers'!
exceptionClass
    ^ Object
!

errorClass
    ^ Object
!

signalTestError
    ^ self errorClass signal: 'boom'
!

signalTestErrorWith: aString
    ^ self errorClass signal: aString
!

nestedHandledValue
    ^ [
        [
            self signalTestError.
            1
        ] on: self errorClass do: [2]
    ] on: self exceptionClass do: [3]
!

!ExceptionHandlingTest methodsFor: 'tests'!
testOnDoCatchesSignaledException
    | value |
    value := [
        self signalTestError.
        1
    ] on: self errorClass do: [2].
    ^ self assert: value equals: 2
!

testInnerHandlerWins
    ^ self assert: self nestedHandledValue equals: 2
!
