"ExpressionSpecTest — one expression test migrated to xUnit"

!ExpressionSpecTest methodsFor: 'tests'!
testThisContextReceiverIsNil
    ^ thisContext receiver isNil
!

!ExpressionSpecTest methodsFor: 'running'!
runOn: aResult
    self testThisContextReceiverIsNil.
    aResult recordPass: self selector: #testThisContextReceiverIsNil.
    ^ aResult
!
