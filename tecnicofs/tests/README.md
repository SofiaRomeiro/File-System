## TEST CASES

- Total tests : 6
- Total passing : 6

### Description of each test case

#### 1. tfs_built_in_functions
- Tests the simpliest situation in a file system.
- Open, write, read and close inside the file system

#### 2. copy_to_external_simple
- This test gives 2 paths - one from system FS, other from tfs - and a string to be written
- Writes the string on the tfs and copies the content of tfs file to a file located in the system tfs
- Checks if the file contents are as expected
- Variables:
1. Relative output path
2. Input type : char

#### . copy_to_external_errors
- This test causes situations where `copy_to_external_fs` should throw an error
- **Case 1** : wrong directory when giving a path
- **Case 2** : source file doesn't exist

#### . write_10_blocks_simple
- This test fills in a new file up to 10 blocks via multiple writes
- Each write always targeting only 1 block of the file
- Checks if the file contents are as expected
- Variables :
1. `SIZE = 256`
2. `COUNT = 40`

#### . write_10_blocks_spill
- This test fills in a new file up to 10 blocks via multiple writes 
- Some calls to tfs_write may imply filling in 2 consecutive blocks
- Checks if the file contents are as expected
- Variables :
1. `SIZE = 250`
2. `COUNT = 40`

#### . write_more_than_10_blocks_simple
- This test fills in a new file up to 20 blocks via multiple writes
- Causes the file to hold 10 direct references + 10 indirect
references from a reference bloce
- Each write always targeting **only 1 block** of the file
- Checks if the file contents are as expected
- Variables :
1. `SIZE = 256`
2. `COUNT = 80`

#### . copy_to_external_ints
- This test gives 2 paths - one from system FS, other from tfs - and a string to be written
- Writes the string on the tfs and copies the content of tfs file to a file located in the system tfs
- Checks if the file contents are as expected
- Variables:
1. Absolute output path
2. Input type : int

#### . copy_to_external_huge_file
- { write test description }
- Variables:
1. Relative output path
2. Input type : file
3. Array size = 5204

#### . copy_to_external_direct_blocks
- { write test description }
- Variables:
1. Relative output path
2. Input type : file
3. Array size = 5204







