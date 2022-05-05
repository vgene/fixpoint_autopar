/*
 * A simple program to grep for a pattern in all files in a directory.
 */


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

// find all file names in a directory recursively and put in a list
void find_files(char *dir, char ***list, int *list_size) {
  // go through all files in the directory
  DIR *d = opendir(dir);
  struct dirent *dir_entry;
  while ((dir_entry = readdir(d)) != NULL) {
    // if it is a symlink, ignore
    if (dir_entry->d_type == DT_LNK) {
      continue;
    }

    // if it is a file
    if (dir_entry->d_type == DT_REG) {
      // add it to the list
      char *file_name = malloc(strlen(dir) + strlen(dir_entry->d_name) + 2);
      sprintf(file_name, "%s/%s", dir, dir_entry->d_name);
      *list = realloc(*list, sizeof(char *) * (*list_size + 1));
      (*list)[*list_size] = file_name;
      (*list_size)++;
    }
    // if it is a directory
    else if (dir_entry->d_type == DT_DIR) {
      // if it is not . or ..
      if (strcmp(dir_entry->d_name, ".") != 0 && strcmp(dir_entry->d_name, "..") != 0) {
        // go through all files in the directory
        char *new_dir = malloc(strlen(dir) + strlen(dir_entry->d_name) + 2);
        sprintf(new_dir, "%s/%s", dir, dir_entry->d_name);
        find_files(new_dir, list, list_size);
        free(new_dir);
      }
    }
  }
}

// find a pattern in a file and print to output
void grep(char *file_name, char *pattern) {
  // open the file
  FILE *f = fopen(file_name, "r");
  if (f == NULL) {
    printf("Error opening file %s\n", file_name);
    return;
  }

  // read the file
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  while ((read = getline(&line, &len, f)) != -1) {
    // if the line contains the pattern
    if (strstr(line, pattern) != NULL) {
      // print the file name and the line
      printf("%s: %s", file_name, line);
    }
  }

  // close the file
  fclose(f);
}

int main(int argc, char *argv[])
{
    int i;
    char *pattern;
    char *directory;
    FILE *fp;
    char line[1024];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s pattern directory\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pattern = argv[1];
    directory = argv[2];

    // find all files in the directory
    char **list;
    int list_size = 0;
    list = malloc(sizeof(char *) * 1024);
    find_files(directory, &list, &list_size);

    /*
     * // print all file names
     * for (i = 0; i < list_size; i++) {
     *   // print file name
     *   printf("%s\n", list[i]);
     * }
     */

    // grep all files
    for (i = 0; i < list_size; i++) {
      grep(list[i], pattern);
    }

    return 0;
}
