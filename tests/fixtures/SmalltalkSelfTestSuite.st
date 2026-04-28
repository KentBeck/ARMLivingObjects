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
    ^ suite
!

addTestsFrom: aTestClass to: aSuite startingAt: index
    | selectors |
    selectors := aTestClass testSelectors.
    ^ self addTestsFrom: aTestClass selectors: selectors to: aSuite startingAt: index
!

addTestsFrom: aTestClass selectors: selectors to: aSuite startingAt: index
    | testCase |
    ^ (index < (selectors size + 1))
        ifTrue: [
            testCase := aTestClass new.
            testCase selector: (selectors at: index).
            aSuite add: testCase.
            self addTestsFrom: aTestClass selectors: selectors to: aSuite startingAt: index + 1
        ]
        ifFalse: [aSuite]
!
