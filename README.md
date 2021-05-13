# Media-Manager
A windows application written in c++ to find and delete redundant media files found in a directory for Windows 8.1

The program was made using VS community 2015, C++ 14.0, and boost_1_75_0\stage\win32\lib

This is a console application program where a user inputs the root directory of their media library. After that, the user may choose a subdirectory from within that root directory to perform a partial duplicate file scan, or they may simply choose to delete all duplicate files from the entire directory. This is a multithreaded program running on main + 4 additional threads used for file comparison.

An example of a valid directory is G:\projects\test images, and the currently supported filetypes are .jpg, .jpeg, .png, .gif, and .webm files up to a size of 75MB (larger files and files of other types are skipped, but the maximum size may be adjusted up to 2GB depending on available RAM; each thread requires 2 buffers, so 4 threads would use 16GB of RAM in that scenario)


### Here is an example of the program in motion

The main menu

![main menu](https://user-images.githubusercontent.com/43391569/118060953-93ea5000-b359-11eb-88dc-e78a9ab2a3fe.PNG)

My chosen filespace

![filespace (before)](https://user-images.githubusercontent.com/43391569/118060510-8e403a80-b358-11eb-8729-a38aff420cdc.PNG)

And the program reading the filespace

![file input](https://user-images.githubusercontent.com/43391569/118060475-7cf72e00-b358-11eb-97cd-ee94759845bc.PNG)

The program scanning from a subdirectory located within my filespace

![run from subdirectory](https://user-images.githubusercontent.com/43391569/118060602-baf45200-b358-11eb-8ef8-a7e37b6a971e.PNG)

The program running a full scan after I finished with the folders I wanted to

![input31](https://user-images.githubusercontent.com/43391569/118060527-939d8500-b358-11eb-85bd-050fdda7789f.PNG)
![input32](https://user-images.githubusercontent.com/43391569/118060573-a912af00-b358-11eb-8c19-29626081e8dc.PNG)

And my filespace after all duplicates were removed

![filespace (after)](https://user-images.githubusercontent.com/43391569/118060622-c6477d80-b358-11eb-8b35-11767cdaacd4.PNG)
