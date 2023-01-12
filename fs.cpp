#include "fs.h"

bool 
FS::hasPrivilege(int access_rights, int Privilege){
    return ((access_rights&Privilege)/Privilege);
}

bool 
FS::dirHasPrivelege(string dir_path, int Privilege){
    if(dir_path == "/"){
        return 1;
    }
    string file_path = dir_path;
    file_path = get_real_dir(file_path);
    file_path = file_path.substr(1); // remove first /
    int index = 0;
    int read_dir = 0;
    dir_entry *dirs = (dir_entry*)readBlock(0);
    while(file_path.length() > 1){
        dirs = (dir_entry*)readBlock(read_dir);
        string temp = file_path.substr(0, file_path.find_first_of('/'));
        file_path = file_path.substr(file_path.find_first_of('/') + 1);
        for(int i = 0 ; i < get_no_dir_entries(); i++){
            if(dirs[i].file_name == temp){
                index = i;
            }
        }
        read_dir = dirs[index].first_blk;
    }
    if(!hasPrivilege(dirs[index].access_rights, WRITE)){
        return 0;
    }
    return 1;
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

string 
FS::getFile(string file_path, string dir_path){
    dir_path = get_real_dir(dir_path);
    string file = "";
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(dir_path));
    int dir_index = -1;
    dir_entry dir;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == file_path){
            dir = dirs[i];
        }
    }
    int16_t block = dir.first_blk;
    for(int i = 0; i <= dir.size/4096; i++){
        string content = (char*) readBlock(block);
        content = content.substr(0, std::min((int)dir.size-4096*i, 4096));
        file += content;
        block = get_next_block(block);
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
   
int8_t
FS::findIndexOfDir(string file_path){
    file_path = get_real_dir(file_path);
    file_path = file_path.substr(1); // remove first /
    int index = 0;
    int read_dir = 0;
    dir_entry *dirs = (dir_entry*)readBlock(0);
    while(file_path.length() > 1){
        dirs = (dir_entry*)readBlock(read_dir);
        string temp = file_path.substr(0, file_path.find_first_of('/'));
        file_path = file_path.substr(file_path.find_first_of('/') + 1);
        for(int i = 0 ; i < get_no_dir_entries(); i++){
            if(dirs[i].file_name == temp){
                index = i;
            }
        }
        read_dir = dirs[index].first_blk;
    }
    return read_dir;
}
 
int8_t 
FS::findInDir(uint8_t type, std::string dir_name){
    dir_name = get_real_dir(dir_name);
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(dir_name));
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == type){
            return i;
        }
    }
    return -1;
}

dir_entry 
FS::findInDir(std::string file_name, string dir_name){
    dir_name = get_real_dir(dir_name);
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(dir_name));
    int dir_index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == file_name){
            return dirs[i];
        }
    }
    return createDirEntry("", 0, 0, 2, 0);
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

string 
FS::get_real_dir(string dir_path){
    if(dir_path.front() != '/') dir_path = '/' + dir_path;
    if(dir_path.back() != '/') dir_path += '/';
    return dir_path;
}

void 
FS::writeDirectory(int16_t block){
    dir_entry *blk = new dir_entry[get_no_dir_entries()];
    dir_entry dir = createDirEntry("", 0, 0, 2, 0);
    for(int i = 0; i < get_no_dir_entries(); i++){
        memcpy(&blk[i], &dir, sizeof(dir_entry));
    }
    disk.write(block, (uint8_t*)blk);
    
    int16_t *fs = (int16_t*)readBlock(1);
    fs[block] = FAT_EOF;
    disk.write(1, (uint8_t*)fs);
}

int
FS::writeFile(string file, int16_t fs_index){
    int16_t *fs = (int16_t*)readBlock(1);
    int temp_index = fs_index;
    for(int i = 0; i <= file.length()/4096; i++){
        fs_index = temp_index;
        temp_index = findFreeSpace(fs_index);
        if(temp_index == -1){
            return -1;
        }
        fs[fs_index] = temp_index;
        writeBlock(fs_index, (uint8_t*)(char*) file.substr(i*4096, (i+1)*4096).c_str());
        writeBlock(1, (uint8_t*)fs);
        uint16_t *fs = (uint16_t*)readBlock(1);
    }
    fs[fs_index] = FAT_EOF;
    writeBlock(1, (uint8_t*)fs);
    delete[] fs;
    return 0;
}

int
FS::writeEntryToDir(dir_entry entry, string dir_path){
    int dir_index = findInDir(2, dir_path);
    if(dir_index == -1) return -1;
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(get_real_dir(dir_path)));
    memcpy(&dirs[dir_index], &entry, sizeof(dir_entry));
    writeBlock(findIndexOfDir(dir_path), (uint8_t*)dirs);

    delete[] dirs;
    return 0;
}

int
FS::writeEntryToDir(dir_entry entry, string dir_path, string old_name){
    dir_path = get_real_dir(dir_path);
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(dir_path));
    int index = -1;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].file_name == old_name){
            index = i;
        }
    }
    memcpy(&dirs[index], &entry, sizeof(dir_entry));
    writeBlock(findIndexOfDir(dir_path), (uint8_t*)dirs);
    return 0;
}

string 
FS::get_dir_from_filepath(string filepath){
    if(filepath == ".."){
        return currentDir.substr(0, currentDir.find_last_of('/') + 1);
    }
    if(filepath.substr(0,2) == "..") filepath = filepath.substr(2);
    while(filepath.find(".") != -1){
        int place = filepath.find_first_of(".");
        for(int i = place-2; i > 0; i--){
            if(filepath.at(i) == '/'){
                filepath = filepath.substr(0, i) + filepath.substr(place + 2);
                break;
            }
        }
    }
    return filepath.substr(0, filepath.find_last_of('/') + 1);
}

string 
FS::get_name_from_filepath(string filepath){
    if(filepath.substr(0,2) == "..") filepath = filepath.substr(2);
    return filepath.substr(filepath.find_last_of('/') + 1);
}


// Pre-made functions

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
    writeDirectory(0);
    int16_t *fs = (int16_t*)readBlock(1);
    for(int i = 0; i < disk.get_no_blocks(); i++)
        fs[i] = FAT_FREE;
    fs[0] = FAT_EOF;
    fs[1] = FAT_EOF;
    disk.write(1, (uint8_t*)fs);
    currentDir = "/";
    delete[] fs;
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(string filepath)
{
    cout << "FS::create(" << filepath << ")\n";
    string dir_name = get_dir_from_filepath(filepath);
    if(dir_name == ""){
        dir_name = currentDir;
    }
    string name = get_name_from_filepath(filepath);
    if(name.length() > 55){
        cout<< "file name too long"<< endl;
        return 1;
    }
    if(findInDir(name, dir_name).type != 2){
        cout << "Name already exists" << endl;
        return 1;
    }
    if(!dirHasPrivelege(dir_name, WRITE)){
        cout << "Permission denied" << endl;
        return 1;
    }
    string file = getLine();
    int fs_index = findFreeSpace();
    if(fs_index == -1) {
        cout << "Disk is full" << endl;
        return 1;
    }
    if(writeFile(file, fs_index) == -1) {
        cout << "Disk is full" << endl;
        return 1;
    }
    if(writeEntryToDir(createDirEntry(name, file.length(), fs_index, 0, READ + WRITE), dir_name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(string filepath)
{
    cout << "FS::cat(" << filepath << ")\n";
    string dir_name = get_dir_from_filepath(filepath);
    if(dir_name == ""){
        dir_name = currentDir;
    }
    string name = get_name_from_filepath(filepath);
    if(!hasPrivilege(findInDir(name, dir_name).access_rights, READ)){
        cout << "Permission Denied" << endl;
        return 1;
    }
    if(findInDir(name, dir_name).type == 1){
        cout << "Cannot read from directory" << endl;
        return 1;
    }
    if(findInDir(name, dir_name).type == 2){
        cout << "File not found" << endl;
        return 1;
    }
    cout << getFile(name, dir_name) << endl;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    if(!dirHasPrivelege(currentDir, READ)){
        cout << "Permission denied" << endl;
        return 1;
    }
    cout << "FS::ls()\n";
    cout << "name\t type\t accessrights\t size\n";
    if(findIndexOfDir(currentDir) != 0)
        cout << "..\t dir\t -\t -\n";
    dir_entry *dirs = (dir_entry*)readBlock(findIndexOfDir(currentDir));
    int dir_index = 0;
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == 1){
            string access = ((hasPrivilege(dirs[i].access_rights, READ)) ? "r" : "-");
            access += ((hasPrivilege(dirs[i].access_rights, WRITE)) ? 'w': '-');
            access += ((hasPrivilege(dirs[i].access_rights, EXECUTE)) ? 'x': '-');
            if(access.substr(0,2) == "--")
                access = access.substr(1);
            if(access.substr(1) == "--")
                access = access.substr(0,2);
            cout<<dirs[i].file_name << "\t dir\t " << access << "\t -" << endl;
        }
    }
    for(int i = 0; i < get_no_dir_entries(); i++){
        if(dirs[i].type == 0){
            string access = ((hasPrivilege(dirs[i].access_rights, READ)) ? "r" : "-");
            access += ((hasPrivilege(dirs[i].access_rights, WRITE)) ? 'w': '-');
            access += ((hasPrivilege(dirs[i].access_rights, EXECUTE)) ? 'x': '-');
            if(access.substr(0,2) == "--")
                access = access.substr(1);
            if(access.substr(1) == "--")
                access = access.substr(0,2);
            cout<<dirs[i].file_name << "\t file\t " << access << "\t " << to_string(dirs[i].size) << endl;
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

    string source_dir_name = get_dir_from_filepath(sourcepath);
    if(source_dir_name == ""){
        source_dir_name = currentDir;
    }
    string source_name = get_name_from_filepath(sourcepath);

    string dest_dir_name = get_dir_from_filepath(destpath);
    if(dest_dir_name == ""){
        dest_dir_name = currentDir;
    }
    string dest_name = get_name_from_filepath(destpath);
    if(dest_name == ""){
        dest_name = source_name;
    }

    if(!hasPrivilege(findInDir(source_name, source_dir_name).access_rights, READ)){
        cout << "Permission Denied" << endl;
        return 1;
    }
    if(!dirHasPrivelege(dest_dir_name, WRITE)){
        cout << "Permission denied" << endl;
        return 1;
    }
    dir_entry source = findInDir(source_name, source_dir_name);
    if(source.type == 1){ // Type is dir
        cout << "The selected file is a directory" << endl;
        return 1;
    }
    if(destpath == findInDir(dest_name, dest_dir_name).file_name){
        cout << dest_dir_name << " : " << dest_name << endl;
        cout << "File already exists" << endl;
        return 1;
    }
    if(findInDir(dest_name, dest_dir_name).type == 1){
        dest_dir_name += dest_name;
        dest_name = source_name;
    }
    int8_t first_block = findFreeSpace();
    if(first_block == -1) {
        cout << "Disk is full" << endl;
        return 1;
    }
    dir_entry dest = createDirEntry(dest_name, source.size, first_block, source.type, source.access_rights);
    if(writeEntryToDir(dest, dest_dir_name) == -1) {
        cout << "Directory is full" << endl;
        return 1;
    }
    string file = getFile(source_name, source_dir_name);
    writeFile(file, first_block);
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(string sourcepath, string destpath)
{
    cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    string source_dir_name = get_dir_from_filepath(sourcepath);
    if(source_dir_name == ""){
        source_dir_name = currentDir;
    }
    string source_name = get_name_from_filepath(sourcepath);

    string dest_dir_name = get_dir_from_filepath(destpath);
    if(dest_dir_name == ""){
        dest_dir_name = currentDir;
    }
    string dest_name = get_name_from_filepath(destpath);
    if(dest_name == ""){
        dest_name = source_name;
    } 
    if(!hasPrivilege(findInDir(source_name, source_dir_name).access_rights, READ)){
        cout << "Permission Denied" << endl;
        return 1;
    }
    if(!dirHasPrivelege(dest_dir_name, WRITE)){
        cout << "Permission denied" << endl;
        return 1;
    }

    dir_entry source = findInDir(source_name, source_dir_name);
    if(source.type == 1){
        cout << "The selected file is a directory" << endl;
        return 1;
    }
    if(findInDir(dest_name, dest_dir_name).type == 1){ // Type is dir
        writeEntryToDir(source, dest_dir_name + dest_name);
        source = createDirEntry("", 0, 0, 2, 0);
        writeEntryToDir(source, source_dir_name, source_name);
        return 0;
    }
    if(dest_name == findInDir(dest_name, dest_dir_name).file_name){
        cout << "File already exists" << endl;
        return 1;
    }
    source = createDirEntry(dest_name, source.size, source.first_blk, source.type, source.access_rights);
    if(writeEntryToDir(source, dest_dir_name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
    source = createDirEntry("", 0, 0, 2, 0);
    if(writeEntryToDir(source, source_dir_name, source_name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(string filepath)
{
    cout << "FS::rm(" << filepath << ")\n";
    string dir_name = get_dir_from_filepath(filepath);
    if(dir_name == ""){
        dir_name = currentDir;
    }
    string name = get_name_from_filepath(filepath);
    if(!dirHasPrivelege(currentDir, WRITE)){
        cout << "Permission denied" << endl;
        return 1;
    }
    dir_entry file = findInDir(dir_name, dir_name);
    if(file.type == 1){ // Type is dir
        int empty = findInDir(0, dir_name + name) + findInDir(1, dir_name + name);
        if(empty != -2){
            cout << "Directory is not empty" << endl;
            return 1;
        }
        int16_t *fs = (int16_t*)readBlock(1);
        fs[file.first_blk] = FAT_FREE;
        writeBlock(1, (uint8_t*)fs);
        if(writeEntryToDir(createDirEntry("", 0, 0, 2, 0), dir_name, name) == -1){
            cout << "Directory is full" << endl;
            return 1;
        }
        return 0;
    }
    if(writeEntryToDir(createDirEntry("", 0, 0, 2, 0), dir_name, name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
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
    
    string source_dir_name = get_dir_from_filepath(filepath1);
    if(source_dir_name == ""){
        source_dir_name = currentDir;
    }
    string source_name = get_name_from_filepath(filepath1);

    string dest_dir_name = get_dir_from_filepath(filepath2);
    if(dest_dir_name == ""){
        dest_dir_name = currentDir;
    }
    string dest_name = get_name_from_filepath(filepath2);

    if(!hasPrivilege(findInDir(source_name, source_dir_name).access_rights, READ)){
        cout << "Permission Denied" << endl;
        return 1;
    }
    if(!hasPrivilege(findInDir(dest_name, dest_dir_name).access_rights, WRITE)){
        cout << "Permission Denied" << endl;
        return 1;
    }
    dir_entry file = findInDir(source_name, source_dir_name);
    dir_entry file1 = findInDir(dest_name, dest_dir_name);
    if(file.type == 1 || file1.type == 1){ // Type is dir
        cout << "One of the files is a directory" << endl;
        return 1;
    }
    if(file.type == 2 || file1.type == 2) {
        cout << "One of the files does not exist" << endl;
        return 1;
    }
    string append_text = getFile(source_name, source_dir_name);
    int16_t *fs = (int16_t*)readBlock(1);
    int16_t file_block = file1.first_blk;
    int file_size = file1.size + file.size;
    int temp_size = file_size;
    while(fs[file_block] != FAT_EOF){
        file_block = fs[file_block];
    }
    int fittable = 4096-(file.size%4096);
    string content = (char*) readBlock(file_block);
    content = content + append_text;
    writeFile(content, file_block);
    dir_entry source = createDirEntry(file1.file_name, temp_size, file1.first_blk, file1.type, file1.access_rights);
    if(writeEntryToDir(source, source_dir_name, file1.file_name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(string dirpath)
{
    cout << "FS::mkdir(" << dirpath << ")\n";
    if(!dirHasPrivelege(currentDir, WRITE)){
        cout << "Permission denied" << endl;
        return 1;
    }
    string dir_name = get_dir_from_filepath(dirpath);
    if(dir_name == "") dir_name = currentDir;
    string name = get_name_from_filepath(dirpath);
    if(name.length() > 55){
        cout<< "file name too long"<< endl;
        return 1;
    }
    if(findInDir(name, dir_name).type != 2){
        cout << "Specified path is already used" << endl;
        return 1;
    }
    int fs_index = findFreeSpace();
    if(fs_index == -1){
        cout << "Disk is full" << endl;
        return 1;
    }
    writeDirectory(fs_index);
    dir_entry dir = createDirEntry(name, 0, fs_index, 1, READ + WRITE);
    if(writeEntryToDir(dir, dir_name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(string dirpath)
{
    cout << "FS::cd(" << dirpath << ")\n";
    string dir_name = get_dir_from_filepath(dirpath);
    string name = get_name_from_filepath(dirpath);
    if(dir_name == "") dir_name = currentDir;
    else if(dir_name == "/"){
        currentDir = dir_name + name;
        return 0;
    }
    if(name == ".."){
        if(dir_name.length() > 1) dir_name = dir_name.substr(0, dir_name.find_last_of("/"));
        if(dir_name == "") dir_name = "/";
        return 0;
    }

    switch (findInDir(name, dir_name).type)
    {
    case 0:
        cout << "Can't cd into a file" << endl;
        return 1;
        break;
    
    case 1:
        if(currentDir.length() > 1){
            currentDir += "/";
        }
        currentDir += name;
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
    string dir_name = get_dir_from_filepath(filepath);
    string name = get_name_from_filepath(filepath);
    dir_entry file = findInDir(name, dir_name);
    file.access_rights = std::stoi(accessrights);
    if(writeEntryToDir(file, dir_name, name) == -1){
        cout << "Directory is full" << endl;
        return 1;
    }
    return 0;
}
