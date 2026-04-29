TestCase subclass: #MultipleFailureTest instanceVariableNames: ''

!MultipleFailureTest class methodsFor: 'running'!
testSelectors
    | selectors |
    selectors := Array new: 2.
    selectors at: 1 put: #testContinuesAfterFailures.
    selectors at: 2 put: #testRecordsLastFailureReason.
    ^ selectors
!

verificationSelectors
    | selectors |
    selectors := Array new: 3.
    selectors at: 1 put: #expectedFailureOne.
    selectors at: 2 put: #expectedPass.
    selectors at: 3 put: #expectedFailureTwo.
    ^ selectors
!

verificationResult
    | result |
    result := TestResult new.
    ^ self runVerificationOn: result selectors: self verificationSelectors startingAt: 1
!

runVerificationOn: aResult selectors: selectors startingAt: index
    | currentIndex testCase |
    currentIndex := index.
    [currentIndex < (selectors size + 1)] whileTrue: [
        testCase := self new.
        testCase selector: (selectors at: currentIndex).
        testCase runOn: aResult.
        currentIndex := currentIndex + 1
    ].
    ^ aResult
!

!MultipleFailureTest methodsFor: 'verification cases'!
expectedFailureOne
    ^ self assert: false description: #firstFailure
!

expectedPass
    ^ self assert: true
!

expectedFailureTwo
    ^ self assert: false description: #secondFailure
!

!MultipleFailureTest methodsFor: 'tests'!
testContinuesAfterFailures
    | result |
    result := self class verificationResult.
    ^ self assert:
        ((result runCount = 3)
            and: [(result passCount = 1)
            and: [(result failureCount = 2)
            and: [(result failureBacktraces size = 2)]]])
!

testRecordsLastFailureReason
    | result |
    result := self class verificationResult.
    ^ self assert: result lastReason equals: #secondFailure
!
