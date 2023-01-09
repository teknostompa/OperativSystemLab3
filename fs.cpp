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

uint8_t
*FS::readBlock(int block){
    uint8_t *blk = new uint8_t[4096];
    disk.read(block, blk);
    return blk;
}

void 
FS::writeBlock(int block, uint8_t *data){
    disk.write(block, data);
}

dir_entry 
FS::createDirEntry(string file_name, uint32_t size, uint16_t first_blk, uint8_t type, uint8_t access_rights){
    dir_entry dir;
    dir.type = type;
    dir.access_rights = access_rights;
    char *name = new char[56];
    strcpy(dir.file_name, file_name.c_str());
    dir.first_blk = first_blk;
    dir.size = size;
    return dir;
}

dir_entry 
FS::findInDir(string file_name){
    dir_entry *dirs = (dir_entry*)readBlock(0);
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == file_name){
            return dirs[i];
        }
    }
    return createDirEntry("", 0, 0, 2, 0);
}
   
int8_t
FS::findIndexOfDir(string file_path){
    file_path = file_path.substr(1); // remove first /
    int index = -1;
    dir_entry *dirs = (dir_entry*)readBlock(0);
    while (file_path.length() > 1)
    {
        string temp_path = file_path.substr(0, file_path.find_first_of('/'));
        index = -1;
        for(int i = 0; i < get_no_dir_entries(); i++){
            if(dirs[i].file_name == temp_path){
                index = i;
            }
        }
        if(index == -1){
            cout << "Faulty path" << endl;
        }
        dir_entry *dirs = (dir_entry*)readBlock(index);
        if(file_path.find_first_of('/') != -1){
            file_path = file_path.substr(file_path.find_first_of('/'));
        }
    }

    return dirs[index].first_blk;
}

int8_t 
FS::findInDir(uint8_t type){
    dir_entry *dirs = (dir_entry*)readBlock(0);
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == type){
            return i;
        }
    }
    return -1;
}

int8_t 
FS::findInDir(uint8_t type, std::string dir_name){
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(dir_name));
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == type){
            return i;
        }
    }
    return -1;
}

int8_t 
FS::findInDir(std::string file_name, string dir_name){
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(dir_name));
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == file_name){
            return i;
        }
    }
    return -1;
}


int16_t 
FS::findFreeSpace(){
    uint16_t *fs = (uint16_t*)readBlock(1);
    for(int i = 0; i < disk.get_no_blocks(); i++){
        if(fs[i] == FAT_FREE){
            return i;
        }
    }
    return -1;
}

int16_t 
FS::findFreeSpace(string dir_name){
    uint16_t *fs = (uint16_t*)readBlock(findIndexOfDir(dir_name));
    for(int i = 0; i < disk.get_no_blocks(); i++){
        if(fs[i] == FAT_FREE){
            return i;
        }
    }
    return -1;
}

int16_t 
FS::findFreeSpace(uint16_t offset){
    uint16_t *fs = (uint16_t*)readBlock(1);
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
    uint16_t *fs = (uint16_t*)readBlock(1);
    return fs[block];
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

    dir_entry *blk = new dir_entry[get_no_dir_entries()];
    dir_entry dir = createDirEntry("", 0, -1, 2, 0);
    for(int i = 0; i < get_no_dir_entries(); i++){
        memcpy(&blk[i], &dir, sizeof(dir_entry));
    }
    disk.write(0, (uint8_t*)blk);
    delete[] blk;
    // Now we have to format the 1:th block so that all the fs values
    // are free and 0-1 is EOF according to specification.
    int16_t *fs = (int16_t*)readBlock(1);
    for(int i = 0; i < disk.get_no_blocks(); i++){
        switch (i)
        {
        case 0:
            fs[i] = FAT_EOF;
            break;
        case 1:
            fs[i] = FAT_EOF;
            break;
        
        default:
            fs[i] = FAT_FREE;
            break;
        }
    }
    disk.write(1, (uint8_t*)fs);
    delete[] fs;
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
    if(fs_index == -1){
        cout << "The disk is full" << endl;
        return 1;
    }

    //Handle error where disk is full
    int fs_save = fs_index;
    //Update filesystem to include new file
    int16_t *fs = (int16_t*)readBlock(1);
    int temp_index = fs_index;
    for(int i = 0; i <= file.length()/4096; i++){
        fs_index = temp_index;
        temp_index = findFreeSpace(fs_index);
        if(temp_index == -1){
            cout << "The disk is full" << endl;
            return 1;
        }
        fs[fs_index] = temp_index;
        if(fs_index == -1){
            cout<< "No space left in filesystem" << endl;
            return 1;
        }
        writeBlock(fs_index, (uint8_t*)(char*) file.substr(i*4096, (i+1)*4096).c_str());
        uint16_t *fs = (uint16_t*)readBlock(1);
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
    dir_entry *dirs = (dir_entry*)readBlock(0);
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

    if(findInDir(filepath).type == 1){
        cout << "Cannot read from directory" << endl;
        return 1;
    }

    dir_entry dir = findInDir(filepath);
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
    cout << "name\t type\t size\n";
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir + "/"));
    int dir_index = 0;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == 1){
            cout<<dirs[i].file_name << "\t dir\t " << "-" << endl;
        }
    }
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == 0){
            cout<<dirs[i].file_name << "\t file\t " << to_string(dirs[i].size) << endl;
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
    dir_entry source = findInDir(sourcepath);
    if(source.type == 1){ // Type is dir
        cout << "The selected file is a directory" << endl;
        return 1;
    }
    if(findInDir(destpath).type == 1){ // Type is dir
        int dir_index = findInDir(2, currentDir + destpath + "/");
        // Find first block
        int8_t first_block = findFreeSpace();
        if(first_block == -1){
            cout << "The disk is full" << endl;
            return 1;
        }

        // Create dir entry for new file 
        dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir + destpath + "/"));
        dir_entry dest = createDirEntry(sourcepath, source.size, first_block, source.type, source.access_rights);
        dirs[dir_index] = dest;
        writeBlock(findIndexOfDir(currentDir + destpath + "/"), (uint8_t*)dirs);
        
        // Copy all blocks
        int16_t source_block = source.first_blk;
        int16_t *fs = (int16_t*)readBlock(1);
        writeBlock(first_block, readBlock(source_block));
        while(fs[source_block] != FAT_EOF){
            fs[first_block] = findFreeSpace(currentDir + "/");
            if(fs[first_block] == -1){
                cout << "The disk is full" << endl;
                return 1;
            }
            first_block = fs[first_block];
            source_block = fs[source_block];
            writeBlock(first_block, readBlock(source_block));
        }
        fs[first_block] = FAT_EOF;
        writeBlock(1, (uint8_t*)fs);
        return 0;
    }
    if(source.access_rights == 0) {
        cout << "The selected file does not exist" << endl;
        return 1;
    }
    if(destpath == findInDir(destpath).file_name){
        cout << "File already exists" << endl;
        return 1;
    }
    int dir_index = findInDir(2);
    // Find first block
    int8_t first_block = findFreeSpace();
    if(first_block == -1){
        cout << "The disk is full" << endl;
        return 1;
    }

    // Create dir entry for new file
    dir_entry *dirs = (dir_entry*)readBlock(0);
    dir_entry dest = createDirEntry(destpath, source.size, first_block, source.type, source.access_rights);
    dirs[dir_index] = dest;
    writeBlock(0, (uint8_t*)dirs);
    
    // Copy all blocks
    int16_t source_block = source.first_blk;
    int16_t *fs = (int16_t*)readBlock(1);
    writeBlock(first_block, readBlock(source_block));
    while(fs[source_block] != FAT_EOF){
        fs[first_block] = findFreeSpace();
        if(fs[first_block] == -1){
            cout << "The disk is full" << endl;
            return 1;
        }
        first_block = fs[first_block];
        source_block = fs[source_block];
        writeBlock(first_block, readBlock(source_block));
    }
    fs[first_block] = FAT_EOF;
    writeBlock(1, (uint8_t*)fs);

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(string sourcepath, string destpath)
{
    cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    dir_entry source = findInDir(sourcepath);
    if(source.type == 1){
        cout << "The selected file is a directory" << endl;
        return 1;
    }
    if(findInDir(destpath).type == 1){ // Type is dir
        dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir + destpath + "/"));
        int dir_index = findInDir(2, currentDir + destpath + "/");
        memcpy(&dirs[dir_index], &source, sizeof(dir_entry));
        writeBlock(findIndexOfDir(currentDir + destpath + "/"), (uint8_t*)dirs);

        dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir + "/"));
        dir_index = findInDir(sourcepath, currentDir + "/");
        source = createDirEntry("", 0, 0, 2, 0);
        memcpy(&dirs[dir_index], &source, sizeof(dir_entry));
        writeBlock(findIndexOfDir(currentDir + "/"), (uint8_t*)dirs);
        return 0;
    }
    if(source.access_rights == 0) {
        cout << "The selected file does not exist" << endl;
        return 1;
    }
    if(destpath == findInDir(destpath).file_name){
        cout << "File already exists" << endl;
        return 1;
    }
    source = createDirEntry(destpath, source.size, source.first_blk, source.type, source.access_rights);
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir + "/"));
    int dir_index = findInDir(sourcepath, currentDir + "/");
    memcpy(&dirs[dir_index], &source, sizeof(dir_entry));
    writeBlock(0, (uint8_t*)dirs);
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(string filepath)
{
    cout << "FS::rm(" << filepath << ")\n";
    dir_entry file = findInDir(filepath);
    if(file.type == 1){ // Type is dir
        int empty = findInDir(0, currentDir + filepath + "/") + findInDir(1, currentDir + filepath + "/");
        if(empty != -2){
            cout << "Directory is not empty" << endl;
            return 1;
        }
        // Remove dir
        int16_t *fs = (int16_t*)readBlock(1);
        fs[file.first_blk] = FAT_FREE;
        writeBlock(1, (uint8_t*)fs);

        dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir + "/"));
        dirs[findInDir(filepath, currentDir)] = createDirEntry("", 0, 0, 2, 0);
        writeBlock(findIndexOfDir(currentDir + "/"), (uint8_t*)dirs);
        return 0;
    }
    if(file.access_rights == 0) {
        cout << "The selected file does not exist" << endl;
        return 1;
    }

    // Remove dir entry
    dir_entry *dirs = (dir_entry*)readBlock(0);
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == filepath){
            dir_index = i;
            break;
        }
    }
    dir_entry dir = createDirEntry("", 0, -1, 2, 0);
    memcpy(&dirs[dir_index], &dir, sizeof(dir_entry));
    writeBlock(0, (uint8_t*)dirs);
    // Mark all fat blocks as free
    int16_t source_block = file.first_blk;
    int16_t *fs = (int16_t*)readBlock(1);
    while(source_block != FAT_EOF) {
        int16_t temp = fs[source_block];
        fs[source_block] = FAT_FREE;
        source_block = temp;
    }
    writeBlock(1, (uint8_t*)fs);

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(string filepath1, string filepath2)
{
    cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    dir_entry file = findInDir(filepath1);
    dir_entry file1 = findInDir(filepath2);
    if(file.type == 1 || file1.type == 1){ // Type is dir
        cout << "One of the files is a directory" << endl;
        return 1;
    }
    if(file.access_rights == 0 || file1.access_rights == 0) {
        cout << "One of the files does not exist" << endl;
        return 1;
    }
    //Get the string to append
    string append_text = "";
    dir_entry dir = findInDir(filepath1);
    int16_t block = dir.first_blk;
    for(int i = 0; i <= dir.size/4096; i++){
        string content = (char*) readBlock(block);
        content = content.substr(0, std::min((int)dir.size-4096*i, 4096));
        append_text += content;
        block = get_next_block(block);
    }
    //
    int16_t *fs = (int16_t*)readBlock(1);
    int16_t file_block = file1.first_blk;
    int file_size = file1.size + file.size;
    int temp_size = file_size;
    while(fs[file_block] != FAT_EOF){
        file_block = fs[file_block];
    }
    int fittable = 4096-(file.size%4096);
    string content = (char*) readBlock(file_block);

    content = content + append_text.substr(0,fittable);
    writeBlock(file_block, (uint8_t*)content.c_str());

    append_text = append_text.substr(0, std::min((int)append_text.length(), 4096));
    file_size -= fittable;
    int16_t next_space;
    while (file_size > 0)
    {
        next_space = findFreeSpace();
        if(next_space == -1){
            cout << "The disk is full" << endl;
            return 1;
        }
        writeBlock(next_space, (uint8_t*)append_text.substr(0,std::min((int)append_text.length(), 4096)).c_str());
        fs[file_block] = next_space;
        file_block = next_space;
        writeBlock(1, (uint8_t*)fs);
        int16_t *fs = (int16_t*)readBlock(1);
        file_size -= std::min((int)append_text.length(), 4096);
    }
    
    fs[file_block] = FAT_EOF;

    dir_entry *dirs = (dir_entry*)readBlock(0);
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == filepath2){
            dir_index = i;
            break;
        }
    }
    dir_entry source = createDirEntry(file1.file_name, temp_size, file1.first_blk, file1.type, file1.access_rights);
    memcpy(&dirs[dir_index], &source, sizeof(dir_entry));
    writeBlock(0, (uint8_t*)dirs);
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(string dirpath)
{
    cout << "FS::mkdir(" << dirpath << ")\n";
    
    //Handle error for long filename
    if(dirpath.length() > 55){
        cout<< "file name too long"<< endl;
        return 1;
    }

    if(findInDir(dirpath, currentDir) != -1){
        cout << "Specified path is already used" << endl;
        return 1;
    }

    //Get free memoryspace on disk
    int fs_index = findFreeSpace();
    if(fs_index == -1){
        cout << "The disk is full" << endl;
        return 1;
    }

    //Update filesystem to include new file
    uint16_t *fs = (uint16_t*)readBlock(1);

    fs[fs_index] = FAT_EOF;
    writeBlock(1, (uint8_t*)fs);
    delete[] fs;

    //Find place for dir and make new direntry
    int dir_index = findInDir(2);
    if(dir_index == -1){
        cout<< "No space left in directory" << endl;
        return 1;
    }
    dir_entry dir = createDirEntry(dirpath, 0, fs_index, 1, 7);

    //write new direntry to dir
    dir_entry *dirs = (dir_entry*)readBlock(0);
    memcpy(&dirs[dir_index], &dir, sizeof(dir_entry));
    writeBlock(0, (uint8_t*)dirs);

    dir_entry *blk = (dir_entry*)readBlock(fs_index);
    dir = createDirEntry("", 0, -1, 2, 0);
    for(int i = 0; i < get_no_dir_entries(); i++){
        memcpy(&blk[i], &dir, sizeof(dir_entry));
    }
    writeBlock(fs_index, (uint8_t*)blk);
    delete[] dirs;
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(string dirpath)
{
    cout << "FS::cd(" << dirpath << ")\n";
    if(dirpath == ".."){
        if(currentDir.length() > 1){
            currentDir = currentDir.substr(0, currentDir.length()-2);
            currentDir = currentDir.substr(0, currentDir.find_last_of("/") + 1);
        }
        return 0;
    }
    switch (findInDir(dirpath).type)
    {
    case 0:
        cout << "Can't cd into a file" << endl;
        return 1;
        break;
    
    case 1:
        if(currentDir.length() > 1){
            currentDir += "/";
        }
        currentDir += dirpath;
        break;

    case 2:
        cout << "Dir not found" << endl;
        return 1;
        break;

    default:
        break;
    }
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    cout << "FS::pwd()\n";
    cout << currentDir << endl;
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
