
# Assignment 2
[Assignment Description](https://www.cs.binghamton.edu/~kchiu/cs447/assign/2/)


---

Name: Ritik Ramaiya
 
B-Number: B00960003
 
---
*Any comments, descriptions goes Here*

how to run this project:

1. clone the github repo and use "make clean" command to compile everything.

2. run the training_data.py file with the command and .dat file will be created. 

        python2 training_data.py 4 4 0
        
3. run the query_file.py file with the command:

         python2 query_file.py 5 4 0 2
         
4. after the .dat file is created run the dump command to see the points.

     ./dump data_22155646.dat
     
     ./dump query_22155708.dat
     
5. create a k-nn executable file.


    g++ k-nn.cpp -o k-nn
    
    then use the "make" command.
     
6. now use the command given:



./k-nn n-cores data_22155646.dat query_22155708.dat result_file.dat

n-cores can be of any value

7. to verify the result file use dump command.


./dump result_file.dat



** HAVE ATTEMPTED EXTRA CREDIT PART TOO **
