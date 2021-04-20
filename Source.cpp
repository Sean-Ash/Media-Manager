///this program will remove all repeat jpg, jpeg, png, gif, and webm files with a filesize under 250mb from the scanned directory
///if the user wishes to maintain their file structure, they may scan from subdirectories one by one so that the remaining file stays within the chosen subdirectory
#include <iostream>
#include <codecvt>
#include <chrono> ///used to log time taken to perform processes
#include <boost/filesystem.hpp> ///used to read windows file system
#include <boost/functional/hash.hpp> ///used to hash incoming files

///these header files are used as well but are automatically added by the compiler I'm using (VS 2015, C++ 14.0, along with boost 1.75_0 win32)
//#include <fstream>
//#include <iterator>
//#include <string>
//#include <locale>
//#include <wstring>
//#include <map>
///-----------------------------------------------------------------------------------------------------------

///struct used by multimap to store media data
struct media_data
{
	double fhash;
	std::wstring fpath;
};


///cleans console for menuing purposes
void clear_console();

///0s all indexes in global media buffers (buffer1 and buffer2)
void clear_buffers();

///get hash value (partial) of selected file
double get_hash(std::wstring filepath);


//--------------------------------------------------------------------------
///takes root directory where files are located as input, scans for all wanted media files
///returns a trimmed map containing only potential duplicate files into media_map
void scan_directories(std::wstring& directorypath, int& counter, std::multimap<int, media_data>& media_map);

///scan_directories helper function
void find_duplicates(std::vector<int>& duplicates, std::multimap<int, std::wstring>& media_map);

///scan_directories helper function
void process_entries(std::vector<int>& keys_of_duplicates, std::multimap<int, std::wstring>& initial_read, std::multimap<int, media_data>& return_map);
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
///deletes media_files matching files from within chosen subdirectory, leaving one copy behind in the chosen subdirectory
void clear_selected_media_files(std::ifstream& ifile1, std::ifstream& ifile2, std::wstring& media_dir, std::multimap<int, media_data>& media_map, int& counter);

///clear_selected_media_files helper function, reads in sub_map from the chosen subdirectory to scan against the primary media_map
void load_subdirectory(std::wstring directory, std::multimap<int, media_data>& sub_map);

///clear_selected_media_files helper function, keeps media_map and sub_map up to date on which indexes have been removed in tree structures
void sync_maps(std::multimap<int, media_data>& sub_map, std::multimap<int, media_data>::iterator mt);
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
///reads db file into multimap
void read_database(std::string const& db, std::multimap<int, media_data>& media_map);

///performs byte by byte comparison on given media_files to confirm they are identical (returns 1 if true, 0 if false, and -1 if error)
int match_media_files(std::ifstream& ifile1, std::ifstream& ifile2, std::wstring& file1, std::wstring& file2);

void write_map_to_db(std::string const& db, std::multimap<int, media_data>& media_map);
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
///brute deletion function that deletes all duplicate media files found from the entire directory, leaving remaining file in an arbitrary location within
///should run clear_media_files individually from all important subdirectories (where you wish surviving media copies to remain) before using
void final_cleanup(std::ifstream& ifile1, std::ifstream& ifile2, std::multimap<int, media_data>& media_map, int& counter);
//--------------------------------------------------------------------------


//media buffers declared globally to avoid stack overflow
//buffer_size may be increased to allow higher file sizes, but program is not optimized for it and will result in slower runtime
const int buffer_size = 250000000; //250mb of char space (files with higher filesizes wouldn't be fully compared so are excluded)
char buffer1[buffer_size] = { 0 }; //arrays must be initialized to 0 in case first input media is too small to fill buffer
char buffer2[buffer_size] = { 0 };

int main()
{
	std::string const db = "database.txt"; //static database location, loaded on startup and saved on return
	std::ifstream ifile1, ifile2; //ifstream for media_files
	std::wofstream ofile; //ofstream for db file
	std::multimap<int, media_data> media_map; //multimap in the form <int, double, string>, <filesize, file hash, filepath> with filesize being the key value
	std::wstring media_dir; //user input media directories (root and sub)
	int input = 0, counter = 0; //user menu input and counter for files scanned/deleted


								//read db into map
	read_database(db, media_map);


	//console control menu
	while (true)
	{
		switch (input)
		{
			//main menu
		case 0:
		{
			clear_console();
			std::cout << "Choose an input:\n";
			std::cout << "1: input root directory to scan for duplicate media\n(creates database of media files within)\n\n";
			std::cout << "2: scan from subdirectory\n(removes duplicate media files from database and leaves the last copy in chosen subdirectory)\n\n";
			std::cout << "3: final cleanup\n(only select after all important subdirectories are processed individually)\n\n";
			std::cout << "4: exit program\n";
			std::cout << "\n\nInput: ";
			std::cin >> input;
			break;
		}

		//read files to db
		case 1:
		{
			//delete old db values if program is reloaded but new input is selected
			media_map.clear();


			clear_console();
			std::cout << "Scan Directory\nPlease input the filepath to the media files directory you would like to scan:\n";
			std::cout << "for example, C:\\media\n\n";
			std::cout << "Input: ";

			//mixing '>>' '<<' and getline, have to clean input
			std::cin.clear();
			std::cin.sync();
			std::cin.ignore();


			//read user input
			std::getline(std::wcin, media_dir);


			//log time taken to run function
			auto time1 = std::chrono::high_resolution_clock::now();

			//read files from directory into media_map (only retains files that have potential duplicates)
			scan_directories(media_dir, counter, media_map);

			auto time2 = std::chrono::high_resolution_clock::now();

			auto ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);


			std::cout << "\nScanned " << counter << " files in " << ms_taken.count() << "ms\n\n";
			counter = 0;
			input = 0;
			system("PAUSE");
			break;
		}

		//delete media_files from entire directory with final copies ending in selected subdirectory
		case 2:
		{
			clear_console();
			std::cout << "Please select a subdirectory to scan from\n";
			std::cout << "   Duplicates of media files from the selected subdirectory will be removed.\n";
			std::cout << "   The final copy of any deleted media files will remain in selected subdirectory.\n\n";
			std::cout << "Input: ";


			//mixing '>>' '<<' and getline, have to clean input
			std::cin.clear();
			std::cin.sync();
			std::cin.ignore();


			std::getline(std::wcin, media_dir);

			//log time taken to run function
			auto time1 = std::chrono::high_resolution_clock::now();

			//load chose subdirectory, and for any file found within to be duplicate in the entire directory, delete all but one
			clear_selected_media_files(ifile1, ifile2, media_dir, media_map, counter);

			auto time2 = std::chrono::high_resolution_clock::now();

			auto ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);


			std::cout << "\nAction completed. " << counter << " duplicate media files were removed in " << ms_taken.count() << "ms\n\n";

			counter = 0;
			input = 0;
			system("PAUSE");
			break;
		}

		//delete all redundant media_files
		case 3:
		{
			clear_console();

			std::cout << "Full redundant media files scan.\n\n";


			//log time taken to run function
			auto time1 = std::chrono::high_resolution_clock::now();

			final_cleanup(ifile1, ifile2, media_map, counter);

			auto time2 = std::chrono::high_resolution_clock::now();

			auto ms_taken = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1);


			std::cout << "\nAction completed. " << counter << " duplicate media files were removed in " << ms_taken.count() << "ms\n\n\n";

			counter = 0;
			input = 0;
			system("PAUSE");
			break;
		}

		//exit program
		case 4:
		{
			//write db file on exit to allow speedier startup if user has to stop midway
			write_map_to_db(db, media_map);

			return 0;
		}

		//ask again if non-menu item is chosen
		default:
			input = 0;
		}
	}
}

void clear_console()
{
	std::cin.clear();
	system("cls");
}

void clear_buffers()
{
	for (int i = 0; i < buffer_size; i++)
	{
		buffer1[i] = 0;
		buffer2[i] = 0;
	}
}

double get_hash(std::wstring filepath)
{
	const int local_block_size = 30000; //only require partial hash for initial matching (lower is faster, more prevents unintentional collisions)
	char local_buffer[local_block_size] = { 0 }; //buffer for incoming media binary
	std::ifstream ifile;

	ifile.open(filepath, std::ifstream::binary | std::ifstream::in);

	if (ifile.fail())
		return -1;

	std::memset(local_buffer, 0, local_block_size);

	ifile.read(local_buffer, local_block_size);

	return boost::hash_value(local_buffer);
}

void scan_directories(std::wstring& directorypath, int& counter, std::multimap<int, media_data>& media_map)
{
	boost::filesystem::recursive_directory_iterator start(directorypath); //start directory
	const boost::filesystem::recursive_directory_iterator end; //end directory
	std::multimap<int, std::wstring> initial_read; //temporary multimap for files
	std::vector<int> keys_of_duplicates; //vector that stores any multimap entry with more than one file for a key (repeat filesizes)
	std::wstring fpath = L""; //filepath, using wstring to allow non-eng chars
	int filesize = 0;

	//recursively iterate all directories and scan for all wanted filetypes (doesn't check for filesize limitation)
	for (; start != end; ++start)
	{
		counter++;

		if (start->path().extension() == ".jpg" || start->path().extension() == ".jpeg" || start->path().extension() == ".png" || start->path().extension() == ".gif" || start->path().extension() == ".webm")
		{
			//insert filesize and path into multimap
			initial_read.insert({ boost::filesystem::file_size(start->path()), start->path().wstring() });
		}
	}


	//read any values from temporary multimap with duplicate keys into vector
	find_duplicates(keys_of_duplicates, initial_read);

	//trim off any excess files from temporary multimap, and then, sync it with main multimap (media_map)
	process_entries(keys_of_duplicates, initial_read, media_map);

	//local multimap is deleted on function return
	//multimap containing potential duplicate files is sent back to main
}

void find_duplicates(std::vector<int>& duplicate_keys, std::multimap<int, std::wstring>& media_map)
{
	int current_key = 0, prev_key = 0;
	int key_count = 0;
	bool already_written = false;

	//iterate multimap while push_backing all keys that contain more than one file into vector
	for (std::multimap<int, std::wstring>::iterator it = media_map.begin(); it != media_map.end(); it++)
	{
		//point to current iterator filesize value
		current_key = it->first;

		//check if filesizes are the same and if the specific filesize has already been written into vector
		if (prev_key == current_key && false == already_written)
		{
			key_count = media_map.count(current_key);
			duplicate_keys.push_back(current_key);
			already_written = true;
		}
		//elseif keys are different, reset check to find the next key value to insert
		else if (prev_key != current_key) already_written = false; //array is sorted, so only have to check current and prev elements to check key space

		prev_key = current_key;
	}
}

void process_entries(std::vector<int>& keys_of_duplicates, std::multimap<int, std::wstring>& initial_read, std::multimap<int, media_data>& return_map)
{
	media_data feeder; //feeds values to multimap
	int get_key = 0; //reads key value from vector

	for (int i = 0; i < keys_of_duplicates.size(); i++)
	{
		//get key of duplicate
		get_key = keys_of_duplicates[i];

		//puts range values of selected key into result
		std::pair<std::multimap<int, std::wstring>::iterator, std::multimap<int, std::wstring>::iterator> result = initial_read.equal_range(get_key);

		//inner loop iterates the media_map
		for (std::multimap<int, std::wstring>::iterator mt = result.first; mt != result.second; mt++)
		{
			//get hash value of file
			feeder.fhash = get_hash(mt->second);//boost::hash_value(local_buffer);

												//get filepath
			feeder.fpath = mt->second;

			//emplace filesize as key and media_map (hash and filepath) as second value
			return_map.emplace(mt->first, feeder);
		}
	}
}

void clear_selected_media_files(std::ifstream& ifile1, std::ifstream& ifile2, std::wstring& media_dir, std::multimap<int, media_data>& media_map, int& counter)
{
	std::pair<std::multimap<int, media_data>::iterator, std::multimap<int, media_data>::iterator> get_data;
	std::multimap<int, media_data> sub_map;
	int check_dir = 0;
	int media_case = 0; //return value for match_media_files (-1 is error, 0 is no match, 1 is match)
	std::size_t s_case = 0;


	//get directory user would like to perform subscan with
	load_subdirectory(media_dir, sub_map);

	//iterate outer sub_map containing the media files of interest
	for (std::multimap<int, media_data>::iterator it = sub_map.begin(); it != sub_map.end(); it++)
	{

		//scan against the inner, primary map to find files that may match
		for (std::multimap<int, media_data>::iterator mt = media_map.begin(); mt != media_map.end();)
		{

			//if filesize and filepath match while filepaths differ and the size of the media is less than the globally defined filesize limit (long line, but eh)
			if (it->first == mt->first && it->second.fhash == mt->second.fhash && it->second.fpath != mt->second.fpath && buffer_size > it->first && buffer_size > mt->first)
			{
				//check selected files against each other
				media_case = match_media_files(ifile1, ifile2, it->second.fpath, mt->second.fpath);

				//if media_files match
				if (1 == media_case)
				{
					//reset
					media_case = 0;

					//delete offending media from file system
					_wremove(mt->second.fpath.c_str());

					//delete offending media from map(s)
					sync_maps(sub_map, mt);

					mt = media_map.erase(mt);

					//increase media_files deleted counter
					counter++;
				}
			}

			else
			{
				mt++;
			}
		}

		if (sub_map.end() == it) //may hit end of iteration early due to deletion in sub_map
			break;
	}
}

void load_subdirectory(std::wstring directory, std::multimap<int, media_data>& sub_map)
{
	media_data feeder; //feeds data to multimap
	boost::filesystem::recursive_directory_iterator start(directory); //start of directory
	const boost::filesystem::recursive_directory_iterator end; //end of directory

															   //scan subdirectory (and its subdirectories) into the sub_map if filetypes match
	for (; start != end; ++start)
	{
		if (start->path().extension() == ".jpg" || start->path().extension() == ".jpeg" || start->path().extension() == ".png" || start->path().extension() == ".gif" || start->path().extension() == ".webm")
		{
			feeder.fhash = get_hash(start->path().wstring());

			feeder.fpath = start->path().wstring();

			sub_map.emplace(boost::filesystem::file_size(start->path()), feeder);
		}
	}
}

void sync_maps(std::multimap<int, media_data>& sub_map, std::multimap<int, media_data>::iterator mt)
{
	std::multimap<int, media_data>::iterator next_it;

	//scan sub_map for iterator mt
	for (std::multimap<int, media_data>::iterator it = sub_map.begin(), next_it = it; it != sub_map.end(); it = next_it)
	{
		++next_it;

		if (it->first == mt->first && it->second.fhash == mt->second.fhash && it->second.fpath == mt->second.fpath)
		{
			//if found, delete it
			sub_map.erase(it);
		}
	}
}

void read_database(std::string const& db, std::multimap<int, media_data>& media_map)
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

int match_media_files(std::ifstream& ifile1, std::ifstream& ifile2, std::wstring& file1, std::wstring& file2)
{
	int sequential_0s = 0;

	//read media1 into a char buffer
	ifile1.open(file1, std::ifstream::binary | std::ifstream::in);


	//check if media opened successfully, return error if not
	if (ifile1.fail())
	{
		clear_buffers();
		return -1;
	}

	std::memset(buffer1, 0, buffer_size);
	ifile1.read(buffer1, buffer_size);
	ifile1.close();

	//read media2 into a char buffer
	ifile2.open(file2, std::ifstream::binary | std::ifstream::in);

	if (ifile2.fail())
	{
		clear_buffers();
		return -2;
	}

	std::memset(buffer2, 0, buffer_size);
	ifile2.read(buffer2, buffer_size);
	ifile2.close();

	//compare buffers byte by byte
	for (int i = 0; i < buffer_size; i++)
	{
		if (buffer1[i] != buffer2[i])
		{
			clear_buffers();
			return 0; //return no match if media buffers differ
		}

		if ('0' == buffer1[i] && '0' == buffer1[i - 1])
		{
			sequential_0s++;
			if (1000 == sequential_0s)
			{
				clear_buffers();
				return 0; //return if eof is passed and 0 space is reached early (I'm assuming the media doesn't contain 1000 sequential 0s naturally somehow)
			}
		}
		else sequential_0s = 0;
	}
	clear_buffers();
	return 1; //return true if media buffers are identical
}

void write_map_to_db(std::string const& db, std::multimap<int, media_data>& media_map)
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

void final_cleanup(std::ifstream& ifile1, std::ifstream& ifile2, std::multimap<int, media_data>& media_map, int& counter)
{
	int media_case = 0;

	//iterate entire map, deleting any repeat values found and leaving only 1 copy behind
	for (std::multimap<int, media_data>::iterator it = media_map.begin(), next_it = it; it != media_map.end();)
	{
		++next_it;

		//break early if end of map is reached
		if (media_map.end() == it || media_map.end() == next_it)
		{
			break;
		}

		//if iterators match, filepaths differ, and they are within the allowed filesize
		if (it->first == next_it->first && it->second.fhash == next_it->second.fhash && it->second.fpath != next_it->second.fpath && buffer_size > it->first && buffer_size >next_it->first)
		{
			//check if media match with byte by byte comparison
			media_case = match_media_files(ifile1, ifile2, it->second.fpath, next_it->second.fpath);

			//if media_files match
			if (1 == media_case)
			{
				//reset
				media_case = 0;

				//delete offending media from file system
				_wremove(it->second.fpath.c_str());

				it = media_map.erase(it);

				//increase media_files deleted counter
				counter++;
			}
		}
		else
		{
			it++;
		}
	}
}