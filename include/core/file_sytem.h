#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H
#include <stddef.h>
bool init_file_system(bool formatonfail = true, const char *basepath = "littlefs", uint8_t maxopenfiles = 4, const char *partitionlable = NULL);
size_t file_system_get_size();
size_t file_system_get_used();
/* file and directory interaction */
char* readfile(const char *path, unsigned int *bytesread);
unsigned int writefile(const char *path, const char *data, unsigned int lenght);
char *list_file(const char *dir_path);
bool remove_file(const char *path);
bool rename_file(const char *pathform, const char *pathto);
bool make_directory(const char *path);
bool remove_dir(const char *path);

#endif // !FILE_SYSTEM_H
