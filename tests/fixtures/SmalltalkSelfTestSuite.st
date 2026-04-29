Object subclass: #SmalltalkSelfTestSuite instanceVariableNames: ''

!SmalltalkSelfTestSuite class methodsFor: 'running'!
selfTest
    | result |
    result := TestResult new.
    self suite runOn: result.
    ^ result
!

suite
    | suite |
    suite := TestSuite new.
    self addTestsFrom: ContextTest to: suite startingAt: 1.
    self addTestsFrom: BlockActivationTest to: suite startingAt: 1.
    self addTestsFrom: ExceptionHandlingTest to: suite startingAt: 1.
    self addTestsFrom: MultipleFailureTest to: suite startingAt: 1.
    ^ suite
!

addTestsFrom: aTestClass to: aSuite startingAt: index
    | selectors |
    selectors := aTestClass testSelectors.
    ^ self addTestsFrom: aTestClass selectors: selectors to: aSuite startingAt: index
!

addTestsFrom: aTestClass selectors: selectors to: aSuite startingAt: index
    | currentIndex testCase |
    currentIndex := index.
    [currentIndex < (selectors size + 1)] whileTrue: [
        testCase := aTestClass new.
        testCase selector: (selectors at: currentIndex).
        aSuite add: testCase.
        currentIndex := currentIndex + 1
    ].
    ^ aSuite
!
