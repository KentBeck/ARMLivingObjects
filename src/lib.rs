#![allow(unused)]

//! ARM64 Smalltalk context/frame prototype.
//!
//! Implements Cog-style stack frames where method activations use a
//! conventional stack discipline and context objects are created lazily
//! only when needed ("married" to a frame).

use std::arch::asm;

/// Word size in bytes on AArch64.
const WORD_SIZE: usize = 8;

/// A stack that grows downward, as in the Cog VM.
/// The stack pointer points to the last occupied slot.
struct Stack {
    memory: Vec<u64>,
    sp: usize, // index into memory (in words)
}

impl Stack {
    /// Create a stack with the given capacity in words.
    fn new(capacity_words: usize) -> Self {
        Stack {
            memory: vec![0u64; capacity_words],
            sp: capacity_words, // empty: SP is one past the top
        }
    }

    /// Push a value onto the stack (stack grows down).
    fn push(&mut self, value: u64) {
        self.sp -= 1;
        self.memory[self.sp] = value;
    }

    /// Read the top of stack without popping.
    fn top(&self) -> u64 {
        self.memory[self.sp]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_a_value_and_read_it_back() {
        let mut stack = Stack::new(16);
        stack.push(42);
        assert_eq!(stack.top(), 42);
    }
}
