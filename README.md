# Structure-oriented Persistent Data Management (C)

PBL project: student, course allotment and result management for a small education institute, written in C.

## Features
- **Authentication**: user id + password (password stored as a salted hash in `users.dat`), 3 login attempts, new user registration. Default account: `admin` / `admin123`.
- **Menu driven** interface.
- **Structures**: `Student {id, name, fees, course, grade}` and `User {username, hash}`; courses (DS, AI, ML, Web) and grades (Excellent, Good, Fail) are kept as tables/enums.
- **Persistent storage**: binary files `students.dat` and `users.dat` (`fread`/`fwrite`); students are held in a dynamic array (`realloc`) and sorted by ID with `qsort`.
- **Rules enforced**: unique positive ID, non-empty name, no negative fees; a course is allotted only when fees paid >= 15000; a result can be declared only after a course is allotted.
- Reports: all students, search by ID, course-wise list, result summary.

## Build and run
```
gcc -Wall -Wextra -o student_mgmt student_mgmt.c
./student_mgmt          # Windows: student_mgmt.exe
```

## Menu
1 Register student | 2 View all | 3 Search | 4 Update name | 5 Pay fees | 6 Allot course | 7 Declare result | 8 Delete | 9 Course-wise report | 10 Result summary | 0 Logout and save

## Test cases
| # | Input / action | Expected output | Result |
|---|----------------|-----------------|--------|
| 1 | Login with wrong password 3 times | "Invalid user id or password (attempt n of 3)", then "Too many failed attempts" | Pass |
| 2 | Register user `shuvankar`, then login | "registered successfully", "Login successful" | Pass |
| 3 | Register student 101 Rahul, fees 15000 | Registered, fee status Paid | Pass |
| 4 | Register student 102 Priya, fees 5000 | Registered, fee status Pending | Pass |
| 5 | Register duplicate ID 101 | "ID 101 already exists." | Pass |
| 6 | Register ID -5 | "ID must be positive." | Pass |
| 7 | Allot course to 102 (fees pending) | "Fees pending ... Course cannot be allotted." | Pass |
| 8 | Pay 10000 for 102, then allot AI | "Fees cleared", "AI allotted to Priya." | Pass |
| 9 | Declare result for 104 (no course) | "No course allotted yet. Result cannot be declared." | Pass |
| 10 | Declare result for unknown ID 999 | "Student not found." | Pass |
| 11 | Results: 102 Excellent, 101 Fail, 103 Good | Result declared messages | Pass |
| 12 | Update name 101 to "Rahul Das", search 101 | Updated row displayed | Pass |
| 13 | Delete 104 with confirmation `y` | "Student deleted." | Pass |
| 14 | Course-wise report and result summary | 1 student each in DS, AI, ML; Web empty; Excellent 1, Good 1, Fail 1 | Pass |
| 15 | Close program, run again, view all | All 3 students still present (persistent storage) | Pass |

Full console output of the test runs is in `test_output.txt`.

## Sample output (view all students)
```
ID     NAME                     FEES       FEE STATUS COURSE   RESULT
---------------------------------------------------------------------
101    Rahul Das                15000.00   Paid       DS       Fail
102    Priya                    15000.00   Paid       AI       Excellent
103    Amit                     20000.00   Paid       ML       Good
Total students: 3
```
