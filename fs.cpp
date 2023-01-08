#include "fs.h"

bool 
FS::hasPrivilege(int access_rights, int Privilege){
    return ((access_rights&Privilege)/Privilege);
}

string 
FS::getLine(){
    string line = " ", file;

    getline(cin, line);
    while (line != "")
    {
        file += line + "\n";
        getline(cin, line);
    }
    return file;
}

uint8_t* 
FS::readBlock(int block){
    uint8_t* blk = new uint8_t[4096];
    disk.read(block, blk);
    return blk;
}

void 
FS::writeBlock(int block, uint8_t* data){
    disk.write(block, data);
}

dir_entry 
FS::createDirEntry(string file_name, uint32_t size, uint16_t first_blk, uint8_t type, uint8_t access_rights){
    dir_entry dir;
    dir.type = type;
    dir.access_rights = access_rights;
    strcpy(dir.file_name, file_name.c_str());
    dir.first_blk = first_blk;
    dir.size = size;
    return dir;
}

dir_entry 
FS::findInDir(string file_name){
    dir_entry* dirs = (dir_entry*)readBlock(0);
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == file_name){
            return dirs[i];
        }
    }
    return dir_entry();
}

int8_t 
FS::findInDir(uint8_t type){
    dir_entry* dirs = (dir_entry*)readBlock(0);
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == type){
            return i;
        }
    }
    return -1;
}

int8_t 
FS::findFreeSpace(){
    uint16_t* fs = (uint16_t*)readBlock(1);
    for(int i = 2; i < disk.get_no_blocks(); i++){
        if(fs[i] == FAT_FREE){
            return i;
        }
    }
    return -1;
}

int8_t 
FS::findFreeSpace(uint16_t offset){
    uint16_t* fs = (uint16_t*)readBlock(1);
    for(int i = offset+1; i < disk.get_no_blocks(); i++){
        if(fs[i] == FAT_FREE){
            return i;
        }
    }
    return -1;
}

int16_t 
FS::get_no_dir_entries(){
    return (disk.get_disk_size()/disk.get_no_blocks())/sizeof(dir_entry);
}

int16_t 
FS::get_next_block(int block){
    uint16_t* fs = (uint16_t*)readBlock(1);
    int ret = fs[block];
    return ret;
}

FS::FS()
{
    cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    cout << "FS::format()\n";
    // First we have to clean the 0:th block so that any remains
    // of the previous filesystem is deleted.

    dir_entry* blk = new dir_entry[get_no_dir_entries()];
    dir_entry dir = createDirEntry("", 0, -1, 2, 0);
    for(int i = 0; i < get_no_dir_entries(); i++){
        memcpy(&blk[i], &dir, sizeof(dir_entry));
    }
    disk.write(0, (uint8_t*)blk);
    delete[] blk;
    // Now we have to format the 1:th block so that all the fs values
    // are free and 0-1 is EOF according to specification.

    for(int i = 0; i < disk.get_no_blocks(); i++){
        switch (i)
        {
        case 0:
            fat[i] = FAT_EOF;
            break;
        case 1:
            fat[i] = FAT_EOF;
            break;
        
        default:
            fat[i] = FAT_FREE;
            break;
        }
    }
    disk.write(1, (uint8_t*)fat);
    // When we are done, we delete references and return
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(string filepath)
{
    cout << "FS::create(" << filepath << ")\n";
    
    //Handle error for long filename
    if(filepath.length() > 55){
        cout<< "file name too long"<< endl;
        return 1;
    }

    //Get full file content
    string file = getLine();

    //Get free memoryspace on disk
    int fs_index = findFreeSpace();

    //Handle error where disk is full
    int fs_save = fs_index;
    //Update filesystem to include new file
    uint16_t* fs = (uint16_t*)readBlock(1);
    int temp_index = fs_index;
    for(int i = 0; i <= file.length()/4096; i++){
        fs_index = temp_index;
        temp_index = findFreeSpace(fs_index);
        fs[fs_index] = temp_index;
        if(fs_index == -1){
            cout<< "No space left in filesystem" << endl;
            return 1;
        }
        writeBlock(fs_index, (uint8_t*)(char*) file.substr(i*4096, (i+1)*4096).c_str());
        uint16_t* fs = (uint16_t*)readBlock(1);
        writeBlock(1, (uint8_t*)fs);
    }
    fs[fs_index] = FAT_EOF;
    writeBlock(1, (uint8_t*)fs);
    delete[] fs;
    fs_index = fs_save;

    //Find place for dir and make new direntry
    int dir_index = findInDir(2);
    if(dir_index == -1){
        cout<< "No space left in directory" << endl;
        return 1;
    }
    dir_entry dir = createDirEntry(filepath, file.length(), fs_index, 0, 7);

    //write new direntry to dir
    dir_entry* dirs = (dir_entry*)readBlock(0);
    memcpy(&dirs[dir_index], &dir, sizeof(dir_entry));
    writeBlock(0, (uint8_t*)dirs);
    delete[] dirs;
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(string filepath)
{
    cout << "FS::cat(" << filepath << ")\n";

    dir_entry dir = findInDir(filepath);
    cout << dir.first_blk << endl;
    int16_t block = dir.first_blk;
    for(int i = 0; i <= dir.size/4096; i++){
        string content = (char*) readBlock(block);
        content = content.substr(0, std::min((int)dir.size-4096*i, 4096));
        cout << content;
        block = get_next_block(block);
    }
    cout << endl;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    cout << "FS::ls()\n";
    cout << "name\t size\n";
    dir_entry* dirs = (dir_entry*)readBlock(0);
    int dir_index = 0;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type != 2){
            cout<<dirs[i].file_name << "\t " << to_string(dirs[i].size) << endl;
        }
    }
    delete[] dirs;
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(string sourcepath, string destpath)
{
    cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(string sourcepath, string destpath)
{
    cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(string filepath)
{
    cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(string filepath1, string filepath2)
{
    cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(string dirpath)
{
    cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(string dirpath)
{
    cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(string accessrights, string filepath)
{
    cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
