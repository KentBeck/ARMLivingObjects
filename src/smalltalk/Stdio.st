"Stdio — raw file-descriptor access for transport"

Object subclass: #Stdio instanceVariableNames: ''

!Stdio class methodsFor: 'primitive io'!
readFd: anFd count: aCount
    <primitive: 35>
    ^ nil
!

writeFd: anFd string: aString
    <primitive: 36>
    ^ nil
!

!Stdio class methodsFor: 'convenience'!
read: anFd count: aCount
    ^ self readFd: anFd count: aCount
!

write: anFd string: aString
    ^ self writeFd: anFd string: aString
!

stdin
    ^ 0
!

stdout
    ^ 1
!

readStdin: aCount
    ^ self read: self stdin count: aCount
!

writeStdout: aString
    ^ self write: self stdout string: aString
!
