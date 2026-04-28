TestCase subclass: #ExceptionHandlingTest instanceVariableNames: ''

!ExceptionHandlingTest class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    ^ self runExceptionHandlingTestsOn: result selectors: self testSelectors startingAt: 1
!

testSelectors
    | selectors |
    selectors := Array new: 7.
    selectors at: 1 put: #testOnDoCatchesSignaledException.
    selectors at: 2 put: #testInnerHandlerWins.
    selectors at: 3 put: #testEnsureRunsOnNormalCompletion.
    selectors at: 4 put: #testEnsureRunsOnExceptionalExit.
    selectors at: 5 put: #testExceptionCarriesMessageText.
    selectors at: 6 put: #testResumeReturnsHandlerValue.
    selectors at: 7 put: #testErrorHandlerDoesNotCatchException.
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
    ^ Exception
!

errorClass
    ^ Error
!

signalTestError
    ^ self errorClass signal: 'boom'
!

signalTestErrorWith: aString
    ^ self errorClass signal: aString
!

signalTestException
    ^ self exceptionClass signal: 'boom'
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

testEnsureRunsOnNormalCompletion
    | steps value |
    steps := Array new: 2.
    value := [
        steps at: 1 put: #body.
        11
    ] ensure: [
        steps at: 2 put: #ensure
    ].
    ^ self assert:
        (value = 11 and: [
            (steps at: 1) = #body and: [(steps at: 2) = #ensure]])
!

testEnsureRunsOnExceptionalExit
    | steps value |
    steps := Array new: 2.
    value := [
        [
            steps at: 1 put: #body.
            self signalTestError.
            1
        ] ensure: [
            steps at: 2 put: #ensure
        ]
    ] on: self errorClass do: [2].
    ^ self assert:
        (value = 2 and: [
            (steps at: 1) = #body and: [(steps at: 2) = #ensure]])
!

testExceptionCarriesMessageText
    | value |
    value := [
        self signalTestErrorWith: 'kapow'
    ] on: self errorClass do: [:ex | ex messageText].
    ^ self assert: value equals: 'kapow'
!

testResumeReturnsHandlerValue
    | value |
    value := [
        self signalTestError
    ] on: self errorClass do: [:ex | ex resume: 41].
    ^ self assert: value equals: 41
!

testErrorHandlerDoesNotCatchException
    | value |
    value := [
        [
            self signalTestException.
            1
        ] on: self errorClass do: [2]
    ] on: self exceptionClass do: [3].
    ^ self assert: value equals: 3
!
