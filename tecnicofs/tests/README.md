## TEST CASES

- Total tests : 6

### Description of each test case

#### 1. tfs_built_in_functions (TEACHER's TEST)
- Tests the simpliest situation in a file system.
- Open, write, read and close inside the file system

#### 2. copy_to_external_simple (TEACHER's TEST)
- This test gives 2 paths - one from system FS, other from tfs - and a string to be written
- Writes the string on the tfs and copies the content of tfs file to a file located in the system tfs
- Checks if the file contents are as expected
- Variables:
1. Relative output path
2. Input type : char

#### 3. copy_to_external_errors (TEACHER's TEST)
- This test causes situations where `copy_to_external_fs` should throw an error
- **Case 1** : wrong directory when giving a path
- **Case 2** : source file doesn't exist

#### 4. write_10_blocks_simple (TEACHER's TEST)
- This test fills in a new file up to 10 blocks via multiple writes
- Each write always targeting only 1 block of the file
- Checks if the file contents are as expected
- Variables :
1. `SIZE = 256`
2. `COUNT = 40`

#### 5. write_10_blocks_spill (TEACHER's TEST)
- This test fills in a new file up to 10 blocks via multiple writes 
- Some calls to tfs_write may imply filling in 2 consecutive blocks
- Checks if the file contents are as expected
- Variables :
1. `SIZE = 250`
2. `COUNT = 40`

#### 6. write_more_than_10_blocks_simple (TEACHER's TEST)
- This test fills in a new file up to 20 blocks via multiple writes
- Causes the file to hold 10 direct references + 10 indirect
references from a reference bloce
- Each write always targeting **only 1 block** of the file
- Checks if the file contents are as expected
- Variables :
1. `SIZE = 256`
2. `COUNT = 80`

#### 7. copy_to_external_int
- This test gives 2 paths - one from system FS, other from tfs - and an array of ***int*** to be written
- Writes the string on the tfs and copies the content of tfs file to a file located in the system
- Checks if the system's file contents are as expected
- Variables:
1. Absolute output path
2. Input type : int
3. Array size = 10

#### 8. copy_to_external_huge_file
- This test gives 2 paths - one from system FS, other from tfs - and an array of ***char*** to be written
- Writes the array on the tfs and copies the content of tfs file to a file located in the system
- Checks if the system's file contents are as expected
- Variables:
1. Relative output path
2. Input type : char array
3. Array size = 5204

#### 9. copy_to_external_direct_blocks
- This test gives 2 paths - one from system FS, other from tfs - and an array of ***char*** to be written
- Writes the string on the tfs and copies the content of tfs file to a file located in the system
- Checks if the system's file contents are as expected
- Variables:
1. Relative output path
2. Input type : char array
3. Array size = 10240

#### 10. copy_to_external_indirect_blocks
- This test gives 2 paths - one from system FS, other from tfs - and an array of ***char*** to be written
- Writes the string on the tfs and copies the content of tfs file to a file located in the system
- Checks if the system's file contents are as expected
- Variables:
1. Relative output path
2. Input type : char array
3. Array size = 20480







