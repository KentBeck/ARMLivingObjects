"TestResult — explicit result object for in-image xUnit"
"Instance variables: runCount, passCount, failureCount, lastFailure, lastSelector, lastReason"

Object subclass: #TestResult instanceVariableNames: 'runCount passCount failureCount lastFailure lastSelector lastReason'

!TestResult methodsFor: 'initialization'!
initialize
    runCount := 0.
    passCount := 0.
    failureCount := 0.
    lastFailure := nil.
    lastSelector := nil.
    lastReason := nil.
    ^ self
!

!TestResult methodsFor: 'recording'!
recordPass: aCase selector: aSelector
    runCount := runCount + 1.
    passCount := passCount + 1.
    lastFailure := nil.
    lastSelector := aSelector.
    lastReason := nil.
    ^ aCase
!

recordFailure: aCase selector: aSelector reason: aSymbol
    runCount := runCount + 1.
    failureCount := failureCount + 1.
    lastFailure := aCase.
    lastSelector := aSelector.
    lastReason := aSymbol.
    ^ aCase
!

!TestResult methodsFor: 'accessing'!
runCount
    ^ runCount
!

passCount
    ^ passCount
!

failureCount
    ^ failureCount
!

lastFailure
    ^ lastFailure
!

lastSelector
    ^ lastSelector
!

lastReason
    ^ lastReason
!

wasSuccessful
    ^ failureCount = 0
!
