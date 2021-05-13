#include <iostream>
#include <codecvt> ///req for windows to read unicode characters
#include <chrono> ///used to log time taken to perform processes
#include <thread> ///multithreading library
#include <boost/filesystem.hpp> ///used to read windows file system
#include <boost/functional/hash.hpp> ///used to hash incoming files

///these files are used as well but are added by other libraries or by the compiler i'm using (VS 2015, C++ 14.0, along with boost 1.75_0 win32)
//#include <fstream>
//#include <iterator>
//#include <string>
//#include <locale>
//#include <wstring>
//#include <map>
///-----------------------------------------------------------------------------------------------------------

///struct used to store required file data
struct media_data
{
	double fhash;
	std::wstring fpath;
};


//menu functions------------------------------------------------------------
///cleans console for menuing purposes
void clear_console();
//--------------------------------------------------------------------------


//file scan functions-------------------------------------------------------
///takes root directory wstring as input, scans for all wanted media files
///returns a trimmed map containing only potential duplicate files into media_map
void scan_directories(std::wstring &directorypath, int &counter, std::multimap<int, media_data> &media_map);

///scan_directories helper function
void find_duplicates(std::vector<int> &duplicate_keys, std::multimap<int, std::wstring> &media_map);

///get hash value (partial) of selected file
double get_hash(std::wstring filepath, int filesize);

///scan_directories helper function
void process_entries(std::vector<int> &keys_of_duplicates, std::multimap<int, std::wstring> &initial_read, std::multimap<int, media_data> &return_map);

///separates trimmed multimap into an array of vectors to allow multithreading
void split_map (std::vector<std::vector<std::pair<int, media_data>>> &media_vector_vector, std::multimap<int, media_data> &media_map_to_split);
//--------------------------------------------------------------------------


//deletion via subdirectory functions---------------------------------------
///multithread manager function: deletes duplicate media files from selected subdirectory from entire directory
void selected_duplicate_deletion(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, std::wstring &media_dir, int &counter);

///reads the given directory and fills the given multimap with data from all valid files within
void load_subdirectory(std::wstring &directory, std::multimap<int, media_data>& sub_map);

///selected_duplicate_deletion helper function: spreads deletion work between multiple threads defined within selected_duplicate_deletion
void remove_selected_duplicates_from_subvectors(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, std::vector<std::vector<std::pair<int, media_data>>> &sub_media_vector, std::vector<int> &indices, int &counter, char *buffer1, char *buffer2);

//deletion of all duplicate media files in directory functions---------------
///multithread manager function: deletes all duplicate media files from entire directory
void full_duplicate_deletion(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, int &counter);

///full_duplicate_deletion helper function: spreads deletion work between multiple threads defined within full_duplicate_deletion
void remove_all_duplicates_from_subvectors(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, std::vector<int> &indices, int &counter, char *buffer1, char *buffer2);

///remove_duplicates_from_subvectors helper function: scans given vector for given iterator, deletes if found
void sync_vectors(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, int &index, std::vector<std::pair<int, media_data>>::iterator &mt);
//--------------------------------------------------------------------------


//shared functions----------------------------------------------------------
///performs byte by byte comparison on given media_files to confirm they are identical
///returns 1 if true, 0 if false, and -1 or -2 if a read error occurs on file1 or file2 respectively
int match_media_files(std::ifstream &ifile1, std::ifstream &ifile2, char *buffer1, char *buffer2, int &filesize, std::wstring &file1, std::wstring &file2);

///0s indexes in global media buffers after each use by threads for safety
void clear_buffers(int &filesize, char *buffer1, char *buffer2);

///used by get_hash to clear local buffer
void clear_buffer(int &filesize, char *buffer1);
//--------------------------------------------------------------------------


//currently unused functions------------------------------------------------
void debug();

///reads db file into multimap
void read_database(std::string const &db, std::multimap<int, media_data> &media_map);

///writes multimap to bd file
void write_map_to_db(std::string const &db, std::multimap<int, media_data> &media_map);
//--------------------------------------------------------------------------


//buffer_size may be increased to allow higher file sizes, but will use more RAM
#define buffer_size 75000000 //75MB of char space (files with higher filesizes wouldn't be fully compared so are excluded)
//note: filesize is stored in an integer, so the maximum buffer_size possible is 2147483647byes (2048MB or 2GB)

//the number of bytes to compute during file hashing
#define bytes_to_hash 30000 //.03MB of char space (larger numbers cause significant performance drops on file read and are unnecessary)

//required to stop threads from running over each other when calling _wremove
std::mutex deletion_mutex;

//media buffers need to declared globally to avoid a potential stack overflow
char buffer1[buffer_size] = { 0 }; //arrays are to 0 for safety
char buffer2[buffer_size] = { 0 };
char buffer3[buffer_size] = { 0 };
char buffer4[buffer_size] = { 0 };
char buffer5[buffer_size] = { 0 };
char buffer6[buffer_size] = { 0 };
char buffer7[buffer_size] = { 0 };
char buffer8[buffer_size] = { 0 };

int main()
{
	//std::string const db = "media.db"; //static database location, loaded on startup and saved on return (currently unused)
	//std::wofstream ofile; //ofstream for db file
	std::multimap<int, media_data> media_map; //multimap in the form <int, double, string>, <filesize, file hash, filepath>, with filesize being the key value
	std::vector<std::vector<std::pair<int, media_data>>> media_vector; //contains multimap from above fractured by filesizes for thread safety
	std::wstring media_dir; //user input media directories (root and sub)
	int input = 0; //user menu input
	int counter = 0; //counter for files scanned/deleted

	//console control menu
	while (true)
	{
		switch (input)
		{
		//main menu
		case 0:
		{
			clear_console();

			//prompt user
			//std::cout << "Duplicate Media Remover.\n";
			std::cout << "This program will remove duplicate files with a filesize less than " << buffer_size/1000000 << "MB\n";
			std::cout << "(it is recommended to run 2 on all important folders before moving to 3)\n";
			std::cout << "-----------------------------------------------------------------------\n\n";
			std::cout << "1: choose root media file directory\n\n";
			std::cout << "2: remove duplicates from subdirectory (used to preserve file structure)\n\n";
			std::cout << "3: remove duplicates from entire directory (does not preserve file structure)\n\n";
			std::cout << "4: exit program\n";
			std::cout << "\n\nInput: ";
			std::cin >> input;
			break;
		}

		//read files to db
		case 1:
		{
			//delete old db values if program is reloaded but new input is selected
			//media_map.clear();

			clear_console();

			//prompt user
			std::cout << "Scan Directory\nPlease input the filepath to the media files directory you would like to scan:\n";
			std::cout << "for example, C:\\media\\vacation photos\n\n";
			std::cout << "Input: ";

			//mixing '>>' '<<' and getline, have to clean input
			std::cin.clear();
			std::cin.sync();
			std::cin.ignore();

			//read user input
			std::getline(std::wcin, media_dir);

			//log time taken to run functions
			auto time1 = std::chrono::high_resolution_clock::now();

			//read files from directory into media_map (only retains files that have potential duplicates)
			scan_directories(media_dir, counter, media_map);

			//split mmap into vector array
			split_map(media_vector, media_map);

			auto time2 = std::chrono::high_resolution_clock::now();

			auto ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);

			std::cout << "\nScanned " << counter << " files in " << ms_taken.count() << "ms\n\n";
			
			//reset variables
			counter = 0;
			input = 0;
			
			//wait for user recognition
			system("PAUSE");
			break;
		}

		//delete media_files from entire directory with final copies ending in selected subdirectory
		case 2:
		{
			clear_console();

			//prompt user
			std::cout << "Please select a subdirectory\n";
			std::cout << "   The given directory will be scanned for all valid media files\n";
			std::cout << "   Those files will then be compared against files in the root directory\n";
			std::cout << "   Duplicate files will be removed\n";
			std::cout << "   The remaining file will remain in the selected subdirectory\n\n";
			std::cout << "Input: ";


			//mixing '>>' '<<' and getline, have to clean input
			std::cin.clear();
			std::cin.sync();
			std::cin.ignore();

			//read user input
			std::getline(std::wcin, media_dir);

			//run function and log time
			auto time1 = std::chrono::high_resolution_clock::now();

			selected_duplicate_deletion(media_vector, media_dir, counter);

			auto time2 = std::chrono::high_resolution_clock::now();

			auto ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);

			std::cout << "\nAction completed. " << counter << " duplicate media files were removed in " << ms_taken.count() << "ms\n\n";

			//reset variables
			counter = 0;
			input = 0;

			//wait for user recognition
			system("PAUSE");
			break;
		}

		//delete all redundant media_files
		case 3:
		{
			clear_console();

			std::cout << "Full redundant media files scan.\n\n";

			//run function and log times
			auto time1 = std::chrono::high_resolution_clock::now();

			full_duplicate_deletion(media_vector, counter);

			auto time2 = std::chrono::high_resolution_clock::now();

			auto ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);

			std::cout << "\nAction completed. " << counter << " duplicate media files were removed in " << ms_taken.count() << "ms\n\n\n";

			//reset variables
			counter = 0;
			input = 0;

			//wait for user recognition
			system("PAUSE");
			break;
		}

		//exit program
		case 4:
		{
			//write db file on exit to allow speedier startup if user has to stop midway
			//write_map_to_db(db, media_map);

			return 0;
		}

		//ask again if non-menu item is chosen
		default:
			input = 0;
		}
	}
}

//menu functions------------------------------------------------------------
void clear_console()
{
	std::cin.clear();
	system("cls");
}
//--------------------------------------------------------------------------


//file scan functions-------------------------------------------------------
void scan_directories(std::wstring &directorypath, int &counter, std::multimap<int, media_data> &media_map)
{
	boost::filesystem::recursive_directory_iterator start(directorypath); //root directory
	const boost::filesystem::recursive_directory_iterator end; //end of root directory (and all subdirectories)
	std::multimap<int, std::wstring> initial_read; //temporary multimap for files
	std::vector<int> keys_of_duplicates; //vector that stores any multimap entry with more than one matching key (repeat filesizes)

	//recursively iterate all directories and scan for all wanted filetypes (doesn't check for filesize limitation)
	for (; start != end; ++start)
	{
		counter++; //number of files read

		//check if file is appropriate type (haven't tested with mp4, mp3, or other media formats yet, so they are excluded for now)
		if (start->path().extension() == ".jpg" || start->path().extension() == ".jpeg" || start->path().extension() == ".png" || start->path().extension() == ".gif" || start->path().extension() == ".webm")
		{
			//check if the current file is small enough to fit inside the global buffers
			if (boost::filesystem::file_size(start->path()) <= buffer_size)
			{
				//insert filesize and path into multimap if it is appropriate type
				initial_read.insert({ boost::filesystem::file_size(start->path()), start->path().wstring() });
			}
		}
	}

	//read any values from initial_read with duplicate keys into a vector
	find_duplicates(keys_of_duplicates, initial_read);

	//trim off any excess files from temporary multimap, and then, sync it with main multimap (media_map)
	process_entries(keys_of_duplicates, initial_read, media_map);

	//local multimap is deleted on function return
	//multimap containing potential duplicate files is sent back to main
}

void find_duplicates(std::vector<int> &duplicate_keys, std::multimap<int, std::wstring> &media_map)
{
	int prev_key = 0;
	bool already_written = false;

	//iterate multimap while push_backing all keys that contain more than one file into the vector
	for (std::multimap<int, std::wstring>::iterator it = media_map.begin(); it != media_map.end(); it++)
	{
		//check if filesizes are the same and if the specific filesize has already been written into vector
		if (prev_key == it->first && false == already_written)
		{
			//if so, push key value into vector
			duplicate_keys.push_back(it->first);
			already_written = true;
		}
		//elseif keys are different, reset check to find the next key value to insert
		else if (prev_key != it->first) already_written = false; //array is sorted, so only have to check current and prev elements to check key space

		prev_key = it->first;
	}
}

double get_hash(std::wstring filepath, int filesize)
{
	static char local_buffer[bytes_to_hash] = { 0 }; //buffer for incoming media binary
	static std::ifstream ifile;
	static double hash_value;

	//load file in binary mode
	ifile.open(filepath, std::ifstream::binary | std::ifstream::in);

	//return if read fails
	if (ifile.fail())
		return -1;

	//load file into memory
	std::memset(local_buffer, 0, bytes_to_hash);

	ifile.read(local_buffer, bytes_to_hash);

	//get hash
	hash_value = boost::hash_value(local_buffer);
	
	//close file
	ifile.close();

	//clear buffer
	clear_buffer(filesize, local_buffer);

	//return hash value of file in buffer
	return hash_value;
}

void process_entries(std::vector<int> &keys_of_duplicates, std::multimap<int, std::wstring> &initial_read, std::multimap<int, media_data> &return_map)
{
	media_data feeder; //feeds values to multimap
	int filesize = 0;

	for (int i = 0; i < keys_of_duplicates.size(); i++)
	{
		//puts range values of selected key into result
		std::pair<std::multimap<int, std::wstring>::iterator, std::multimap<int, std::wstring>::iterator> result = initial_read.equal_range(keys_of_duplicates[i]);

		//inner loop iterates the media_map only for the chosen key value from first to last entry
		for (std::multimap<int, std::wstring>::iterator mt = result.first; mt != result.second; mt++)
		{
			filesize = mt->first;

			//get hash value of file
			feeder.fhash = get_hash(mt->second, filesize);

			//get filepath
			feeder.fpath = mt->second;

			//emplace filesize as key and media_map (hash and filepath) as second value
			return_map.emplace(mt->first, feeder);
		}
	}
}

void split_map(std::vector<std::vector<std::pair<int, media_data>>> &media_vector_vector, std::multimap<int, media_data> &media_map_to_split)
{
	//for each unique filesize (key) value in media_map, create a new vector to contain that keys entries

	std::vector<std::pair<int, media_data>> carryover_vector;
	int current_filesize = NULL;
	double current_hash = NULL;
	static int i = 0;

	std::multimap<int, media_data>::iterator it = media_map_to_split.begin();

	//load first value into vector
	current_filesize = it->first;

	current_hash = it->second.fhash;

	carryover_vector.emplace_back(it->first, it->second);

	for (it; it != media_map_to_split.end();)
	{
		it++;

		//shortcut if end of mmap is reached
		if (it == media_map_to_split.end())
		{
			media_vector_vector.push_back(carryover_vector);
		}
		//else, normal logic
		else
		{
			//if key values match, place them into the same vector
			if (current_filesize == it->first && current_hash == it->second.fhash)
			{
				carryover_vector.emplace_back(it->first, it->second);
			}

			//else, new key is found
			//add completed vector to vector of vectors and start a new one
			else
			{
				current_filesize = it->first;

				current_hash = it->second.fhash;

				media_vector_vector.push_back(carryover_vector);

				carryover_vector.clear();

				carryover_vector.emplace_back(it->first, it->second);
			}
		}
	}
}
//--------------------------------------------------------------------------


//deletion via subdirectory functions---------------------------------------
void selected_duplicate_deletion(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, std::wstring &media_dir, int &counter)
{
	std::multimap<int, media_data> sub_map; //multimap for initial file read
	std::vector<std::vector<std::pair<int, media_data>>> sub_vector; //vector of vectors to store fractured multimap for thread safety
	std::thread thread1, thread2, thread3, thread4;
	std::vector<int> thread1_queue, thread2_queue, thread3_queue, thread4_queue; //queues for each running thread
	bool thread1_is_running = false, thread2_is_running = false, thread3_is_running = false, thread4_is_running = false;
	int temp_counter1 = 0, temp_counter2 = 0, temp_counter3 = 0, temp_counter4 = 0; //counters to sum after threads run
	int switcher = 1; //switch channel


	load_subdirectory(media_dir, sub_map);

	split_map(sub_vector, sub_map);

	for (int i = 0; i < sub_vector.size(); i++)
	{
		switch (switcher)
		{
		case 1:
		{
			thread1_queue.push_back(i);
			switcher++;
			break;
		}
		case 2:
		{
			thread2_queue.push_back(i);
			switcher++;
			break;
		}
		case 3:
		{
			thread3_queue.push_back(i);
			switcher++;
			break;
		}
		case 4:
		{
			thread4_queue.push_back(i);
			switcher = 1;
			break;
		}
		}
	}

	//run multithreading (if thread queue isn't empty)
	if (0 < thread1_queue.size())
	{
		thread1_is_running = true;
		thread1 = std::thread(remove_selected_duplicates_from_subvectors, std::ref(media_vector), std::ref(sub_vector), std::ref(thread1_queue), std::ref(temp_counter1), buffer1, buffer2);
	}

	if (0 < thread2_queue.size())
	{
		thread2_is_running = true;
		thread2 = std::thread(remove_selected_duplicates_from_subvectors, std::ref(media_vector), std::ref(sub_vector), std::ref(thread2_queue), std::ref(temp_counter2), buffer3, buffer4);
	}

	if (0 < thread3_queue.size())
	{
		thread3_is_running = true;
		thread3 = std::thread(remove_selected_duplicates_from_subvectors, std::ref(media_vector), std::ref(sub_vector), std::ref(thread3_queue), std::ref(temp_counter3), buffer5, buffer6);
	}

	if (0 < thread4_queue.size())
	{
		thread4_is_running = true;
		thread4 = std::thread(remove_selected_duplicates_from_subvectors, std::ref(media_vector), std::ref(sub_vector), std::ref(thread4_queue), std::ref(temp_counter4), buffer7, buffer8);
	}

	//join threads (if ran)
	if (thread1_is_running)
	{
		thread1.join();
		counter += temp_counter1;
	}

	if (thread2_is_running)
	{
		thread2.join();
		counter += temp_counter2;
	}

	if (thread3_is_running)
	{
		thread3.join();
		counter += temp_counter3;
	}

	if (thread4_is_running)
	{
		thread4.join();
		counter += temp_counter4;
	}
}

void load_subdirectory(std::wstring &directory, std::multimap<int, media_data>& sub_map)
{
	media_data feeder; //feeds data to multimap
	boost::filesystem::recursive_directory_iterator start(directory); //start of directory
	const boost::filesystem::recursive_directory_iterator end; //end of directory
	int filesize;
															   //scan subdirectory (and its subdirectories) into the sub_map if filetypes match
	for (; start != end; ++start)
	{
		if (start->path().extension() == ".jpg" || start->path().extension() == ".jpeg" || start->path().extension() == ".png" || start->path().extension() == ".gif" || start->path().extension() == ".webm")
		{
			filesize = boost::filesystem::file_size(start->path());
			if (filesize <= buffer_size)
			{
				feeder.fhash = get_hash(start->path().wstring(), filesize);

				feeder.fpath = start->path().wstring();

				sub_map.emplace(boost::filesystem::file_size(start->path()), feeder);
			}
		}
	}
}

void remove_selected_duplicates_from_subvectors(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, std::vector<std::vector<std::pair<int, media_data>>> &sub_media_vector, std::vector<int> &indices, int &counter, char *buffer1, char *buffer2)
{
	std::ifstream ifile1, ifile2; //ifstream for files to scan against each other
	int media_case = 0; //return value for match_media_files (-1 and -2 are read errors, 0 is no match, 1 is match)

	//iterate through indices given to the running thread
	for (int i = 0; i < indices.size(); i++)
	{
		for (auto it = sub_media_vector[indices[i]].begin(); it != sub_media_vector[indices[i]].end(); it++)
		{
			//scan for appropriate filesize in media_vector
			for (int j = 0; j < media_vector.size(); j++)
			{
				for (auto nit = media_vector[j].begin(); nit != media_vector[j].end(); nit++)
				{
					//when correct index is found
					if (it->first == nit->first)
					{
						for (nit; nit != media_vector[j].end();)
						{
							//check if current 'it' is the selected 'nit'
							if (it->second.fpath != nit->second.fpath)
							{
								//if duplicate images are found, remove second element found (earliest vector entries are preserved)
								media_case = match_media_files(ifile1, ifile2, buffer1, buffer2, it->first, it->second.fpath, nit->second.fpath);

								//if media_files match
								if (1 == media_case)
								{
									//reset media_case
									media_case = 0;

									//delete offending media from file system (mutex required to not potentially overload system calls)
									deletion_mutex.lock();

									_wremove(nit->second.fpath.c_str());

									deletion_mutex.unlock();

									//check if entry needs to be erased from it vector
									//sync_vectors(sub_media_vector, i, nit);

									//erase entry from nit vector
									nit = media_vector[j].erase(nit);

									//increase media files deleted counter
									counter++;
								}
								else
								{
									nit++;
								}
							}
							else nit++;

							//may hit end of iteration early due to deletion in media_vector, so check is required
							if (media_vector[j].end() == nit)
								break;
						}
					}
					if (media_vector[j].end() == nit)
						break;
				}
			}
		}
	}
}

void sync_vectors(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, int &index, std::vector<std::pair<int, media_data>>::iterator &mt)
{
	for (auto it = media_vector[index].begin(); it != media_vector[index].end(); it++)
	{
		if (it->second.fpath == mt->second.fpath)
		{
			//if deleted element is found in media_vector, remove it
			media_vector[index].erase(it);
			break;
		}
	}
}
//--------------------------------------------------------------------------


//deletion of all duplicate media files in directory functions---------------
void full_duplicate_deletion(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, int &counter)
{
	std::thread thread1, thread2, thread3, thread4;
	std::vector<int> thread1_queue, thread2_queue, thread3_queue, thread4_queue;
	bool thread1_is_running = false, thread2_is_running = false, thread3_is_running = false, thread4_is_running = false;
	int temp_counter1 = 0, temp_counter2 = 0, temp_counter3 = 0, temp_counter4 = 0;
	int switcher = 1;

	//split vector<vector> indices into three queues
	for (int i = 0; i < media_vector.size(); i++)
	{
		switch (switcher)
		{
		case 1:
		{
			thread1_queue.push_back(i);
			switcher++;
			break;
		}
		case 2:
		{
			thread2_queue.push_back(i);
			switcher++;
			break;
		}
		case 3:
		{
			thread3_queue.push_back(i);
			switcher++;
			break;
		}
		case 4:
		{
			thread4_queue.push_back(i);
			switcher = 1;
			break;
		}
		}
	}

	//run multithreading if thread queue isn't empty
	if (0 < thread1_queue.size())
	{
		thread1_is_running = true;
		thread1 = std::thread(remove_all_duplicates_from_subvectors, std::ref(media_vector), std::ref(thread1_queue), std::ref(temp_counter1), buffer1, buffer2);
	}
	
	if (0 < thread2_queue.size())
	{
		thread2_is_running = true;
		thread2 = std::thread(remove_all_duplicates_from_subvectors, std::ref(media_vector), std::ref(thread2_queue), std::ref(temp_counter2), buffer3, buffer4);
	}
	
	if (0 < thread3_queue.size())
	{
		thread3_is_running = true;
		thread3 = std::thread(remove_all_duplicates_from_subvectors, std::ref(media_vector), std::ref(thread3_queue), std::ref(temp_counter3), buffer5, buffer6);
	}

	if (0 < thread4_queue.size())
	{
		thread4_is_running = true;
		thread4 = std::thread(remove_all_duplicates_from_subvectors, std::ref(media_vector), std::ref(thread4_queue), std::ref(temp_counter4), buffer7, buffer8);
	}

	//join threads (if ran)
	if (thread1_is_running)
	{
		thread1.join();
		counter += temp_counter1;
	}

	if (thread2_is_running)
	{
		thread2.join();
		counter += temp_counter2;
	}

	if (thread3_is_running)
	{
		thread3.join();
		counter += temp_counter3;
	}

	if (thread4_is_running)
	{
		thread4.join();
		counter += temp_counter4;
	}
}

void remove_all_duplicates_from_subvectors(std::vector<std::vector<std::pair<int, media_data>>> &media_vector, std::vector<int> &indices, int &counter, char *buffer1, char *buffer2)
{
	std::ifstream ifile1, ifile2;
	int media_case = 0; //return value for match_media_files (-1 is error, 0 is no match, 1 is match)

	//iterate through indices given to the running thread
	for (int i = 0; i < indices.size(); i++)
	{
		//iterate the appropiate subvector given by the current index
		for (auto it = media_vector[indices[i]].begin(); it != media_vector[indices[i]].end(); it++) //likely to cause a crash from ++it's location
		{
			//iterate all following elements, comparing against original
			for (auto nit = std::next(it); nit != media_vector[indices[i]].end();)
			{
				//if duplicate images are found, remove second element found (earliest vector entries are preserved)
				media_case = match_media_files(ifile1, ifile2, buffer1, buffer2, it->first, it->second.fpath, nit->second.fpath);

				//if media_files match
				if (1 == media_case)
				{
					//reset media_case
					media_case = 0;

					//delete offending media from file system (mutex required not to potentially overload system calls)
					deletion_mutex.lock();

					_wremove(nit->second.fpath.c_str());

					deletion_mutex.unlock();

					//erase entry from vector
					nit = media_vector[indices[i]].erase(nit);

					//increase media files deleted counter
					counter++;
				}
				else
				{
					nit++;
				}
			}
			//may hit end of iteration early due to deletion in media_vector, so check is required
			if (media_vector[indices[i]].end() == it) 
				break;
		}
	}
}
//--------------------------------------------------------------------------


//shared functions----------------------------------------------------------
int match_media_files(std::ifstream &ifile1, std::ifstream &ifile2, char *buffer1, char *buffer2, int &filesize, std::wstring &file1, std::wstring &file2)
{
	//read media1 into a char buffer
	ifile1.open(file1, std::ifstream::binary | std::ifstream::in);


	//check if media1 opened successfully or return error if it didn't
	if (ifile1.fail())
	{
		clear_buffers(filesize, buffer1, buffer2);
		return -1; //if file2 fails to read, return -1
	}

	//load media1 into buffer
	std::memset(buffer1, 0, buffer_size);
	ifile1.read(buffer1, buffer_size);
	
	//close ifstream for media1
	ifile1.close();

	//read media2 into a char buffer
	ifile2.open(file2, std::ifstream::binary | std::ifstream::in);

	//check if media2 opened successfully or return error if it didn't
	if (ifile2.fail())
	{
		clear_buffers(filesize, buffer1, buffer2);
		return -2; //if file2 fails to read, return -2
	}

	//load media2 into buffer
	std::memset(buffer2, 0, buffer_size);
	ifile2.read(buffer2, buffer_size);
	
	//close ifstream for media2
	ifile2.close();

	//compare buffers byte by byte
	for (int i = 0; i < filesize; i++)
	{
		//if buffers differ
		if (buffer1[i] != buffer2[i])
		{
			//clear used portion of buffers
			clear_buffers(filesize, buffer1, buffer2);
			return 0; //return false for match
		}
	}

	//only reached if buffers are identical
	//reset buffers
	clear_buffers(filesize, buffer1, buffer2);
	return 1; //return true for match
}

void clear_buffers(int &filesize, char *buffer1, char *buffer2)
{
	//reset selected buffer indices to 0
	for (int i = 0; i < filesize; i++)
	{
		buffer1[i] = 0;
		buffer2[i] = 0;
	}
}

void clear_buffer(int &filesize, char *buffer1)
{
	//reset selected buffer indices to 0
	for (int i = 0; i < filesize; i++)
	{
		buffer1[i] = 0;
	}
}
//--------------------------------------------------------------------------


//currently unused functions------------------------------------------------
void debug()
{
	/*int _switch = 1;
	std::wstring fpath = L"G:\\projects\\test images";
	std::wstring subfolder = L"G:\\projects\\test images\\sub folder";

	scan_directories(fpath, counter, media_map);

	//split mmap into vector array
	split_map(media_vector, media_map);

	counter = 0;

	//std::vector<int> test_vec;
	//test_vec.push_back(0);
	//test_vec.push_back(1);
	//test_vec.push_back(2);
	//test_vec.push_back(3);

	//log time taken to run function
	auto time1 = std::chrono::high_resolution_clock::now();

	selected_duplicate_deletion(media_vector, subfolder, counter);
	//read files from directory into media_map (only retains files that have potential duplicates)
	//full_duplicate_deletion(media_vector, counter);

	auto time2 = std::chrono::high_resolution_clock::now();

	auto ms_taken1 = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);

	std::cout << "\nRemoved " << counter << " files in " << ms_taken1.count() << "ms\n\n";

	//log time taken to run function
	auto time3 = std::chrono::high_resolution_clock::now();

	//selected_duplicate_deletion(media_vector, subfolder, counter);
	//read files from directory into media_map (only retains files that have potential duplicates)
	full_duplicate_deletion(media_vector, counter);

	auto time4 = std::chrono::high_resolution_clock::now();

	auto ms_taken2 = std::chrono::duration_cast<std::chrono::milliseconds>(time4 - time3);

	std::cout << "\nRemoved " << counter << " files in " << ms_taken2.count() << "ms\n\n";

	//final_cleanup(ifile1, ifile2, separated_filesizes, counter);*/
}

void read_database(std::string const &db, std::multimap<int, media_data> &media_map)
{
	std::wifstream ifile;
	std::wstring line;
	media_data feeder;
	int filesize, count = 0;
	double hash;

	ifile.open(db);

	std::getline(ifile, line); //throw away first line (indexers)

	while (std::getline(ifile, line))
	{
		if (0 == count) //get filesize
		{
			filesize = std::stoi(line);
			count++;
		}

		else if (1 == count) //get hash
		{
			hash = std::stod(line);
			count++;
		}

		else if (2 == count) //get filepath
		{
			feeder.fhash = hash;
			feeder.fpath = line;
			media_map.emplace(filesize, feeder);
			count = 0;
		}

	}

	ifile.close();
}

void write_map_to_db(std::string const &db, std::multimap<int, media_data> &media_map)
{
	std::locale loc(std::locale::classic(), new std::codecvt_utf8<wchar_t>); // to imbue wofstream with unicode chars
	std::wofstream database;

	database.imbue(loc);

	//open db
	database.open(db);

	//check for write permission
	if (database.fail())
		return;

	//prevent hash from being written in sci notation
	database.setf(std::ios_base::fixed);

	//output first line
	database << "filesize, file hash, filepath\n";

	//output map data for each element
	for (std::multimap<int, media_data>::iterator it = media_map.begin(); it != media_map.end(); ++it)
	{
		//map_reader = it->second;
		database << it->first << std::endl;
		database << it->second.fhash << std::endl;
		database << it->second.fpath << std::endl;
	}

	//close file
	database.close();
}
//--------------------------------------------------------------------------