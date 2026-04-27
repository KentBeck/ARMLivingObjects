"TestResult — explicit result object for in-image xUnit"
"Instance variables: runCount, passCount, failureCount, lastFailure, lastSelector, lastReason, lastBacktrace, failureBacktraces"

Object subclass: #TestResult instanceVariableNames: 'runCount passCount failureCount lastFailure lastSelector lastReason lastBacktrace failureBacktraces'

!TestResult class methodsFor: 'instance creation'!
new
    ^ self basicNew initialize
!

!TestResult methodsFor: 'initialization'!
initialize
    runCount := 0.
    passCount := 0.
    failureCount := 0.
    lastFailure := nil.
    lastSelector := nil.
    lastReason := nil.
    lastBacktrace := nil.
    failureBacktraces := Array new: 0.
    ^ self
!

!TestResult methodsFor: 'recording'!
recordPass: aCase selector: aSelector
    runCount := runCount + 1.
    passCount := passCount + 1.
    lastFailure := nil.
    lastSelector := aSelector.
    lastReason := nil.
    lastBacktrace := nil.
    ^ aCase
!

recordFailure: aCase selector: aSelector reason: aSymbol
    ^ self recordFailure: aCase selector: aSelector reason: aSymbol backtrace: (Array new: 0)
!

recordFailure: aCase selector: aSelector reason: aSymbol backtrace: aBacktrace
    runCount := runCount + 1.
    failureCount := failureCount + 1.
    lastFailure := aCase.
    lastSelector := aSelector.
    lastReason := aSymbol.
    lastBacktrace := aBacktrace.
    self storeFailureBacktrace: aBacktrace.
    ^ aCase
!

!TestResult methodsFor: 'private'!
storeFailureBacktrace: aBacktrace
    | grown oldSize |
    oldSize := failureBacktraces size.
    grown := Array new: oldSize + 1.
    self copyBacktracesFrom: failureBacktraces to: grown startingAt: 1.
    grown at: oldSize + 1 put: aBacktrace.
    failureBacktraces := grown.
    ^ aBacktrace
!

copyBacktracesFrom: oldStorage to: newStorage startingAt: index
    ^ (index < (oldStorage size + 1))
        ifTrue: [
            newStorage at: index put: (oldStorage at: index).
            self copyBacktracesFrom: oldStorage to: newStorage startingAt: index + 1
        ]
        ifFalse: [newStorage]
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

lastBacktrace
    ^ lastBacktrace
!

failureBacktraces
    ^ failureBacktraces
!

wasSuccessful
    ^ failureCount = 0
!
