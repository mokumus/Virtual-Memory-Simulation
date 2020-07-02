Part 3 run command:
./sortArrays

It takes long to execute, it simply outputs test results as recored in test1.txt

This part can calculate best frame size and best PR algorithm for each sort(BONUS 1) and , results can be viewed at Part_3_Graph.xlsx file under project folder.

===================================================================

Part 2:
Bubble sort takes while to finish, about 20 seconds for  N = 8000 with LRU.
If you want to test with big inputs, change the bubble sort's end input at line 269

data_bs.end =  n_words / 4; to  data_bs.end =  n_words / pow(2,11); or a bigger divisor

Index sort algorithm applied from this paper:
https://research.ijcaonline.org/volume78/number14/pxc3891325.pdf


===================================================================


Sample output for part 2 with bubble sort adjusted, Big print interval to prevent too many prints

./sortArrays 12 7 10 LRU global  1000000000 diskFile.dat

============INPUTS============
Frame size: 2^12 = 4096
# of physical frames: 2^7 = 128
# of virtual frames: 2^10 = 1024
Page replacemet method: LRU
Allocation policy: global
Page Table Print Interval: 1000000000
Disk file name: diskFile.dat
==============================
# entries: 1152
# words: 4194304
RAM: 512 KB
VM : 4096 KB
==============================
Bubble Sort[0, 2048]					// Bubble sort N = 2048
Merge Sort[1048576, 2097152]			// While others N = 1,048,576
Quick Sort[2097152, 3145728]
Index Sort[3145728, 4194304]
===============================
#0 - Bubble Sort stats
# Reads: 6295350
# Writes: 2103250
# Misses: 1
# Replacements: 0
# DPW: 0
# DPR: 1
Sort success: yes 
Took 26.728736 seconds to execute 
===============================
#0 - Quick Sort stats
# Reads: 20971520
# Writes: 20971520
# Misses: 86322
# Replacements: 86279
# DPW: 43758
# DPR: 42564
Sort success: yes 
Took 78.285301 seconds to execute 
===============================
#0 - Merge Sort stats
# Reads: 20971520
# Writes: 20971520
# Misses: 1574
# Replacements: 1532
# DPW: 630
# DPR: 944
Sort success: yes 
Took 79.525378 seconds to execute 
===============================
#0 - Index Sort stats
# Reads: 20971520
# Writes: 20971520
# Misses: 87453
# Replacements: 87411
# DPW: 42856
# DPR: 44597
Sort success: yes 
Took 78.941527 seconds to execute 
===============================
#0 - Check stats
# Reads: 6295544
# Writes: 0
# Misses: 769
# Replacements: 769
# DPW: 0
# DPR: 769
