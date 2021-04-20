# Media-Manager
A c++ windows application to find and delete redundant media files found in a directory for Windows 8.1 OS

The program was made using VS community 2015, C++ 14.0, and boost_1_75_0\stage\win32\lib

Duplicate media files are found by first loading every file of the desired type from a chosen directory into a multimap, which is then trimmed into a smaller multimap that only contains files that had a filesize matching at least one other file in the directory (potential duplicates).
After the filesize scan, the files are opened in binary mode via ifstream and a partial hash is performed on them for further identification.
Then, during a duplicate media scan, any files loaded into the trimmed multimap that have a filesize and hash value that match (and are under the 250MB filesize limit), are checked byte by byte to insure they are an exact match before a file is deleted

The program takes in a user input root directory, such as G:\projects\test images, and finds all repeat .jpg, .jpeg, .png, .gif, and .webm files within.
The user is then prompted whether they would like to organize their media collection manually (scan the main directory from a subfolder, where the subfolder will contain the remaining media copy), or to simply remove all but one copy of each duplicate file which will be left arbitrarily wherever it was first found.

It's intended use it to have the user input folders one by one, from most important to least, to retain the users preferred file structure and to only perform a full duplicate removal once that is finished.

Using a test folder of 1000 files and three folders totalling 1GB of space, the program loaded the directory in 3954ms and removed 991 duplicate files in 1507640ms (about 1 duplicate file every 1.5s).

On my personal media hoard of 137GB, it scanned a total of 86k files across 402 folders in 104732ms, removed 2075 duplicates using the folders I had selected in a total of 4461157ms, and finished with a full redundant media removal of an extra 2572 files in 4117327ms.



Changes that can be made in the future:
The buffers used to check images are set at a flat 250MB and must be zeroed after every check.
Can instead only read and zero memory space matching the actual filesize.

Read speed is determined mostly by hardware, but deletion speed can be increased by multithreading that portion of code
Add multithreading.

There is currently no error checking, so for an example, a char input on the main menu will simply break the program.
Add error checking.

The program is also sometimes intercepted and throttled by antivirus software.
Might be able to whitelist or encapsulate it, or just ask user disable antivirus at runtime if it matters.

Currently, the only purpose of the database the program writes is to allow the user to resume where they left off if they close the program.
It's arguably faster to just perform another directory read operation on startup, so it might be better to just remove it.
