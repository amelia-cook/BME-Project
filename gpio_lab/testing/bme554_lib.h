/**
 * amelia's idea for what should occur:
 *  - .h file with macro definitions (or, exposed function headers)
 *  - the macros call functions in the .c file ("hidden" from the students iykyk)
 *  - the functions can then raise events that we can wait for from the tests
 *      - the events themselves would also be within the .c file rather than the .h (also "hidden")
 *  - events can be for things like button presses or error state
 *      - can't do it for things like state transitions, we won't have a sense of what the student states will look like
 *  - macros CAN take in values of variables, which means that we don't care about variable names
 * 
 * for example:
 *  - freq down button gets triggered via our test
 *  - student code decrements accordingly etc, calls our macro and gives frequency
 *  - our macro raises the "down button pressed" event that the test is waiting on
 *  - the test then gets the student's frequency, calculates the frequency based on the LED's actual output,
 *    and compares both to each other and to what we expect based on the test
 *      - if we start running main and press freq down, we expect freq = 1
 *      - if we start running main, press freq up, freq up, freq down, we expect freq = 3
 *      - etc etc
 * 
 * cons:
 *  - does this require different macros for each lab?
 *  - still can't test specific state transitions (ie awake vs reset vs entry states etc etc)
 * 
 * pros:
 *  - lots of the macros would be reusable either way
 *  - there is no harm in giving them all of them at the start (don't need to update their library)
 *  - we can control what information they give us without stressing about code specifics
 *  - state transitions don't REALLY matter
 *      - state transitions are ultimately a design choice, so they fall under the code review side of things
 *      - this tests the internal mechanisms that impact external functionality, which is what's important
 */